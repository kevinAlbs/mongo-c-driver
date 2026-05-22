#include <mongoc/mongoc.h>
#include <bson/bson.h>
#include <stdio.h>
#include <stdlib.h>

int
main (void)
{
   mongoc_init ();

   const char *uri_str = getenv ("MONGODB_URI");
   if (!uri_str) {
      uri_str = "mongodb://localhost:27017";
   }

   mongoc_async_client_t *client = mongoc_async_client_new (uri_str);
   if (!client) {
      fprintf (stderr, "Failed to create client for URI: %s\n", uri_str);
      return 1;
   }

   mongoc_async_database_t *db = mongoc_async_client_get_database (client, "admin");

   bson_t *cmd = bson_new ();
   BSON_APPEND_INT32 (cmd, "ping", 1);
   bson_unowned_t cmd_view = {.data = bson_get_data (cmd), .len = cmd->len};

   mongoc_async_future_t *future = mongoc_async_database_run_command (db, cmd_view);
   while (!mongoc_async_future_poll (future)) {
      mongoc_async_future_make_progress (future);
   }

   bson_owned_t *reply = mongoc_async_future_get_bson (future);
   int ok = reply ? 1 : 0;
   if (!reply) {
      fprintf (stderr, "Ping failed\n");
   } else {
      bson_unowned_t rv = bson_owned_as_view (reply);
      bson_t bson;
      bson_init_static (&bson, (const uint8_t *) rv.data, rv.len);
      char *str = bson_as_relaxed_extended_json (&bson, NULL);
      printf ("Ping reply: %s\n", str);
      bson_free (str);
      bson_owned_destroy (reply);
   }

   mongoc_async_future_destroy (future);
   bson_destroy (cmd);
   mongoc_async_database_destroy (db);
   mongoc_async_client_destroy (client);

   mongoc_cleanup ();
   return ok ? 0 : 1;
}
