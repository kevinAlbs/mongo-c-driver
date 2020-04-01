/* Test the behavior of monitoring servers that are constantly changing roles.
 */

#include <mongoc/mongoc.h>
#include <stdio.h>

void
apm_command_started_cb (const mongoc_apm_command_started_t *event)
{
   const char *cmd;

   cmd = mongoc_apm_command_started_get_command_name (event);
   MONGOC_DEBUG ("apm_command_started_cb: %s", cmd);
}
void
apm_command_succeeded_cb (const mongoc_apm_command_succeeded_t *event)
{
   const char *cmd;

   cmd = mongoc_apm_command_succeeded_get_command_name (event);
   MONGOC_DEBUG ("apm_command_succeeded_cb: %s", cmd);
}
void
apm_command_failed_cb (const mongoc_apm_command_failed_t *event)
{
   const char *cmd;

   cmd = mongoc_apm_command_failed_get_command_name (event);
   MONGOC_DEBUG ("apm_command_failed_cb: %s", cmd);
}
void
apm_server_changed_cb (const mongoc_apm_server_changed_t *event)
{
   MONGOC_DEBUG ("apm_server_changed_cb");
}
void
apm_server_opening_cb (const mongoc_apm_server_opening_t *event)
{
   MONGOC_DEBUG ("apm_server_opening_cb");
}
void
apm_server_closed_cb (const mongoc_apm_server_closed_t *event)
{
   MONGOC_DEBUG ("apm_server_closed_cb");
}
void
apm_topology_changed_cb (const mongoc_apm_topology_changed_t *event)
{
   MONGOC_DEBUG ("apm_topology_changed_cb");
}
void
apm_topology_opening_cb (const mongoc_apm_topology_opening_t *event)
{
   MONGOC_DEBUG ("apm_topology_opening_cb");
}
void
apm_topology_closed_cb (const mongoc_apm_topology_closed_t *event)
{
   MONGOC_DEBUG ("apm_topology_closed_cb");
}
void
apm_server_heartbeat_started_cb (
   const mongoc_apm_server_heartbeat_started_t *event)
{
   MONGOC_DEBUG ("apm_server_heartbeat_started_cb");
}
void
apm_server_heartbeat_succeeded_cb (
   const mongoc_apm_server_heartbeat_succeeded_t *event)
{
   MONGOC_DEBUG ("apm_server_heartbeat_succeeded_cb");
}
void
apm_server_heartbeat_failed_cb (
   const mongoc_apm_server_heartbeat_failed_t *event)
{
   MONGOC_DEBUG ("apm_server_heartbeat_failed_cb");
}

static mongoc_apm_callbacks_t *
apm_callbacks_new (void)
{
   mongoc_apm_callbacks_t *callbacks;

   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_started_cb (callbacks, apm_command_started_cb);
   mongoc_apm_set_command_succeeded_cb (callbacks, apm_command_succeeded_cb);
   mongoc_apm_set_command_failed_cb (callbacks, apm_command_failed_cb);
   mongoc_apm_set_server_changed_cb (callbacks, apm_server_changed_cb);
   mongoc_apm_set_server_opening_cb (callbacks, apm_server_opening_cb);
   mongoc_apm_set_server_closed_cb (callbacks, apm_server_closed_cb);
   mongoc_apm_set_topology_changed_cb (callbacks, apm_topology_changed_cb);
   mongoc_apm_set_topology_opening_cb (callbacks, apm_topology_opening_cb);
   mongoc_apm_set_topology_closed_cb (callbacks, apm_topology_closed_cb);
   mongoc_apm_set_server_heartbeat_started_cb (callbacks,
                                               apm_server_heartbeat_started_cb);
   mongoc_apm_set_server_heartbeat_succeeded_cb (
      callbacks, apm_server_heartbeat_succeeded_cb);
   mongoc_apm_set_server_heartbeat_failed_cb (callbacks,
                                              apm_server_heartbeat_failed_cb);

   return callbacks;
}

static void
_fail_insert (mongoc_client_t *client, bool close_connection)
{
   bson_t cmd = BSON_INITIALIZER;
   bson_error_t error;
   bool ret;

   BCON_APPEND (&cmd,
                "configureFailPoint",
                "failCommand",
                "mode",
                "{",
                "times",
                BCON_INT32 (1),
                "}",
                "data",
                "{",
                "failCommands",
                "[",
                "insert",
                "]",
                "errorCode",
                BCON_INT32 (10107),
                "closeConnection",
                BCON_BOOL (close_connection),
                "}");

   ret = mongoc_client_command_simple (
      client, "admin", &cmd, NULL /* read prefs */, NULL /* reply */, &error);
   BSON_ASSERT (ret);
}

int
main (int argc, char **argv)
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   mongoc_uri_t *uri;
   mongoc_apm_callbacks_t *callbacks;
   bson_t doc = BSON_INITIALIZER;
   mongoc_collection_t *collection;
   bson_error_t error;
   bool ret;

   mongoc_init ();

   uri = mongoc_uri_new ("mongodb://localhost:27017,localhost:27018/"
                         "?replicaSet=rs0&retryWrites=true");
   pool = mongoc_client_pool_new (uri);

   /* Set APM callbacks for everything. */
   callbacks = apm_callbacks_new ();
   mongoc_client_pool_set_apm_callbacks (pool, callbacks, NULL /* context */);

   /* Topology scanning begins as soon as the first client is popped. */
   client = mongoc_client_pool_pop (pool);
   collection = mongoc_client_get_collection (client, "test", "test");

   /* Warm it up */
   BCON_APPEND (&doc, "insert", BCON_INT32 (1));
   ret = mongoc_collection_insert_one (collection, &doc, NULL, NULL, &error);
   BSON_ASSERT (ret);

   /* Trigger an insert failure. Since it is a retryable write, this should
    * still succeed.
    * Q: will SDAM immediately rescan, or will this wait 500ms?
    */
   _fail_insert (client, true);
   BCON_APPEND (&doc, "insert", BCON_INT32 (2));
   ret = mongoc_collection_insert_one (collection, &doc, NULL, NULL, &error);
   BSON_ASSERT (ret);


   bson_destroy (&doc);
   mongoc_collection_destroy (collection);
   mongoc_apm_callbacks_destroy (callbacks);
   mongoc_client_pool_push (pool, client);
   mongoc_client_pool_destroy (pool);
   mongoc_uri_destroy (uri);
   mongoc_cleanup ();
}