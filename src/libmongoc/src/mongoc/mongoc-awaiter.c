#include "mongoc-awaiter-private.h"

#include "bson/bson.h"
#include "mongoc-host-list-private.h"
#include "mongoc-topology-description-private.h"
#include "mongoc-set-private.h"
#include "mongoc-server-description-private.h"
#include "mongoc-client-private.h"
#include "mongoc-rpc-private.h"
#include "mongoc-stream-private.h"

/* socket timeout must be longer than the timeout of the await command. */
#define SOCKET_TIMEOUT 60000

typedef struct {
   mongoc_stream_t *stream;
   mongoc_host_list_t host;
   bson_t topology_version;
} mongoc_awaiter_node_t;

struct _mongoc_awaiter_t {
   mongoc_set_t *nodes;
   ismaster_callback_t ismaster_callback;
   uint32_t request_id;
};

static void
_send_cmd (mongoc_stream_t *stream, const bson_t *cmd, uint32_t request_id) {
   mongoc_array_t iov;
   mongoc_rpc_t rpc;
   mongoc_rpc_section_t section; /* only one type 0 section. */
   bson_error_t error;
   bool ret;

   rpc.header.msg_len = 0; 
   rpc.header.request_id = request_id;
   rpc.header.response_to = 0;
   rpc.header.opcode = MONGOC_OPCODE_MSG;
   rpc.msg.flags = 0;
   rpc.msg.n_sections = 1;

   section.payload_type = 0;
   section.payload.bson_document = bson_get_data (cmd);
   rpc.msg.sections[0] = section;

   _mongoc_array_init (&iov, sizeof (mongoc_iovec_t));
   _mongoc_rpc_gather (&rpc, &iov);
   _mongoc_rpc_swab_to_le (&rpc);

   ret = _mongoc_stream_writev_full (stream, iov.data, iov.len, SOCKET_TIMEOUT, &error);

   if (!ret) {
      MONGOC_DEBUG ("error in writev: %s\n", error.message);
   }

   _mongoc_array_clear (&iov);
}

static void
_recv_reply (mongoc_stream_t *stream, bson_t *out) {
   bool ret;
   bson_error_t error;
   mongoc_buffer_t buffer;
   int32_t msg_len;
   mongoc_rpc_t rpc;
   bson_t reply_local;

   _mongoc_buffer_init (&buffer, NULL, 0, NULL, NULL);
   ret = _mongoc_buffer_append_from_stream (&buffer, stream, 4, SOCKET_TIMEOUT, &error);
   if (!ret) {
      MONGOC_DEBUG ("error in append len from stream: %s\n", error.message);
      return;
   }

   BSON_ASSERT (buffer.len == 4);
   memcpy (&msg_len, buffer.data, 4);
   ret = _mongoc_buffer_append_from_stream (&buffer, stream, (size_t) msg_len - 4, SOCKET_TIMEOUT, &error);
   if (!ret) {
      MONGOC_DEBUG ("error in append from stream: %s\n", error.message);
      return;
   }

   ret = _mongoc_rpc_scatter (&rpc, buffer.data, buffer.len);
   if (!ret) {
      MONGOC_DEBUG ("error in scatter: %s\n", error.message);
      return;
   }

   if (BSON_UINT32_FROM_LE (rpc.header.opcode) != MONGOC_OPCODE_MSG) {
      MONGOC_DEBUG ("not opmsg reply");
      return;
   }

   _mongoc_rpc_swab_from_le (&rpc);
   memcpy (&msg_len, rpc.msg.sections[0].payload.bson_document, 4);
   msg_len = BSON_UINT32_FROM_LE (msg_len);
   bson_init_static (&reply_local, rpc.msg.sections[0].payload.bson_document, msg_len);
   bson_copy_to (&reply_local, out);
}

static void
_send_ismaster (mongoc_awaiter_t *awaiter, mongoc_awaiter_node_t *node) {
   bson_t cmd;
   char *ismaster_str;

   /* Update the topology version from the one stored on the topology description. */
   bson_init (&cmd);
   BCON_APPEND (&cmd, "ismaster", BCON_INT32 (1), "$db", "admin");
   if (!bson_empty (&node->topology_version)) {
      BCON_APPEND (&cmd, "topologyVersion", BCON_DOCUMENT (&node->topology_version));
      BCON_APPEND (&cmd, "maxAwaitTimeMS", BCON_INT32(10000));
   }

   ismaster_str = bson_as_json (&cmd, NULL);
   MONGOC_DEBUG ("sending %s to node: %s", ismaster_str, node->host.host_and_port);
   bson_free (ismaster_str);
   _send_cmd (node->stream, &cmd, (awaiter->request_id++));
}

static mongoc_awaiter_node_t * mongoc_awaiter_node_new (mongoc_awaiter_t* awaiter, mongoc_host_list_t *host) {
   mongoc_awaiter_node_t* node;
   bson_error_t error;
   
   node = bson_malloc0 (sizeof (*node));
   memcpy (&node->host, host, sizeof (*host));
   /* Create a new stream. TODO: handle TLS. */
   node->stream = mongoc_client_connect_tcp (SOCKET_TIMEOUT, host, &error);
   if (!node->stream) {
      MONGOC_ERROR ("aw snap - couldn't connect to %s", host->host_and_port);
   }

   bson_init (&node->topology_version);
   _send_ismaster (awaiter, node);

   return node;
}

static void _mongoc_awaiter_node_destroy (void *node_void, void *unused) {
   mongoc_awaiter_node_t *node;

   node = (mongoc_awaiter_node_t *) node_void;
   mongoc_stream_destroy (node->stream);
}

mongoc_awaiter_t *
mongoc_awaiter_new (ismaster_callback_t ismaster_cb)
{
   mongoc_awaiter_t* awaiter;

   awaiter = bson_malloc0 (sizeof (*awaiter));
   awaiter->ismaster_callback = ismaster_cb;
   awaiter->nodes = mongoc_set_new (1, _mongoc_awaiter_node_destroy, NULL);
   return awaiter;
}

void
mongoc_awaiter_reconcile_w_lock (
   mongoc_awaiter_t *awaiter, const mongoc_topology_description_t *description)
{
   mongoc_awaiter_node_t *node;
   uint32_t i;
   mongoc_array_t invalid_ids;
   _mongoc_array_init (&invalid_ids, sizeof (uint32_t));
   
   /* Create new nodes from the topology description that aren't in the node set. */
   for (i = 0; i < description->servers->items_len; i++) {
      mongoc_server_description_t *sd;
      uint32_t id;

      sd = mongoc_set_get_item_and_id (description->servers, i, &id);
      node = mongoc_set_get (awaiter->nodes, id);
      if (!node) {
         MONGOC_DEBUG ("adding node: %s", sd->host.host_and_port);
         node = mongoc_awaiter_node_new (awaiter, &sd->host);
         mongoc_set_add (awaiter->nodes, id, node);
      }

      bson_destroy (&node->topology_version);
      bson_copy_to (&sd->topology_version, &node->topology_version);
   }

   /* Remove all nodes from the node set that are not in the topology description. */
   for (i = 0; i < awaiter->nodes->items_len; i++) {
      uint32_t id;

      mongoc_set_get_item_and_id (awaiter->nodes, i, &id);
      if (!mongoc_set_get (description->servers, id)) {
         _mongoc_array_append_val (&invalid_ids, id);
      }
   }
   for (i = 0; i < invalid_ids.len; i++) {
      uint32_t id;
      
      id = _mongoc_array_index (&invalid_ids, uint32_t, i);
      MONGOC_DEBUG ("removing node by id: %d", id);
      mongoc_set_rm (awaiter->nodes, id);
   }
}

void
mongoc_awaiter_check (mongoc_awaiter_t *awaiter, void* context)
{
   int i;
   bson_t reply;
   mongoc_stream_poll_t *poller = NULL;
   uint32_t n_ready;

   poller = (mongoc_stream_poll_t *) bson_malloc0 (sizeof (*poller) * awaiter->nodes->items_len);

   /* Poll all nodes. */
   for (i = 0; i < awaiter->nodes->items_len; i++) {
      mongoc_awaiter_node_t *node;

      node = (mongoc_awaiter_node_t *) mongoc_set_get_item (awaiter->nodes, i);
      poller[i].stream = node->stream;
      poller[i].events = POLLIN;
      poller[i].revents = 0;
   }

   n_ready = mongoc_stream_poll (poller, awaiter->nodes->items_len, 1 /* poll timeout, can this be 0? */);
   MONGOC_DEBUG ("%d streams are ready", n_ready);
   if (n_ready == 0) {
      return;
   }

   /* Poll all nodes. */
   for (i = 0; i < awaiter->nodes->items_len; i++) {
      mongoc_awaiter_node_t *node;
      bson_error_t error = {0};
      char *reply_str;
      uint32_t id;

      if ( (poller[i].revents & POLLIN) == 0) {
         continue;
      }

      node = (mongoc_awaiter_node_t *) mongoc_set_get_item_and_id(awaiter->nodes, i, &id);
      MONGOC_DEBUG ("node %s replying", node->host.host_and_port);
      
      _recv_reply (node->stream, &reply);
      reply_str = bson_as_json (&reply, NULL);
      MONGOC_DEBUG ("node %d replied: %s", id, reply_str);
      awaiter->ismaster_callback (id, &reply, 1 /* RTT */, context, &error);
      /* TODO:  awaiter->ismaster_callback () */
      
      _send_ismaster (awaiter, node);
   }
}

void
mongoc_awaiter_destroy (mongoc_awaiter_t *awaiter)
{
   mongoc_set_destroy (awaiter->nodes);
   bson_free (awaiter);
}

void
mongoc_awaiter_dump (mongoc_awaiter_t *awaiter)
{
   uint32_t i;

   MONGOC_DEBUG ("Dumping state of awaiter");
   for (i = 0; i < awaiter->nodes->items_len; i++) {
      mongoc_awaiter_node_t *node;
      uint32_t id;

      node = (mongoc_awaiter_node_t *) mongoc_set_get_item_and_id (awaiter->nodes, i, &id);
      MONGOC_DEBUG ("id: %d, host: %s", id, node->host.host_and_port);
   }
}