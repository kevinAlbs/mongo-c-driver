/* gcc example-client.c -o example-client $(pkg-config --cflags --libs
 * libmongoc-1.0) */

/* ./example-client [CONNECTION_STRING [COLLECTION_NAME]] */

#include <mongoc/mongoc.h>
#include <stdio.h>
#include <stdlib.h>

void
command_started (const mongoc_apm_command_started_t *event)
{
   char *s;

   s = bson_as_relaxed_extended_json (
      mongoc_apm_command_started_get_command (event), NULL);
   printf ("Command %s started on %s:\n%s\n\n",
           mongoc_apm_command_started_get_command_name (event),
           mongoc_apm_command_started_get_host (event)->host,
           s);

   bson_free (s);
}


void
command_succeeded (const mongoc_apm_command_succeeded_t *event)
{
   char *s;

   s = bson_as_relaxed_extended_json (
      mongoc_apm_command_succeeded_get_reply (event), NULL);
   printf ("Command %s succeeded:\n%s\n\n",
           mongoc_apm_command_succeeded_get_command_name (event),
           s);

   bson_free (s);
}


void
command_failed (const mongoc_apm_command_failed_t *event)
{
   bson_error_t error;

   mongoc_apm_command_failed_get_error (event, &error);
   printf ("Command %s failed:\n\"%s\"\n\n",
           mongoc_apm_command_failed_get_command_name (event),
           error.message);
}

static void test_exhaust_cursor (mongoc_client_t* client) {
   mongoc_collection_t *coll;
   mongoc_cursor_t *cursor;
   int i;
   const bson_t *out;
   bson_error_t error;
   bson_t filter = BSON_INITIALIZER;
   bson_t *find_opts;
   mongoc_read_prefs_t *rp;

   coll = mongoc_client_get_collection (client, "db", "coll");
   if (!mongoc_collection_delete_many (coll, &filter, NULL, NULL, &error)) {
      MONGOC_ERROR ("delete error: %s", error.message);
      abort ();
   }

   for (i = 0; i < 10; i++) {
      bson_t *to_insert;

      to_insert = BCON_NEW ("x", BCON_INT32(i));
      if (!mongoc_collection_insert_one (coll, to_insert, NULL, NULL, &error)) {
         MONGOC_ERROR ("insert error: %s", error.message);
         abort ();
      }
      bson_destroy (to_insert);
   }

   rp = mongoc_read_prefs_new (MONGOC_READ_SECONDARY_PREFERRED);
   find_opts = BCON_NEW ("exhaust", BCON_BOOL(true), "batchSize", BCON_INT32(2));
   cursor = mongoc_collection_find_with_opts (coll, &filter, find_opts, rp);
   while (mongoc_cursor_next (cursor, &out)) {
      char* str = bson_as_canonical_extended_json (out, NULL);
      MONGOC_INFO ("got doc: %s", str);
      bson_free (str);
   }
   if (mongoc_cursor_error (cursor, &error)) {
      MONGOC_ERROR ("cursor error: %s", error.message);
      abort ();
   }
   
   bson_destroy (find_opts);
   mongoc_collection_destroy (coll);
   mongoc_cursor_destroy (cursor);
   mongoc_read_prefs_destroy (rp);
}

static void test_insert_find (mongoc_client_t *client) {
   mongoc_collection_t *coll;
   mongoc_cursor_t *cursor;
   const bson_t *out;
   bson_error_t error;
   bson_t filter = BSON_INITIALIZER;
   int i;

   coll = mongoc_client_get_collection (client, "db", "coll");
   if (!mongoc_collection_delete_many (coll, &filter, NULL, NULL, &error)) {
      MONGOC_ERROR ("delete error: %s", error.message);
      abort ();
   }

   for (i = 0; i < 10; i++) {
      bson_t *to_insert;

      to_insert = BCON_NEW ("x", BCON_INT32(i));
      MONGOC_INFO ("insert begin");
      if (!mongoc_collection_insert_one (coll, to_insert, NULL, NULL, &error)) {
         MONGOC_ERROR ("insert error: %s", error.message);
         abort ();
      }
      MONGOC_INFO ("insert end");
      bson_destroy (to_insert);
   }

   cursor = mongoc_collection_find_with_opts (coll, &filter, NULL, NULL);
   while (mongoc_cursor_next (cursor, &out)) {
      char* str = bson_as_canonical_extended_json (out, NULL);
      MONGOC_INFO ("got doc: %s", str);
      bson_free (str);
   }
   if (mongoc_cursor_error (cursor, &error)) {
      MONGOC_ERROR ("cursor error: %s", error.message);
      abort ();
   }
   
   mongoc_collection_destroy (coll);
   mongoc_cursor_destroy (cursor);
}

int
main (int argc, char *argv[])
{
   mongoc_client_t *client;
   bson_error_t error;
   const char *collection_name = "test";
   const char *uri_string = "mongodb://127.0.0.1/?appname=client-example";
   mongoc_uri_t *uri;
   mongoc_apm_callbacks_t *callbacks;
   char* testcase = getenv("TESTCASE");

   mongoc_init ();
   if (argc > 1) {
      uri_string = argv[1];
   }

   if (argc > 2) {
      collection_name = argv[2];
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

   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_started_cb (callbacks, command_started);
   mongoc_apm_set_command_succeeded_cb (callbacks, command_succeeded);
   mongoc_apm_set_command_failed_cb (callbacks, command_failed);
   if (getenv ("DEBUG")) {
      mongoc_client_set_apm_callbacks (client, callbacks, NULL);
   }

   mongoc_client_set_error_api (client, 2);

   if (NULL == testcase) {
      testcase = "INSERT_FIND";
   }

   if (0 == strcmp (testcase, "EXHAUST_CURSOR")) {
      test_exhaust_cursor (client);
   }
   if (0 == strcmp (testcase, "INSERT_FIND")) {
      test_insert_find (client);
   }


   mongoc_uri_destroy (uri);
   mongoc_client_destroy (client);
   mongoc_cleanup ();
   mongoc_apm_callbacks_destroy (callbacks);
   return EXIT_SUCCESS;
}
