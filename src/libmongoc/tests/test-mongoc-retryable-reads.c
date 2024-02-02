#include <mongoc/mongoc.h>

#include "mongoc/mongoc-collection-private.h"

#include "json-test.h"
#include "test-libmongoc.h"
#include "mock_server/mock-rs.h"
#include "mock_server/future.h"
#include "mock_server/future-functions.h"
#include "json-test-operations.h"
#include "test-mongoc-retryability-helpers.h"

static bool
retryable_reads_test_run_operation (json_test_ctx_t *ctx,
                                    const bson_t *test,
                                    const bson_t *operation)
{
   bool *explicit_session = (bool *) ctx->config->ctx;
   bson_t reply;
   bson_iter_t iter;
   const char *op_name;
   uint32_t op_len;
   bool res;

   bson_iter_init_find (&iter, operation, "name");
   op_name = bson_iter_utf8 (&iter, &op_len);
   if (strcmp (op_name, "estimatedDocumentCount") == 0 ||
       strcmp (op_name, "count") == 0) {
      /* CDRIVER-3612: mongoc_collection_estimated_document_count does not
       * support explicit sessions */
      *explicit_session = false;
   }
   res = json_test_operation (ctx,
                              test,
                              operation,
                              ctx->collection,
                              *explicit_session ? ctx->sessions[0] : NULL,
                              &reply);

   bson_destroy (&reply);

   return res;
}


/* Callback for JSON tests from Retryable Reads Spec */
static void
test_retryable_reads_cb (bson_t *scenario)
{
   bool explicit_session;
   json_test_config_t config = JSON_TEST_CONFIG_INIT;

   /* use the context pointer to send "explicit_session" to the callback */
   config.ctx = &explicit_session;
   config.run_operation_cb = retryable_reads_test_run_operation;
   config.scenario = scenario;
   config.command_started_events_only = true;
   explicit_session = true;
   run_json_general_test (&config);
   explicit_session = false;
   run_json_general_test (&config);
}


static void
_set_failpoint (mongoc_client_t *client)
{
   bson_error_t error;
   bson_t *cmd =
      tmp_bson ("{'configureFailPoint': 'failCommand',"
                " 'mode': {'times': 1},"
                " 'data': {'errorCode': 10107, 'failCommands': ['count']}}");

   ASSERT (client);

   ASSERT_OR_PRINT (
      mongoc_client_command_simple (client, "admin", cmd, NULL, NULL, &error),
      error);
}
/* Test code paths for all command helpers */
static void
test_cmd_helpers (void *ctx)
{
   mongoc_uri_t *uri;
   mongoc_client_t *client;
   uint32_t server_id;
   mongoc_collection_t *collection;
   bson_t *cmd;
   bson_t reply;
   bson_error_t error;
   bson_iter_t iter;
   mongoc_cursor_t *cursor;
   mongoc_database_t *database;
   const bson_t *doc;

   BSON_UNUSED (ctx);

   uri = test_framework_get_uri ();
   mongoc_uri_set_option_as_bool (uri, MONGOC_URI_RETRYREADS, true);

   client = test_framework_client_new_from_uri (uri, NULL);
   mongoc_client_set_error_api (client, MONGOC_ERROR_API_VERSION_2);
   test_framework_set_ssl_opts (client);
   mongoc_uri_destroy (uri);

   /* clean up in case a previous test aborted */
   server_id = mongoc_topology_select_server_id (
      client->topology, MONGOC_SS_WRITE, NULL, NULL, &error);
   ASSERT_OR_PRINT (server_id, error);
   deactivate_fail_points (client, server_id);

   collection = get_test_collection (client, "retryable_reads");
   database = mongoc_client_get_database (client, "test");

   if (!mongoc_collection_drop (collection, &error)) {
      if (NULL == strstr (error.message, "ns not found")) {
         /* an error besides ns not found */
         ASSERT_OR_PRINT (false, error);
      }
   }

   ASSERT_OR_PRINT (mongoc_collection_insert_one (
                       collection, tmp_bson ("{'_id': 0}"), NULL, NULL, &error),
                    error);
   ASSERT_OR_PRINT (mongoc_collection_insert_one (
                       collection, tmp_bson ("{'_id': 1}"), NULL, NULL, &error),
                    error);

   cmd = tmp_bson ("{'count': '%s'}", collection->collection);

   /* read helpers must retry. */
   _set_failpoint (client);
   ASSERT_OR_PRINT (mongoc_client_read_command_with_opts (
                       client, "test", cmd, NULL, NULL, &reply, &error),
                    error);
   bson_iter_init_find (&iter, &reply, "n");
   ASSERT (bson_iter_as_int64 (&iter) == 2);
   bson_destroy (&reply);

   _set_failpoint (client);
   ASSERT_OR_PRINT (mongoc_database_read_command_with_opts (
                       database, cmd, NULL, NULL, &reply, &error),
                    error);
   bson_iter_init_find (&iter, &reply, "n");
   ASSERT (bson_iter_as_int64 (&iter) == 2);
   bson_destroy (&reply);

   _set_failpoint (client);
   ASSERT_OR_PRINT (mongoc_collection_read_command_with_opts (
                       collection, cmd, NULL, NULL, &reply, &error),
                    error);
   bson_iter_init_find (&iter, &reply, "n");
   ASSERT (bson_iter_as_int64 (&iter) == 2);
   bson_destroy (&reply);

   /* TODO: once CDRIVER-3314 is resolved, test the read+write helpers. */

   /* read/write agnostic command_simple helpers must not retry. */
   _set_failpoint (client);
   ASSERT (
      !mongoc_client_command_simple (client, "test", cmd, NULL, NULL, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_SERVER, 10107, "Failing command");

   _set_failpoint (client);
   ASSERT (!mongoc_database_command_simple (database, cmd, NULL, NULL, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_SERVER, 10107, "Failing command");

   _set_failpoint (client);
   ASSERT (
      !mongoc_collection_command_simple (collection, cmd, NULL, NULL, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_SERVER, 10107, "Failing command");


   /* read/write agnostic command_with_opts helpers must not retry. */
   _set_failpoint (client);
   ASSERT (!mongoc_client_command_with_opts (
      client, "test", cmd, NULL, NULL, NULL, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_SERVER, 10107, "Failing command");

   _set_failpoint (client);
   ASSERT (!mongoc_database_command_with_opts (
      database, cmd, NULL, NULL, NULL, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_SERVER, 10107, "Failing command");

   _set_failpoint (client);
   ASSERT (!mongoc_collection_command_with_opts (
      collection, cmd, NULL, NULL, NULL, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_SERVER, 10107, "Failing command");

   /* read/write agnostic command_simple_with_server_id helper must not retry.
    */
   server_id = mongoc_topology_select_server_id (
      client->topology, MONGOC_SS_WRITE, NULL, NULL, &error);
   ASSERT_OR_PRINT (server_id, error);
   _set_failpoint (client);
   ASSERT (!mongoc_client_command_simple_with_server_id (
      client, "test", cmd, NULL, server_id, NULL, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_SERVER, 10107, "Failing command");


   /* deprecated command helpers (which goes through cursor logic) function must
    * not retry. */
   _set_failpoint (client);
   cursor = mongoc_client_command (
      client, "test", MONGOC_QUERY_NONE, 0, 1, 1, cmd, NULL, NULL);
   ASSERT (!mongoc_cursor_next (cursor, &doc));
   ASSERT (mongoc_cursor_error (cursor, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_SERVER, 10107, "Failing command");
   mongoc_cursor_destroy (cursor);

   _set_failpoint (client);
   cursor = mongoc_database_command (
      database, MONGOC_QUERY_NONE, 0, 1, 1, cmd, NULL, NULL);
   ASSERT (!mongoc_cursor_next (cursor, &doc));
   ASSERT (mongoc_cursor_error (cursor, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_SERVER, 10107, "Failing command");
   mongoc_cursor_destroy (cursor);

   _set_failpoint (client);
   cursor = mongoc_collection_command (
      collection, MONGOC_QUERY_NONE, 0, 1, 1, cmd, NULL, NULL);
   ASSERT (!mongoc_cursor_next (cursor, &doc));
   ASSERT (mongoc_cursor_error (cursor, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_SERVER, 10107, "Failing command");
   mongoc_cursor_destroy (cursor);

   ASSERT_OR_PRINT (mongoc_collection_drop (collection, &error), error);

   deactivate_fail_points (client, server_id);
   mongoc_collection_destroy (collection);
   mongoc_database_destroy (database);
   mongoc_client_destroy (client);
}

static void
test_retry_reads_off (void *ctx)
{
   mongoc_uri_t *uri;
   mongoc_client_t *client;
   mongoc_collection_t *collection;
   uint32_t server_id;
   bson_t *cmd;
   bson_error_t error;
   bool res;

   BSON_UNUSED (ctx);

   uri = test_framework_get_uri ();
   mongoc_uri_set_option_as_bool (uri, "retryreads", false);
   client = test_framework_client_new_from_uri (uri, NULL);
   test_framework_set_ssl_opts (client);

   /* clean up in case a previous test aborted */
   server_id = mongoc_topology_select_server_id (
      client->topology, MONGOC_SS_WRITE, NULL, NULL, &error);
   ASSERT_OR_PRINT (server_id, error);
   deactivate_fail_points (client, server_id);

   collection = get_test_collection (client, "retryable_reads");

   cmd = tmp_bson ("{'configureFailPoint': 'failCommand',"
                   " 'mode': {'times': 1},"
                   " 'data': {'errorCode': 10107, 'failCommands': ['count']}}");
   ASSERT_OR_PRINT (mongoc_client_command_simple_with_server_id (
                       client, "admin", cmd, NULL, server_id, NULL, &error),
                    error);

   cmd = tmp_bson ("{'count': 'coll'}", collection->collection);

   res = mongoc_collection_read_command_with_opts (
      collection, cmd, NULL, NULL, NULL, &error);
   ASSERT (!res);
   ASSERT_CONTAINS (error.message, "failpoint");

   deactivate_fail_points (client, server_id);

   mongoc_collection_destroy (collection);
   mongoc_uri_destroy (uri);
   mongoc_client_destroy (client);
}

typedef struct _test_retry_reads_sharded_on_other_mongos_ctx {
   int count;
   uint16_t ports[2];
} test_retry_reads_sharded_on_other_mongos_ctx;

static void
_test_retry_reads_sharded_on_other_mongos_cb (
   const mongoc_apm_command_failed_t *event)
{
   BSON_ASSERT_PARAM (event);

   test_retry_reads_sharded_on_other_mongos_ctx *const ctx =
      (test_retry_reads_sharded_on_other_mongos_ctx *)
         mongoc_apm_command_failed_get_context (event);
   BSON_ASSERT (ctx);

   ASSERT_WITH_MSG (ctx->count < 2,
                    "expected at most two failpoints to trigger");

   const mongoc_host_list_t *const host =
      mongoc_apm_command_failed_get_host (event);
   BSON_ASSERT (host);
   BSON_ASSERT (!host->next);
   ctx->ports[ctx->count++] = host->port;
}

// Retryable Reads Are Retried on a Different mongos if One is Available
static void
test_retry_reads_sharded_on_other_mongos (void *_ctx)
{
   BSON_UNUSED (_ctx);

   bson_error_t error = {0};

   // This test MUST be executed against a sharded cluster that has at least two
   // mongos instances. Ensure that a test is run against a sharded cluster that
   // has at least two mongoses. If there are more than two mongoses in the
   // cluster, pick two to test against.
   const test_get_two_mongos_clients_result_t clients =
      _test_get_two_mongos_clients ();
   mongoc_client_t *const s0 = clients.s0;
   mongoc_client_t *const s1 = clients.s1;
   BSON_ASSERT (s0 && s1);

   // Deprioritization cannot be deterministically asserted by this test due to
   // randomized selection from suitable servers. Repeat the test a few times to
   // increase the likelihood of detecting incorrect deprioritization behavior.
   for (int i = 0; i < 10; ++i) {
      // Create a client per mongos using the direct connection, and configure
      // the following fail points on each mongos: ...
      //
      // Note: `connectionClosed: false` is deliberately omitted to prevent SDAM
      // error handling behavior from marking server as the Unknown due to a
      // network error, which does not allow it to be a suitable server to be
      // deprioritized during server selection.
      {
         bson_t *const command =
            tmp_bson ("{"
                      "  'configureFailPoint': 'failCommand',"
                      "  'mode': { 'times': 1 },"
                      "  'data': {"
                      "    'failCommands': ['find'],"
                      "    'errorCode': 6"
                      "  }"
                      "}");

         ASSERT_OR_PRINT (mongoc_client_command_simple (
                             s0, "admin", command, NULL, NULL, &error),
                          error);
         ASSERT_OR_PRINT (mongoc_client_command_simple (
                             s1, "admin", command, NULL, NULL, &error),
                          error);
      }

      // Create a client with `retryReads=true` that connects to the cluster,
      // providing the two selected mongoses as seeds.
      mongoc_client_t *client = NULL;
      {
         const char *const host_and_port =
            "mongodb://localhost:27017,localhost:27018/?retryReads=true";
         char *const uri_str =
            test_framework_add_user_password_from_env (host_and_port);
         mongoc_uri_t *const uri = mongoc_uri_new (uri_str);

         client = mongoc_client_new_from_uri_with_error (uri, &error);
         ASSERT_OR_PRINT (client, error);

         mongoc_uri_destroy (uri);
         bson_free (uri_str);
      }
      BSON_ASSERT (client);

      // Enable command monitoring, and execute a `find` command that is
      // supposed to fail on both mongoses.
      {
         test_retry_reads_sharded_on_other_mongos_ctx ctx = {0};

         {
            mongoc_apm_callbacks_t *const callbacks =
               mongoc_apm_callbacks_new ();
            mongoc_apm_set_command_failed_cb (
               callbacks, _test_retry_reads_sharded_on_other_mongos_cb);
            mongoc_client_set_apm_callbacks (client, callbacks, &ctx);
            mongoc_apm_callbacks_destroy (callbacks);
         }

         {
            mongoc_database_t *const db =
               mongoc_client_get_database (client, "db");
            mongoc_collection_t *const coll =
               mongoc_database_get_collection (db, "test");
            mongoc_cursor_t *const cursor = mongoc_collection_find_with_opts (
               coll, tmp_bson ("{}"), NULL, NULL);
            const bson_t *reply = NULL;
            ASSERT_WITH_MSG (!mongoc_cursor_next (cursor, &reply),
                             "expected find command to fail");
            ASSERT_WITH_MSG (mongoc_cursor_error (cursor, &error),
                             "expected find command to fail");
            mongoc_cursor_destroy (cursor);
            mongoc_collection_destroy (coll);
            mongoc_database_destroy (db);
         }

         // Assert that there were failed command events from each mongos.
         ASSERT_WITH_MSG (
            ctx.count == 2,
            "expected exactly 2 failpoints to trigger, but observed %d",
            ctx.count);

         // Note: deprioritization cannot be deterministically asserted by this
         // test due to randomized selection from suitable servers.
         ASSERT_WITH_MSG ((ctx.ports[0] == 27017 || ctx.ports[0] == 27018) &&
                             (ctx.ports[1] == 27017 || ctx.ports[1] == 27018) &&
                             (ctx.ports[0] != ctx.ports[1]),
                          "expected failpoints to trigger once on each mongos, "
                          "but observed failures on %d and %d",
                          ctx.ports[0],
                          ctx.ports[1]);

         mongoc_client_destroy (client);
      }

      // Disable the fail points.
      {
         bson_t *const command =
            tmp_bson ("{"
                      "  'configureFailPoint': 'failCommand',"
                      "  'mode': 'off'"
                      "}");

         ASSERT_OR_PRINT (mongoc_client_command_simple (
                             s0, "admin", command, NULL, NULL, &error),
                          error);
         ASSERT_OR_PRINT (mongoc_client_command_simple (
                             s1, "admin", command, NULL, NULL, &error),
                          error);
      }
   }

   mongoc_client_destroy (s0);
   mongoc_client_destroy (s1);
}

typedef struct _test_retry_reads_sharded_on_same_mongos_ctx {
   int failed_count;
   int succeeded_count;
   uint16_t failed_port;
   uint16_t succeeded_port;
} test_retry_reads_sharded_on_same_mongos_ctx;

static void
_test_retry_reads_sharded_on_same_mongos_cb (
   test_retry_reads_sharded_on_same_mongos_ctx *ctx,
   const mongoc_apm_command_failed_t *failed,
   const mongoc_apm_command_succeeded_t *succeeded)
{
   BSON_ASSERT_PARAM (ctx);
   BSON_ASSERT (failed || true);
   BSON_ASSERT (succeeded || true);

   ASSERT_WITH_MSG (
      ctx->failed_count < 2 && ctx->succeeded_count < 2,
      "expected at most two events, but observed %d failed and %d succeeded",
      ctx->failed_count,
      ctx->succeeded_count);

   if (failed) {
      ctx->failed_count += 1;

      const mongoc_host_list_t *const host =
         mongoc_apm_command_failed_get_host (failed);
      BSON_ASSERT (host);
      BSON_ASSERT (!host->next);
      ctx->failed_port = host->port;
   }

   if (succeeded) {
      ctx->succeeded_count += 1;

      const mongoc_host_list_t *const host =
         mongoc_apm_command_succeeded_get_host (succeeded);
      BSON_ASSERT (host);
      BSON_ASSERT (!host->next);
      ctx->succeeded_port = host->port;
   }
}

static void
_test_retry_reads_sharded_on_same_mongos_failed_cb (
   const mongoc_apm_command_failed_t *event)
{
   _test_retry_reads_sharded_on_same_mongos_cb (
      mongoc_apm_command_failed_get_context (event), event, NULL);
}

static void
_test_retry_reads_sharded_on_same_mongos_succeeded_cb (
   const mongoc_apm_command_succeeded_t *event)
{
   _test_retry_reads_sharded_on_same_mongos_cb (
      mongoc_apm_command_succeeded_get_context (event), NULL, event);
}

// Retryable Reads Are Retried on the Same mongos if No Others are Available
static void
test_retry_reads_sharded_on_same_mongos (void *_ctx)
{
   BSON_UNUSED (_ctx);

   bson_error_t error = {0};

   // Ensure that a test is run against a sharded cluster. If there are multiple
   // mongoses in the cluster, pick one to test against.
   //
   // Note: deliberately requiring *two* servers to ensure server
   // deprioritization actually occurs.
   const test_get_two_mongos_clients_result_t clients =
      _test_get_two_mongos_clients ();
   mongoc_client_t *const s0 = clients.s0;
   mongoc_client_t *const s1 = clients.s1;
   BSON_ASSERT (s0 && s1);

   // Ensure consistent find command results.
   {
      mongoc_database_t *const db = mongoc_client_get_database (s0, "db");
      mongoc_collection_t *const coll =
         mongoc_database_get_collection (db, "test");
      bson_t opts = BSON_INITIALIZER;
      {
         // Ensure drop is observed later.
         mongoc_write_concern_t *const wc = mongoc_write_concern_new ();
         mongoc_write_concern_set_wmajority (wc, 0);
         mongoc_write_concern_append (wc, &opts);
         mongoc_write_concern_destroy (wc);
      }
      ASSERT_OR_PRINT (mongoc_collection_drop_with_opts (coll, &opts, &error),
                       error);
      bson_destroy (&opts);
      mongoc_collection_destroy (coll);
      mongoc_database_destroy (db);
   }

   // Create a client that connects to the mongos using the direct
   // connection, and configure the following fail point on the mongos: ...
   //
   // Note: `connectionClosed: false` is deliberately omitted to prevent SDAM
   // error handling behavior from marking server as the Unknown due to a
   // network error, which does not allow it to be a suitable server to be
   // deprioritized during server selection.
   ASSERT_OR_PRINT (mongoc_client_command_simple (
                       s0,
                       "admin",
                       tmp_bson ("{"
                                 "  'configureFailPoint': 'failCommand',"
                                 "  'mode': { 'times': 1 },"
                                 "  'data': {"
                                 "    'failCommands': ['find'],"
                                 "    'errorCode': 6"
                                 "  }"
                                 "}"),
                       NULL,
                       NULL,
                       &error),
                    error);

   // Create a client with `retryReads=true` that connects to the cluster,
   // providing the selected mongos as the seed.
   mongoc_client_t *client = NULL;
   {
      // Note: deliberately add `directConnection=false` to URI options to
      // prevent initializing the topology as single.
      const char *const host_and_port =
         "mongodb://localhost:27017/?retryReads=true&directConnection=false";
      char *const uri_str =
         test_framework_add_user_password_from_env (host_and_port);
      mongoc_uri_t *const uri = mongoc_uri_new (uri_str);

      client = mongoc_client_new_from_uri_with_error (uri, &error);
      ASSERT_OR_PRINT (client, error);

      // Trigger a connection to update topology.
      ASSERT_OR_PRINT (
         mongoc_client_command_simple (
            client, "admin", tmp_bson ("{'ping': 1}"), NULL, NULL, &error),
         error);

      // Ensure the topology is actually sharded so that server
      // deprioritization code paths are triggered.
      {
         mc_shared_tpld tpld = mc_tpld_take_ref (client->topology);
         ASSERT_WITH_MSG (tpld.ptr->type == MONGOC_TOPOLOGY_SHARDED,
                          "server deprioritization requires topology type "
                          "%d (sharded), but observed %d",
                          MONGOC_TOPOLOGY_SHARDED,
                          tpld.ptr->type);
         mc_tpld_drop_ref (&tpld);
      }

      mongoc_uri_destroy (uri);
      bson_free (uri_str);
   }
   BSON_ASSERT (client);

   // Enable command monitoring, and execute a find command.
   {
      test_retry_reads_sharded_on_same_mongos_ctx ctx = {
         .failed_count = 0,
         .succeeded_count = 0,
      };

      {
         mongoc_apm_callbacks_t *const callbacks = mongoc_apm_callbacks_new ();
         mongoc_apm_set_command_failed_cb (
            callbacks, _test_retry_reads_sharded_on_same_mongos_failed_cb);
         mongoc_apm_set_command_succeeded_cb (
            callbacks, _test_retry_reads_sharded_on_same_mongos_succeeded_cb);
         mongoc_client_set_apm_callbacks (client, callbacks, &ctx);
         mongoc_apm_callbacks_destroy (callbacks);
      }

      {
         mongoc_database_t *const db =
            mongoc_client_get_database (client, "db");
         mongoc_collection_t *const coll =
            mongoc_database_get_collection (db, "test");
         bson_t opts = BSON_INITIALIZER;
         {
            // Ensure drop from earlier is observed.
            mongoc_read_concern_t *const rc = mongoc_read_concern_new ();
            mongoc_read_concern_set_level (rc,
                                           MONGOC_READ_CONCERN_LEVEL_MAJORITY);
            mongoc_read_concern_append (rc, &opts);
            mongoc_read_concern_destroy (rc);
         }
         mongoc_cursor_t *const cursor =
            mongoc_collection_find_with_opts (coll, &opts, NULL, NULL);
         const bson_t *reply = NULL;
         ASSERT_WITH_MSG (
            !mongoc_cursor_next (cursor, &reply),
            "expecting find to succeed with no returned documents");
         ASSERT_WITH_MSG (!mongoc_cursor_error (cursor, &error),
                          "expecting find to succeed with no returned "
                          "documents, but observed error: %s",
                          error.message);
         mongoc_cursor_destroy (cursor);
         bson_destroy (&opts);
         mongoc_collection_destroy (coll);
         mongoc_database_destroy (db);
      }

      // Asserts that there was a failed command and a successful command
      // event.
      ASSERT_WITH_MSG (ctx.failed_count == 1 && ctx.succeeded_count == 1,
                       "expected exactly one failed event and one succeeded "
                       "event, but observed %d failures and %d successes",
                       ctx.failed_count,
                       ctx.succeeded_count);

      // Cannot distinguish "server deprioritization occurred and was
      // reverted due to no other suitable servers" from "only a single
      // suitable server (no deprioritization)" using only observable
      // behavior. This is primarily a regression test. Inspect trace logs or
      // use a debugger to verify correct code paths are triggered.
      ASSERT_WITH_MSG (
         ctx.failed_port == ctx.succeeded_port,
         "expected failed and succeeded events on the same mongos, but "
         "instead observed port %d (failed) and port %d (succeeded)",
         ctx.failed_port,
         ctx.succeeded_port);

      mongoc_client_destroy (client);

      // Disable the fail point.
      ASSERT_OR_PRINT (mongoc_client_command_simple (
                          s0,
                          "admin",
                          tmp_bson ("{"
                                    "  'configureFailPoint': 'failCommand',"
                                    "  'mode': 'off'"
                                    "}"),
                          NULL,
                          NULL,
                          &error),
                       error);
   }

   mongoc_client_destroy (s0);
   mongoc_client_destroy (s1);
}

/*
 *-----------------------------------------------------------------------
 *
 * Runner for the JSON tests for retryable reads.
 *
 *-----------------------------------------------------------------------
 */
static void
test_all_spec_tests (TestSuite *suite)
{
   install_json_test_suite_with_check (suite,
                                       JSON_DIR,
                                       "retryable_reads/legacy",
                                       test_retryable_reads_cb,
                                       TestSuite_CheckLive,
                                       test_framework_skip_if_no_failpoint,
                                       test_framework_skip_if_slow);
}

void
test_retryable_reads_install (TestSuite *suite)
{
   test_all_spec_tests (suite);
   /* Since we need failpoints, require wire version 7 */
   TestSuite_AddFull (suite,
                      "/retryable_reads/cmd_helpers",
                      test_cmd_helpers,
                      NULL,
                      NULL,
                      test_framework_skip_if_max_wire_version_less_than_7,
                      test_framework_skip_if_mongos,
                      test_framework_skip_if_no_failpoint);
   TestSuite_AddFull (suite,
                      "/retryable_reads/retry_off",
                      test_retry_reads_off,
                      NULL,
                      NULL,
                      test_framework_skip_if_max_wire_version_less_than_7,
                      test_framework_skip_if_mongos,
                      test_framework_skip_if_no_failpoint);
   TestSuite_AddFull (suite,
                      "/retryable_reads/sharded/on_other_mongos",
                      test_retry_reads_sharded_on_other_mongos,
                      NULL,
                      NULL,
                      test_framework_skip_if_not_mongos,
                      test_framework_skip_if_no_failpoint,
                      // `retryReads=true` is a 4.2+ feature.
                      test_framework_skip_if_max_wire_version_less_than_8);
   TestSuite_AddFull (suite,
                      "/retryable_reads/sharded/on_same_mongos",
                      test_retry_reads_sharded_on_same_mongos,
                      NULL,
                      NULL,
                      test_framework_skip_if_not_mongos,
                      test_framework_skip_if_no_failpoint,
                      // `retryReads=true` is a 4.2+ feature.
                      test_framework_skip_if_max_wire_version_less_than_8);
}
