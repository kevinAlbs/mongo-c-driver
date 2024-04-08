// example-client-bulkwrite shows use of `mongoc_client_bulkwrite`.
// Example is expected to be run against a MongoDB 8.0+ server.

#include <mongoc/mongoc.h>
#include <stdlib.h> // abort

#define HANDLE_ERROR(...)            \
   if (1) {                          \
      fprintf (stderr, __VA_ARGS__); \
      fprintf (stderr, "\n");        \
      goto fail;                     \
   } else                            \
      (void) 0

#define ASSERT(stmt)                                            \
   if (!stmt) {                                                 \
      fprintf (stderr,                                          \
               "assertion failed on line: %d, statement: %s\n", \
               __LINE__,                                        \
               #stmt);                                          \
      abort ();                                                 \
   } else                                                       \
      (void) 0

int
main (int argc, char *argv[])
{
   mongoc_uri_t *uri = NULL;
   mongoc_client_t *client = NULL;
   bson_error_t error;
   bool ok = false;

   mongoc_init ();

   if (argc > 2) {
      HANDLE_ERROR (
         "Unexpected arguments. Expected usage: %s [CONNECTION_STRING]",
         argv[0]);
   }

   // Construct client.
   {
      const char *uri_string =
         "mongodb://localhost:27017/?appname=example-clientbulkwrite";
      if (argc > 1) {
         uri_string = argv[1];
      }

      uri = mongoc_uri_new_with_error (uri_string, &error);
      if (!uri) {
         HANDLE_ERROR ("Failed to parse URI: %s", error.message);
      }
      client = mongoc_client_new_from_uri_with_error (uri, &error);
      if (!client) {
         HANDLE_ERROR ("Failed to create client: %s", error.message);
      }
   }

   printf ("Insert one document ... begin");
   {
      mongoc_bulkwritev2_t *bw =
         mongoc_client_bulkwritev2_new (client, MONGOC_BULKWRITEOPTIONSV2_NONE);
      bson_t *document = BCON_NEW ("foo", "1");
      mongoc_insertone_modelv2_t model = {.document = document};
      if (!mongoc_client_bulkwritev2_append_insertone (
             bw, "db.coll", -1, model, &error)) {
         HANDLE_ERROR ("error appending insert one: %s", error.message);
      }
      mongoc_bulkwritereturnv2_t *bwr = mongoc_bulkwritev2_execute (bw);
      if (mongoc_bulkwritereturnv2_error (bwr, &error, NULL /* doc */)) {
         HANDLE_ERROR ("error executing bulk write: %s", error.message);
      }
      printf ("Inserted Count: %" PRId64,
              mongoc_bulkwritereturnv2_insertedCount (bwr));
      mongoc_bulkwritereturnv2_destroy (bwr);
      bson_destroy (document);
      mongoc_bulkwritev2_destroy (bw);
   }
   printf ("Insert one document ... end");

   printf ("Insert two documents on different collections ... begin");
   {
      mongoc_bulkwritev2_t *bw =
         mongoc_client_bulkwritev2_new (client, MONGOC_BULKWRITEOPTIONSV2_NONE);
      // Add first document to `db.coll1`
      {
         bson_t *document = BCON_NEW ("foo", "1");
         mongoc_insertone_modelv2_t model = {.document = document};
         if (!mongoc_client_bulkwritev2_append_insertone (
                bw, "db.coll1", -1, model, &error)) {
            HANDLE_ERROR ("error appending insert one: %s", error.message);
         }
         bson_destroy (document);
      }
      // Add second document to `db.coll2`
      {
         bson_t *document = BCON_NEW ("foo", "2");
         mongoc_insertone_modelv2_t model = {.document = document};
         if (!mongoc_client_bulkwritev2_append_insertone (
                bw, "db.coll2", -1, model, &error)) {
            HANDLE_ERROR ("error appending insert one: %s", error.message);
         }
         bson_destroy (document);
      }

      mongoc_bulkwritereturnv2_t *bwr = mongoc_bulkwritev2_execute (bw);
      if (mongoc_bulkwritereturnv2_error (bwr, &error, NULL /* doc */)) {
         HANDLE_ERROR ("error executing bulk write: %s", error.message);
      }
      printf ("Inserted Count: %" PRId64,
              mongoc_bulkwritereturnv2_insertedCount (bwr));
      mongoc_bulkwritereturnv2_destroy (bwr);
      mongoc_bulkwritev2_destroy (bw);
   }
   printf ("Insert two documents on different collections ... end");

   printf ("Do an unordered bulk write ... begin");
   {
      mongoc_bulkwritev2_t *bw = mongoc_client_bulkwritev2_new (
         client,
         (mongoc_bulkwriteoptionsv2_t){.ordered = MONGOC_OPT_BOOLV2_FALSE});
      // Add first document to `db.coll1`
      {
         bson_t *document = BCON_NEW ("foo", "1");
         mongoc_insertone_modelv2_t model = {.document = document};
         if (!mongoc_client_bulkwritev2_append_insertone (
                bw, "db.coll1", -1, model, &error)) {
            HANDLE_ERROR ("error appending insert one: %s", error.message);
         }
         bson_destroy (document);
      }
      // Add second document to `db.coll2`
      {
         bson_t *document = BCON_NEW ("foo", "2");
         mongoc_insertone_modelv2_t model = {.document = document};
         if (!mongoc_client_bulkwritev2_append_insertone (
                bw, "db.coll2", -1, model, &error)) {
            HANDLE_ERROR ("error appending insert one: %s", error.message);
         }
         bson_destroy (document);
      }

      mongoc_bulkwritereturnv2_t *bwr = mongoc_bulkwritev2_execute (bw);
      if (mongoc_bulkwritereturnv2_error (bwr, &error, NULL /* doc */)) {
         HANDLE_ERROR ("error executing bulk write: %s", error.message);
      }
      printf ("Inserted Count: %" PRId64,
              mongoc_bulkwritereturnv2_insertedCount (bwr));
      mongoc_bulkwritereturnv2_destroy (bwr);
      mongoc_bulkwritev2_destroy (bw);
   }
   printf ("Do an unordered bulk write ... end");

   printf ("Get verbose results ... begin");
   {
      mongoc_bulkwritev2_t *bw = mongoc_client_bulkwritev2_new (
         client,
         (mongoc_bulkwriteoptionsv2_t){.verboseResults =
                                          MONGOC_OPT_BOOLV2_TRUE});
      // Add first document to `db.coll1`
      {
         bson_t *document = BCON_NEW ("foo", "1");
         mongoc_insertone_modelv2_t model = {.document = document};
         if (!mongoc_client_bulkwritev2_append_insertone (
                bw, "db.coll1", -1, model, &error)) {
            HANDLE_ERROR ("error appending insert one: %s", error.message);
         }
         bson_destroy (document);
      }
      // Add second document to `db.coll2`
      {
         bson_t *document = BCON_NEW ("foo", "2");
         mongoc_insertone_modelv2_t model = {.document = document};
         if (!mongoc_client_bulkwritev2_append_insertone (
                bw, "db.coll2", -1, model, &error)) {
            HANDLE_ERROR ("error appending insert one: %s", error.message);
         }
         bson_destroy (document);
      }

      mongoc_bulkwritereturnv2_t *bwr = mongoc_bulkwritev2_execute (bw);
      if (mongoc_bulkwritereturnv2_error (bwr, &error, NULL /* doc */)) {
         HANDLE_ERROR ("error executing bulk write: %s", error.message);
      }
      printf ("Inserted Count: %" PRId64,
              mongoc_bulkwritereturnv2_insertedCount (bwr));
      // Print verbose results.
      const bson_t *vr = mongoc_bulkwritereturnv2_verboseResults (bwr);
      char *vr_str = bson_as_relaxed_extended_json (vr, NULL);
      printf ("Verbose results: %s\n", vr_str);
      bson_free (vr_str);
      mongoc_bulkwritereturnv2_destroy (bwr);
      mongoc_bulkwritev2_destroy (bw);
   }
   printf ("Get verbose results ... end");

   ok = true;
fail:
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
   mongoc_cleanup ();
   return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
