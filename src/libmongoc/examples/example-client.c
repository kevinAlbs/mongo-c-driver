/* gcc example-client.c -o example-client $(pkg-config --cflags --libs
 * libmongoc-1.0) */

/* ./example-client [CONNECTION_STRING [COLLECTION_NAME]] */

#include <mongoc/mongoc.h>
#include <stdio.h>
#include <stdlib.h>
#include "mongoc/mongoc-apm-private.h"
static void
started_cb (const mongoc_apm_command_started_t *event)
{
   char *as_str;
   bson_t *new_event;

   new_event = BCON_NEW ("command_started_event",
                         "{",
                         "command",
                         BCON_DOCUMENT (event->command),
                         "command_name",
                         BCON_UTF8 (event->command_name),
                         "database_name",
                         BCON_UTF8 (event->database_name),
                         "operation_id",
                         BCON_INT64 (event->operation_id),
                         "}");
   as_str = bson_as_json (new_event, NULL);
   printf ("%s\n", as_str);
   bson_free (as_str);
   bson_destroy (new_event);
}

int
main (int argc, char *argv[])
{
   mongoc_client_t *client;
   bson_error_t error;
   bson_t* cmd;
   const char *uri_string = "mongodb://127.0.0.1/?appname=client-example";
   mongoc_uri_t *uri;
   mongoc_apm_callbacks_t *callbacks;
   mongoc_read_concern_t *rc;
   bson_t opts;

   mongoc_init ();
   if (argc > 1) {
      uri_string = argv[1];
   }

   uri = mongoc_uri_new_with_error (uri_string, &error);
   if (!uri) {
      fprintf (stderr,
               "failed to parse URI: %s\n"
               "error message:       %s\n",
               uri_string,
               error.message);
      return EXIT_FAILURE;
   }

   client = mongoc_client_new_from_uri (uri);
   if (!client) {
      return EXIT_FAILURE;
   }

   mongoc_client_set_error_api (client, 2);

   rc = mongoc_read_concern_new ();
   mongoc_read_concern_set_level (rc, MONGOC_READ_CONCERN_LEVEL_MAJORITY);
   bson_init (&opts);
   mongoc_read_concern_append (rc, &opts);

   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_started_cb (callbacks, started_cb);
   mongoc_client_set_apm_callbacks (client, callbacks, NULL);
   mongoc_apm_callbacks_destroy (callbacks);


   cmd = BCON_NEW ("ping", BCON_INT32(1));

   if (mongoc_client_command_with_opts (client, "db", cmd, NULL, &opts, NULL, &error)) {
      printf("error=%s\n", error.message);
      abort();
   }

   bson_destroy (&opts);
   mongoc_read_concern_destroy (rc);
   bson_destroy (cmd);
   mongoc_uri_destroy (uri);
   mongoc_client_destroy (client);
   mongoc_cleanup ();

   return EXIT_SUCCESS;
}
