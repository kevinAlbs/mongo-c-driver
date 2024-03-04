/*
Do a simplistic benchmark of an insert-only workload on collection vs. client
bulk write. This may inform C implementation decisions.

Sample output:

Insert many with collection bulk write................done
median :00.40s
min    :00.36s
max    :00.44s
Insert many with client bulk write................done
median :00.43s
min    :00.42s
max    :00.53s
Insert one 100x with collection bulk write................done
median :00.64s
min    :00.45s
max    :00.72s
Insert one 100x with client bulk write................done
median :00.68s
min    :00.53s
max    :00.70s
*/

#include <mongoc.h>

#define MONGOC_COMPILATION 1
#include "bulkWrite-impl.c"

#define FAIL(fmt, ...)                                                         \
   if (1) {                                                                    \
      printf (                                                                 \
         "Failed at line: %d with message :" fmt "\n", __LINE__, __VA_ARGS__); \
      abort ();                                                                \
   } else                                                                      \
      (void) 0

#define MAX_TRIALS 16

int
cmp_int64 (const void *a, const void *b)
{
   const int64_t *ai = a;
   const int64_t *bi = b;
   if (*ai < *bi) {
      return -1;
   }
   if (*ai > *bi) {
      return 1;
   }
   return 0;
}

static void
print_results (int64_t *durations, size_t ntrials)
{
   qsort (durations, ntrials, sizeof (durations[0]), cmp_int64);

   {
      double secs = ((double) durations[ntrials / 2]) / (1000.0 * 1000);
      printf ("median :%05.2fs\n", secs);
   }

   {
      double secs = ((double) durations[0]) / (1000.0 * 1000);
      printf ("min    :%05.2fs\n", secs);
   }

   {
      double secs = ((double) durations[ntrials - 1]) / (1000.0 * 1000);
      printf ("max    :%05.2fs\n", secs);
   }
}

int
main (void)
{
   mongoc_init ();

   mongoc_client_t *client = mongoc_client_new ("mongodb://localhost:27017");

   // Drop test collection.
   {
      mongoc_collection_t *coll =
         mongoc_client_get_collection (client, "db", "coll");
      mongoc_collection_drop (coll, NULL);
      mongoc_collection_destroy (coll);
   }

   printf ("Insert many with collection bulk write");
   {
      mongoc_collection_t *coll =
         mongoc_client_get_collection (client, "db", "coll");
      bson_t doc = BSON_INITIALIZER;
      BSON_APPEND_INT32 (&doc, "x", 123);
      bson_t opts = BSON_INITIALIZER;
      BSON_APPEND_BOOL (&opts, "validate", false); // Disable validation.
      int64_t durations[MAX_TRIALS] = {0};

      for (size_t trial = 0; trial < MAX_TRIALS; trial++) {
         int64_t start = bson_get_monotonic_time ();
         mongoc_bulk_operation_t *bulk =
            mongoc_collection_create_bulk_operation_with_opts (coll, NULL);
         bson_error_t error;
         for (size_t doc_count = 0; doc_count < 100001; doc_count++) {
            bool ok = mongoc_bulk_operation_insert_with_opts (
               bulk, &doc, &opts, &error);
            if (!ok) {
               FAIL ("insert failed: %s", error.message);
            }
         }
         uint32_t server_id =
            mongoc_bulk_operation_execute (bulk, NULL /* reply */, &error);
         if (!server_id) {
            FAIL ("insert failed: %s", error.message);
         }
         mongoc_bulk_operation_destroy (bulk);
         int64_t end = bson_get_monotonic_time ();
         durations[trial] = (end - start);
         printf (".");
         fflush (stdout);
      }
      printf ("done\n");
      print_results (durations, MAX_TRIALS);
      bson_destroy (&opts);
      bson_destroy (&doc);
      mongoc_collection_destroy (coll);
   }

   // Drop test collection.
   {
      mongoc_collection_t *coll =
         mongoc_client_get_collection (client, "db", "coll");
      mongoc_collection_drop (coll, NULL);
      mongoc_collection_destroy (coll);
   }


   printf ("Insert many with client bulk write");
   {
      bson_t doc = BSON_INITIALIZER;
      BSON_APPEND_INT32 (&doc, "x", 123);
      int64_t durations[MAX_TRIALS] = {0};

      for (size_t trial = 0; trial < MAX_TRIALS; trial++) {
         int64_t start = bson_get_monotonic_time ();
         bson_error_t error;
         mongoc_listof_bulkwritemodel_t *models =
            mongoc_listof_bulkwritemodel_new ();
         for (size_t doc_count = 0; doc_count < 100001; doc_count++) {
            if (!mongoc_listof_bulkwritemodel_append_insertone (
                   models, "db.coll", -1, &doc, &error)) {
               FAIL ("appending insert: %s", error.message);
            }
         }
         mongoc_bulkwritereturn_t ret =
            mongoc_client_bulkwrite (client, models, NULL /* opts */);
         if (ret.exc) {
            mongoc_bulkwriteexception_get_error (ret.exc, &error, NULL);
            FAIL ("in bulk write: %s", error.message);
         }
         mongoc_bulkwritereturn_cleanup (&ret);
         mongoc_listof_bulkwritemodel_destroy (models);
         int64_t end = bson_get_monotonic_time ();
         durations[trial] = (end - start);
         printf (".");
         fflush (stdout);
      }
      printf ("done\n");
      print_results (durations, MAX_TRIALS);
      bson_destroy (&doc);
   }

   // Drop test collection.
   {
      mongoc_collection_t *coll =
         mongoc_client_get_collection (client, "db", "coll");
      mongoc_collection_drop (coll, NULL);
      mongoc_collection_destroy (coll);
   }


   printf ("Insert one 100x with collection bulk write");
   {
      mongoc_collection_t *coll =
         mongoc_client_get_collection (client, "db", "coll");
      bson_t doc = BSON_INITIALIZER;
      BSON_APPEND_INT32 (&doc, "x", 123);
      bson_t opts = BSON_INITIALIZER;
      BSON_APPEND_BOOL (&opts, "validate", false); // Disable validation.
      int64_t durations[MAX_TRIALS] = {0};

      for (size_t trial = 0; trial < MAX_TRIALS; trial++) {
         int64_t start = bson_get_monotonic_time ();
         for (size_t doc_count = 0; doc_count < 100; doc_count++) {
            bson_error_t error;
            mongoc_bulk_operation_t *bulk =
               mongoc_collection_create_bulk_operation_with_opts (coll, NULL);
            bool ok = mongoc_bulk_operation_insert_with_opts (
               bulk, &doc, &opts, &error);
            if (!ok) {
               FAIL ("insert failed: %s", error.message);
            }

            uint32_t server_id =
               mongoc_bulk_operation_execute (bulk, NULL /* reply */, &error);
            if (!server_id) {
               FAIL ("insert failed: %s", error.message);
            }
            mongoc_bulk_operation_destroy (bulk);
         }
         int64_t end = bson_get_monotonic_time ();
         durations[trial] = (end - start);
         printf (".");
         fflush (stdout);
      }
      printf ("done\n");
      print_results (durations, MAX_TRIALS);
      bson_destroy (&opts);
      bson_destroy (&doc);
      mongoc_collection_destroy (coll);
   }

   // Drop test collection.
   {
      mongoc_collection_t *coll =
         mongoc_client_get_collection (client, "db", "coll");
      mongoc_collection_drop (coll, NULL);
      mongoc_collection_destroy (coll);
   }


   printf ("Insert one 100x with client bulk write");
   {
      bson_t doc = BSON_INITIALIZER;
      BSON_APPEND_INT32 (&doc, "x", 123);
      int64_t durations[MAX_TRIALS] = {0};

      for (size_t trial = 0; trial < MAX_TRIALS; trial++) {
         int64_t start = bson_get_monotonic_time ();
         for (size_t doc_count = 0; doc_count < 100; doc_count++) {
            bson_error_t error;
            mongoc_listof_bulkwritemodel_t *models =
               mongoc_listof_bulkwritemodel_new ();
            if (!mongoc_listof_bulkwritemodel_append_insertone (
                   models, "db.coll", -1, &doc, &error)) {
               FAIL ("appending insert: %s", error.message);
            }
            mongoc_bulkwritereturn_t ret =
               mongoc_client_bulkwrite (client, models, NULL /* opts */);
            if (ret.exc) {
               mongoc_bulkwriteexception_get_error (ret.exc, &error, NULL);
               FAIL ("in bulk write: %s", error.message);
            }
            mongoc_bulkwritereturn_cleanup (&ret);
            mongoc_listof_bulkwritemodel_destroy (models);
         }
         int64_t end = bson_get_monotonic_time ();
         durations[trial] = (end - start);
         printf (".");
         fflush (stdout);
      }
      printf ("done\n");
      print_results (durations, MAX_TRIALS);
      bson_destroy (&doc);
   }

   mongoc_client_destroy (client);
   mongoc_cleanup ();
}
