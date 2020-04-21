#include "mongoc-topology-private.h"
#include "mongoc-log-private.h"
#include "mongoc-util-private.h"
#include "mongoc-client-private.h"
#include "mongoc-stream-private.h"
#ifdef MONGOC_ENABLE_SSL
#include "mongoc-ssl-private.h"
#endif
#include "mongoc-topology-background-monitor-private.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "bg_monitor"

/* TODO:
 * Clean up mongoc_topology_t
 * move shared things from topology into shared struct
 * move read_only things into readonly struct
 * I _think_ that's everything?
 */

/* TODO: replace these with traces */
#define LOGENTER MONGOC_DEBUG ("%s - enter", BSON_FUNC)
#define LOGEXIT MONGOC_DEBUG ("%s - exit", BSON_FUNC)

typedef struct {
   bson_thread_t thread;
   struct {
      bson_mutex_t mutex;
      mongoc_cond_t cond;
      bool shutting_down;
      bool is_shutdown;
      bool scan_requested;
   } shared;

   uint64_t last_scan_ms; /* time of last scan in milliseconds. */
   uint64_t scan_due_ms;  /* the time of the next scheduled scan. */
   uint32_t server_id;
   uint64_t heartbeat_frequency_ms;
   uint64_t min_heartbeat_frequency_ms;
   int64_t connect_timeout_ms;
   mongoc_stream_t *stream;
   mongoc_topology_t *topology;
   mongoc_host_list_t host;
   int64_t request_id;
   /* TODO: consider wrapping this initial state, and passing it through
    * en-masse, instead of reading from topology and topology scanner. This
    * could be passed to both topology scanner and topology monitor on startup.
    */
   bool use_tls;
#ifdef MONGOC_ENABLE_SSL
   mongoc_ssl_opt_t *ssl_opts;
#endif
   mongoc_uri_t *uri;
   mongoc_apm_callbacks_t apm_callbacks;
   void *apm_context;
   mongoc_stream_initiator_t initiator;
   void *initiator_context;
} mongoc_server_monitor_t;

/* mongoc_background_monitor_t is an extension of mongoc_topology_t. */
typedef struct _mongoc_background_monitor_t {
   mongoc_topology_t *topology;
   mongoc_set_t *server_monitors;
   /* TODO: error queue. */
} mongoc_background_monitor_t;


/* Called only from server monitor thread.
 * Caller must hold no locks
 */
static void
_server_monitor_heartbeat_started (mongoc_server_monitor_t *server_monitor)
{
   if (server_monitor->apm_callbacks.server_heartbeat_started) {
      mongoc_apm_server_heartbeat_started_t event;
      event.host = &server_monitor->host;
      event.context = server_monitor->apm_context;
      server_monitor->apm_callbacks.server_heartbeat_started (&event);
   }
}

/* Called only from server monitor thread.
 * Caller must hold no locks
 */
static void
_server_monitor_heartbeat_succeeded (mongoc_server_monitor_t *server_monitor,
                                     const bson_t *reply,
                                     int64_t duration_usec)
{
   if (server_monitor->apm_callbacks.server_heartbeat_succeeded) {
      mongoc_apm_server_heartbeat_succeeded_t event;
      event.host = &server_monitor->host;
      event.context = server_monitor->apm_context;
      event.reply = reply;
      event.duration_usec = duration_usec;
      server_monitor->apm_callbacks.server_heartbeat_succeeded (&event);
   }
}

/* Called only from server monitor thread.
 * Caller must hold no locks
 */
static void
_server_monitor_heartbeat_failed (mongoc_server_monitor_t *server_monitor,
                                  const bson_error_t *error,
                                  int64_t duration_usec)
{
   if (server_monitor->apm_callbacks.server_heartbeat_failed) {
      mongoc_apm_server_heartbeat_failed_t event;
      event.host = &server_monitor->host;
      event.context = server_monitor->apm_context;
      event.error = error;
      event.duration_usec = duration_usec;
      server_monitor->apm_callbacks.server_heartbeat_failed (&event);
   }
}

static bool
_server_monitor_cmd_send (mongoc_server_monitor_t *server_monitor,
                          bson_t *cmd,
                          bson_t *reply,
                          bson_error_t *error)
{
   mongoc_rpc_t rpc;
   mongoc_array_t array_to_write;
   mongoc_iovec_t *iovec;
   int niovec;
   mongoc_buffer_t buffer;
   uint32_t reply_len;
   bson_t temp_reply;

   rpc.header.msg_len = 0;
   rpc.header.request_id = server_monitor->request_id++;
   rpc.header.response_to = 0;
   rpc.header.opcode = MONGOC_OPCODE_QUERY;
   rpc.query.flags = MONGOC_QUERY_SLAVE_OK;
   rpc.query.collection = "admin.$cmd";
   rpc.query.skip = 0;
   rpc.query.n_return = -1;
   rpc.query.query = bson_get_data (cmd);
   rpc.query.fields = NULL;

   _mongoc_array_init (&array_to_write, sizeof (mongoc_iovec_t));
   _mongoc_rpc_gather (&rpc, &array_to_write);
   iovec = (mongoc_iovec_t *) array_to_write.data;
   niovec = array_to_write.len;
   _mongoc_rpc_swab_to_le (&rpc);

   if (!_mongoc_stream_writev_full (server_monitor->stream,
                                    iovec,
                                    niovec,
                                    server_monitor->connect_timeout_ms,
                                    error)) {
      _mongoc_array_destroy (&array_to_write);
      bson_init (reply);
      return false;
   }
   _mongoc_array_destroy (&array_to_write);

   _mongoc_buffer_init (&buffer, NULL, 0, NULL, NULL);
   if (!_mongoc_buffer_append_from_stream (&buffer,
                                           server_monitor->stream,
                                           4,
                                           server_monitor->connect_timeout_ms,
                                           error)) {
      _mongoc_buffer_destroy (&buffer);
      bson_init (reply);
      return false;
   }

   memcpy (&reply_len, buffer.data, 4);
   reply_len = BSON_UINT32_FROM_LE (reply_len);

   if (!_mongoc_buffer_append_from_stream (&buffer,
                                           server_monitor->stream,
                                           reply_len - buffer.len,
                                           server_monitor->connect_timeout_ms,
                                           error)) {
      _mongoc_buffer_destroy (&buffer);
      bson_init (reply);
      return false;
   }

   if (!_mongoc_rpc_scatter (&rpc, buffer.data, buffer.len)) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Invalid reply from server.");

      _mongoc_buffer_destroy (&buffer);
      bson_init (reply);
      return false;
   }

   if (BSON_UINT32_FROM_LE (rpc.header.opcode) == MONGOC_OPCODE_COMPRESSED) {
      uint8_t *buf = NULL;
      size_t len = BSON_UINT32_FROM_LE (rpc.compressed.uncompressed_size) +
                   sizeof (mongoc_rpc_header_t);

      buf = bson_malloc0 (len);
      if (!_mongoc_rpc_decompress (&rpc, buf, len)) {
         bson_free (buf);
         _mongoc_buffer_destroy (&buffer);
         bson_init (reply);
         bson_set_error (error,
                         MONGOC_ERROR_PROTOCOL,
                         MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                         "Could not decompress server reply");
         return MONGOC_ASYNC_CMD_ERROR;
      }

      _mongoc_buffer_destroy (&buffer);
      _mongoc_buffer_init (&buffer, buf, len, NULL, NULL);
   }

   _mongoc_rpc_swab_from_le (&rpc);

   if (!_mongoc_rpc_get_first_document (&rpc, &temp_reply)) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Invalid reply from server");
      _mongoc_buffer_destroy (&buffer);
      bson_init (reply);
      return false;
   }
   bson_copy_to (&temp_reply, reply);
   _mongoc_buffer_destroy (&buffer);
   return true;
}

static void
_server_monitor_update_topology_description (
   mongoc_server_monitor_t *server_monitor,
   bson_t *reply,
   uint64_t rtt_us,
   bson_error_t *error)
{
   mongoc_topology_t *topology;
   char *temp_str;

   if (!reply) {
      temp_str = bson_strdup_printf ("error: %s", (char *) error->message);
   } else {
      temp_str = bson_as_json (reply, NULL);
   }
   MONGOC_DEBUG ("sm (%d) update topology description: %s",
                 server_monitor->server_id,
                 temp_str);
   bson_free (temp_str);
   topology = server_monitor->topology;
   bson_mutex_lock (&topology->mutex);
   mongoc_topology_description_handle_ismaster (
      &server_monitor->topology->description,
      server_monitor->server_id,
      reply,
      rtt_us / 1000,
      error);
   /* if pooled, wake threads waiting in mongoc_topology_server_by_id */
   mongoc_cond_broadcast (&server_monitor->topology->cond_client);
   /* reconcile server monitors. */
   mongoc_topology_background_monitor_reconcile (topology->background_monitor);
   bson_mutex_unlock (&server_monitor->topology->mutex);
}

/* Called only from server monitor thread.
 * Caller must hold no locks.
 * Locks server_monitor->mutex to reset scan_requested.
 * Locks topology->mutex when updating topology description with new ismaster
 * reply (or error).
 */
static void
_server_monitor_regular_ismaster (mongoc_server_monitor_t *server_monitor)
{
   bson_t cmd;
   bson_t reply;
   bool ret;
   bson_error_t error = {0};
   int64_t rtt_us;
   int64_t start_us;
   int attempts;

   bson_init (&cmd);
   bson_init (&reply);
   rtt_us = 0;
   for (attempts = 0; attempts < 2; attempts++) {
      if (attempts == 1) {
         mongoc_server_description_t *existing_sd;
         bool should_retry;

         /* "Once a server is connected, the client MUST change its type to
          * Unknown only after it has retried the server once. (This rule
          * applies to server checks during monitoring"
          * If the existing server description is not Unknown, retry.
          */
         should_retry = false;

         bson_mutex_lock (&server_monitor->topology->mutex);
         existing_sd = mongoc_topology_description_server_by_id (
            &server_monitor->topology->description,
            server_monitor->server_id,
            NULL);
         if (existing_sd != NULL &&
             existing_sd->type != MONGOC_SERVER_UNKNOWN) {
            should_retry = true;
         }
         /* If the server description is already Unknown, don't retry. */
         bson_mutex_unlock (&server_monitor->topology->mutex);

         if (!should_retry) {
            /* error was previously set. */
            MONGOC_DEBUG ("sm (%d) ismaster failed: %s - not going to retry",
                          server_monitor->server_id,
                          error.message);
            _server_monitor_update_topology_description (
               server_monitor, NULL, -1, &error);
            break;
         } else {
            MONGOC_DEBUG ("sm (%d) ismaster failed - but still not unknown, "
                          "going to retry",
                          server_monitor->server_id);
         }
      }

      bson_reinit (&cmd);
      BCON_APPEND (&cmd, "isMaster", BCON_INT32 (1));

      if (!server_monitor->stream) {
         bson_destroy (&cmd);
         bson_mutex_lock (&server_monitor->topology->mutex);
         bson_copy_to (_mongoc_topology_scanner_get_ismaster (
                          server_monitor->topology->scanner),
                       &cmd);
         bson_mutex_unlock (&server_monitor->topology->mutex);
         /* Using an initiator isn't really necessary. Users can't set them on
          * pools. But it is used for tests. */
         if (server_monitor->initiator) {
            MONGOC_DEBUG ("sm using custom initiator");
            server_monitor->stream =
               server_monitor->initiator (server_monitor->uri,
                                          &server_monitor->host,
                                          server_monitor->initiator_context,
                                          &error);
         } else {
            void *ssl_opts_void = NULL;

#ifdef MONGOC_ENABLE_SSL
            ssl_opts_void = server_monitor->ssl_opts;
#endif
            MONGOC_DEBUG ("sm NOT using custom initiator");
            server_monitor->stream =
               mongoc_client_connect (ssl_opts_void,
                                      server_monitor->uri,
                                      &server_monitor->host,
                                      &error);
         }
         if (!server_monitor->stream) {
            _server_monitor_heartbeat_failed (server_monitor, &error, rtt_us);
            continue;
         }
      }

      /* Cluster time is updated on every reply. Don't wait for notifications,
       * just poll it. */
      bson_mutex_lock (&server_monitor->topology->mutex);
      if (!bson_empty (&server_monitor->topology->description.cluster_time)) {
         bson_append_document (
            &cmd,
            "$clusterTime",
            12,
            &server_monitor->topology->description.cluster_time);
      }
      bson_mutex_unlock (&server_monitor->topology->mutex);

      start_us = bson_get_monotonic_time ();
      bson_destroy (&reply);
      _server_monitor_heartbeat_started (server_monitor);
      ret = _server_monitor_cmd_send (server_monitor, &cmd, &reply, &error);
      /* Invariant:
          - if an app thread requests a scan, the condition variable will be
         woken within minHBMS + time for a scan.
       */
      bson_mutex_lock (&server_monitor->shared.mutex);
      server_monitor->shared.scan_requested = false;
      bson_mutex_unlock (&server_monitor->shared.mutex);
      rtt_us = (bson_get_monotonic_time () - start_us);

      if (ret) {
         _server_monitor_update_topology_description (
            server_monitor, &reply, rtt_us, &error);
         _server_monitor_heartbeat_succeeded (server_monitor, &reply, rtt_us);
         break;
      } else {
         MONGOC_DEBUG ("ismaster failed, closing and null'ing stream");
         mongoc_stream_destroy (server_monitor->stream);
         server_monitor->stream = NULL;
         _server_monitor_heartbeat_failed (server_monitor, &error, rtt_us);
         continue;
      }
   }

   bson_destroy (&cmd);
   bson_destroy (&reply);
}

/* The server monitor thread
 *
 * Runs continuously.
 *
 * Runs an ismaster and sleeps until it is time to scan or woken by a change in
 * shared state:
 * - a request for immediate scan
 * - a request for shutdown
 *
 * Locks and unlocks topology mutex to update description as needed.
 */
static void *
_server_monitor_run (void *server_monitor_void)
{
   mongoc_server_monitor_t *server_monitor;

   server_monitor = (mongoc_server_monitor_t *) server_monitor_void;

   while (true) {
      int64_t now_ms;

      now_ms = bson_get_monotonic_time () / 1000;
      if (now_ms > server_monitor->scan_due_ms) {
         MONGOC_DEBUG ("sm (%d) scan is due", server_monitor->server_id);

         _server_monitor_regular_ismaster (server_monitor);
         server_monitor->last_scan_ms = bson_get_monotonic_time () / 1000;
         server_monitor->scan_due_ms = server_monitor->last_scan_ms +
                                       server_monitor->heartbeat_frequency_ms;
         MONGOC_DEBUG ("sm (%d) last scan: %d",
                       (int) server_monitor->server_id,
                       (int) server_monitor->last_scan_ms);
         MONGOC_DEBUG ("sm (%d) scan due: %d",
                       (int) server_monitor->server_id,
                       (int) server_monitor->scan_due_ms);
      }

      /* Check shared state. */
      bson_mutex_lock (&server_monitor->shared.mutex);
      if (server_monitor->shared.shutting_down) {
         MONGOC_DEBUG ("sm (%d) shutting down", server_monitor->server_id);
         server_monitor->shared.is_shutdown = true;
         bson_mutex_unlock (&server_monitor->shared.mutex);
         break;
      }

      if (server_monitor->shared.scan_requested) {
         MONGOC_DEBUG ("sm (%d) scan requested", server_monitor->server_id);
         MONGOC_DEBUG ("sm (%d) last scan: %d",
                       (int) server_monitor->server_id,
                       (int) server_monitor->last_scan_ms);
         MONGOC_DEBUG ("sm (%d) scan due: %d",
                       (int) server_monitor->server_id,
                       (int) server_monitor->scan_due_ms);
         server_monitor->scan_due_ms =
            server_monitor->last_scan_ms +
            server_monitor->min_heartbeat_frequency_ms;
      }

      MONGOC_DEBUG (
         "sm (%d) sleeping for %d",
         server_monitor->server_id,
         (int) (server_monitor->scan_due_ms - server_monitor->last_scan_ms));
      mongoc_cond_timedwait (&server_monitor->shared.cond,
                             &server_monitor->shared.mutex,
                             server_monitor->scan_due_ms -
                                server_monitor->last_scan_ms);
      MONGOC_DEBUG ("sm (%d) woken up", server_monitor->server_id);
      bson_mutex_unlock (&server_monitor->shared.mutex);
   }
   return NULL;
}

/* Free data for a server monitor.
 * Called from the thread removing the server monitor.
 * Caller must hold topology mutex, but not hold server monitor mutex.
 */
static void
_server_monitor_destroy (mongoc_server_monitor_t *server_monitor)
{
   mongoc_stream_destroy (server_monitor->stream);
   mongoc_uri_destroy (server_monitor->uri);
   mongoc_cond_destroy (&server_monitor->shared.cond);
   bson_mutex_destroy (&server_monitor->shared.mutex);
#ifdef MONGOC_ENABLE_SSL
   if (server_monitor->ssl_opts) {
      _mongoc_ssl_opts_cleanup (server_monitor->ssl_opts, true);
      bson_free (server_monitor->ssl_opts);
   }
#endif
   bson_free (server_monitor);
}

/* Caller must hold topology lock.
 * Called from any thread updating the topology.
 * Called during reconcile.
 * If the thread is completely stopped, then joins the thread, destroys the
 * server monitor, and returns true.
 */
static bool
_server_monitor_try_shutdown_and_destroy (
   mongoc_server_monitor_t *server_monitor)
{
   bool is_shutdown;

   LOGENTER;
   MONGOC_DEBUG ("bg shutting down sm %d", server_monitor->server_id);
   bson_mutex_lock (&server_monitor->shared.mutex);
   is_shutdown = server_monitor->shared.is_shutdown;
   server_monitor->shared.shutting_down = true;
   mongoc_cond_signal (&server_monitor->shared.cond);
   bson_mutex_unlock (&server_monitor->shared.mutex);

   /* Only join once the server monitor thread has exited. Otherwise,
    * the server monitor may be in the middle of scanning (and therefore
    * may need to take the topology mutex again).
    *
    * Since the topology mutex is locked, we're guaranteed that only one thread
    * will join.
    */
   if (is_shutdown) {
      bson_thread_join (server_monitor->thread);
      LOGEXIT;
      _server_monitor_destroy (server_monitor);
      return true;
   }
   LOGEXIT;
   return false; /* Still waiting for shutdown. */
}

/* Only called by one application thread that is responsible for completing
 * shutdown. Caller must not have topology mutex locked. Locks server monitor
 * mutex.
 */
static void
_server_monitor_wait_for_shutdown_and_destroy (
   mongoc_server_monitor_t *server_monitor)
{
   LOGENTER;
   MONGOC_DEBUG ("bg shutting down and waiting sm %d",
                 server_monitor->server_id);
   bson_mutex_lock (&server_monitor->shared.mutex);
   server_monitor->shared.shutting_down = true;
   mongoc_cond_signal (&server_monitor->shared.cond);
   bson_mutex_unlock (&server_monitor->shared.mutex);

   /* Wait for the thread to shutdown. */
   bson_thread_join (server_monitor->thread);
   _server_monitor_destroy (server_monitor);
   LOGEXIT;
}

/* Called only from background monitor thread.
 * Caller may have topology mutex locked, but does not matter.
 * Locks server monitor mutex.
 */
static void
_server_monitor_request_scan (mongoc_server_monitor_t *server_monitor)
{
   LOGENTER;
   bson_mutex_lock (&server_monitor->shared.mutex);
   server_monitor->shared.scan_requested = true;
   mongoc_cond_signal (&server_monitor->shared.cond);
   bson_mutex_unlock (&server_monitor->shared.mutex);
   LOGEXIT;
}

/* Called only from background monitor thread.
 * Caller must have topology mutex locked.
 * Locks server monitor mutex when requesting a scan.
 */
static void
_background_monitor_reconcile_server_monitor (
   mongoc_background_monitor_t *background_monitor,
   mongoc_server_description_t *sd)
{
   mongoc_set_t *server_monitors;
   mongoc_server_monitor_t *server_monitor;
   mongoc_topology_t *topology;

   LOGENTER;

   topology = background_monitor->topology;
   server_monitors = background_monitor->server_monitors;
   server_monitor = mongoc_set_get (server_monitors, sd->id);

   if (!server_monitor) {
      MONGOC_DEBUG ("bg adding server monitor for %d : %s",
                    sd->id,
                    (char *) sd->host.host_and_port);
      server_monitor = bson_malloc0 (sizeof (*server_monitor));
      server_monitor->server_id = sd->id;
      memcpy (&server_monitor->host, &sd->host, sizeof (mongoc_host_list_t));
      server_monitor->topology = topology;
      server_monitor->heartbeat_frequency_ms =
         topology->description.heartbeat_msec;
      server_monitor->min_heartbeat_frequency_ms =
         topology->min_heartbeat_frequency_msec;
      server_monitor->connect_timeout_ms = topology->connect_timeout_msec;
      server_monitor->uri = mongoc_uri_copy (topology->uri);
/* TODO: don't rely on topology scanner to get ssl opts */
#ifdef MONGOC_ENABLE_SSL
      if (topology->scanner->ssl_opts) {
         server_monitor->ssl_opts = bson_malloc0 (sizeof (mongoc_ssl_opt_t));

         MONGOC_DEBUG ("ssl_opts are being copied");
         _mongoc_ssl_opts_copy_to (
            topology->scanner->ssl_opts, server_monitor->ssl_opts, true);
      }
#endif
      memcpy (&server_monitor->apm_callbacks,
              &topology->description.apm_callbacks,
              sizeof (mongoc_apm_callbacks_t));
      server_monitor->apm_context = topology->description.apm_context;
      server_monitor->initiator = topology->scanner->initiator;
      server_monitor->initiator_context = topology->scanner->initiator_context;
      mongoc_cond_init (&server_monitor->shared.cond);
      bson_mutex_init (&server_monitor->shared.mutex);
      bson_thread_create (
         &server_monitor->thread, _server_monitor_run, server_monitor);
      mongoc_set_add (
         server_monitors, server_monitor->server_id, server_monitor);
   }

   LOGEXIT;
}

/* Called from application threads
 * Caller must hold topology lock.
 * Locks topology description mutex to copy out server description errors.
 */
void
mongoc_topology_background_monitor_collect_errors (
   mongoc_background_monitor_t *background_monitor, bson_error_t *error_out)
{
   mongoc_topology_t *topology;
   mongoc_topology_description_t *topology_description;
   mongoc_server_description_t *server_description;
   bson_string_t *error_message;
   int i;

   LOGENTER;

   topology = background_monitor->topology;
   topology_description = &topology->description;
   memset (error_out, 0, sizeof (bson_error_t));
   error_message = bson_string_new ("");

   for (i = 0; i < topology_description->servers->items_len; i++) {
      bson_error_t *error;

      server_description = topology_description->servers->items[i].item;
      error = &server_description->error;
      if (error->code) {
         if (error_message->len > 0) {
            bson_string_append_c (error_message, ' ');
         }
         bson_string_append_printf (
            error_message, "[%s]", server_description->error.message);
         /* The last error's code and domain wins. */
         error_out->code = error->code;
         error_out->domain = error->domain;
      }
   }

   bson_strncpy ((char *) &error_out->message,
                 error_message->str,
                 sizeof (error_out->message));
   bson_string_free (error_message, true);

   LOGEXIT;
}


/*
Consider this.
A mutex protects shared state.
A condition variable lets a thread wait for changes on that shared state.

topology->mutex protects the topology description, _and_ the monitor thread
state.
server_monitor->mutex protects the server monitor state and topology version.

As long as we lock and unlock in order TM -> SM. We're kosher. I think this is
the case because a server monitor won't need to lock SM when updating the
topology. Only when it is checking if it has changed state.

Next up:
[done] 1. copy the topology description, so we do not need to take a TM -> SM
sequence.
[not going to do] 2. separate the topology mutex from the monitoring mutex.
Figure out how to handle topology vs background monitor.
[done]- Document
[done]- Error queue.
   - We store errors in the server description. Let's not do anything funky
quite yet.
[done]- Revisit the callback to updating the topology description.
[done]- Fix leaks
- Get all tests passing.

*/

/* For each function
- what thread can run it?
   topology monitor thread
   server monitor thread
   application thread
- what locks should caller hold
- what locks does it take
*/


/* Reconcile the topology description with the set of server monitors.
 *
 * Called when the topology description is updated (via handshake, monitoring,
 * or invalidation). May be called by server monitor thread or an application
 * thread. Caller must have topology mutex locked. Locks server monitor mutexes.
 * May join / remove server monitors that have completed shutdown.
 */
void
mongoc_topology_background_monitor_reconcile (
   mongoc_background_monitor_t *background_monitor)
{
   mongoc_topology_t *topology;
   mongoc_topology_description_t *td;
   mongoc_set_t *server_descriptions;
   mongoc_set_t *server_monitors;
   uint32_t *server_monitor_ids_to_remove;
   uint32_t n_server_monitor_ids_to_remove = 0;
   int i;

   topology = background_monitor->topology;
   td = &topology->description;
   server_descriptions = td->servers;
   server_monitors = background_monitor->server_monitors;

   if (topology->scanner_state != MONGOC_TOPOLOGY_SCANNER_BG_RUNNING) {
      MONGOC_DEBUG (
         "topology is in the middle of shutting down, do not reconcile");
      LOGEXIT;
      return;
   }

   /* Add newly discovered server monitors, and update existing ones. */
   for (i = 0; i < server_descriptions->items_len; i++) {
      mongoc_server_description_t *sd;

      sd = mongoc_set_get_item (server_descriptions, i);
      _background_monitor_reconcile_server_monitor (background_monitor, sd);
   }

   /* Signal shutdown to server monitors no longer in the topology description.
    */
   server_monitor_ids_to_remove =
      bson_malloc0 (sizeof (uint32_t) * server_monitors->items_len);
   for (i = 0; i < server_monitors->items_len; i++) {
      mongoc_server_monitor_t *server_monitor;
      uint32_t id;

      server_monitor = mongoc_set_get_item_and_id (server_monitors, i, &id);
      if (!mongoc_set_get (server_descriptions, id)) {
         if (_server_monitor_try_shutdown_and_destroy (server_monitor)) {
            server_monitor_ids_to_remove[n_server_monitor_ids_to_remove] = id;
            n_server_monitor_ids_to_remove++;
         }
      }
   }

   /* Remove freed server monitors that have completed shutdown. */
   for (i = 0; i < n_server_monitor_ids_to_remove; i++) {
      mongoc_set_rm (server_monitors, server_monitor_ids_to_remove[i]);
   }
   bson_free (server_monitor_ids_to_remove);
}

/* Request all server monitors to scan.
 * Caller must have topology mutex locked.
 * Only called from application threads (during server selection or "not master"
 * errors). Locks server monitor mutexes to deliver scan_requested.
 */
void
mongoc_topology_background_monitor_request_scan (
   mongoc_background_monitor_t *background_monitor)
{
   mongoc_set_t *server_monitors;
   int i;

   LOGENTER;

   server_monitors = background_monitor->server_monitors;

   for (i = 0; i < server_monitors->items_len; i++) {
      mongoc_server_monitor_t *server_monitor;
      uint32_t id;

      server_monitor = mongoc_set_get_item_and_id (server_monitors, i, &id);
      _server_monitor_request_scan (server_monitor);
   }

   LOGEXIT;
}

/*
 * Robust against being called by multiple threads. But in practice, only
 * expected to be called by one application thread only (because
 * mongoc_client_pool_destroy is not thread-safe). Caller must have topology
 * mutex locked. Locks server monitor mutexes to deliver shutdown. Releases
 * topology mutex to join server monitor threads. Leaves topology mutex locked
 * on exit.
 */
void
mongoc_topology_background_monitor_shutdown (
   struct _mongoc_background_monitor_t *background_monitor)
{
   mongoc_topology_t *topology;
   int i;

   LOGENTER;

   topology = background_monitor->topology;

   if (topology->scanner_state == MONGOC_TOPOLOGY_SCANNER_BG_RUNNING) {
      /* if background threads are running, request a shutdown and signal all
       * server monitors to start shutting down. */
      topology->scanner_state = MONGOC_TOPOLOGY_SCANNER_SHUTTING_DOWN;
      /* TODO: for a faster shutdown, signal shutdown to all servers. */

   } else {
      /* nothing to do if it's already off */
      bson_mutex_unlock (&topology->mutex);
      return;
   }

   bson_mutex_unlock (&topology->mutex);

   /* if we're joining the thread, wait for it to come back and broadcast
    * all listeners */
   for (i = 0; i < background_monitor->server_monitors->items_len; i++) {
      _server_monitor_wait_for_shutdown_and_destroy (
         mongoc_set_get_item (background_monitor->server_monitors, i));
   }
   mongoc_set_destroy (background_monitor->server_monitors);
   /* We have tests that the background scanner is start / stop-able. Why? I'm
    * not sure. But for now, it's easy to  keep that. */
   background_monitor->server_monitors = mongoc_set_new (1, NULL, NULL);

   bson_mutex_lock (&topology->mutex);
   topology->scanner_state = MONGOC_TOPOLOGY_SCANNER_OFF;
   bson_mutex_unlock (&topology->mutex);

   mongoc_cond_broadcast (&topology->cond_client);

   LOGEXIT;
}

mongoc_background_monitor_t *
mongoc_topology_background_monitor_new (mongoc_topology_t *topology)
{
   mongoc_background_monitor_t *background_monitor;

   background_monitor = bson_malloc0 (sizeof (mongoc_background_monitor_t));
   background_monitor->topology = topology;
   background_monitor->server_monitors = mongoc_set_new (1, NULL, NULL);
   return background_monitor;
}

/* Called after shutdown. */
void
mongoc_topology_background_monitor_destroy (
   mongoc_background_monitor_t *background_monitor)
{
   if (!background_monitor) {
      return;
   }
   BSON_ASSERT (background_monitor->topology->scanner_state ==
                MONGOC_TOPOLOGY_SCANNER_OFF);
   mongoc_set_destroy (background_monitor->server_monitors);
   bson_free (background_monitor);
}