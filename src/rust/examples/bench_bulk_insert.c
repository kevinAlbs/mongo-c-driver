/**
 * bench_bulk_insert.c — bulk-insert performance: async Rust driver vs. sync C driver.
 *
 * Inserts N_DOCS documents in a single bulk write using each driver and reports
 * the time spent in two phases:
 *
 *   build  — constructing the operation (BSON args doc for Rust; append_insertone
 *             loop for the C driver)
 *   execute — the network round-trip and server work
 *
 * The build phase reveals the extra marshaling cost of the Rust driver: the
 * entire models array must be serialized into one BSON document before the call,
 * whereas the C driver accumulates operations incrementally.
 *
 * Build:
 *   cmake --build cmake-build --target mongoc-async-bench-bulk-insert
 *
 * Run (requires mongod on localhost:27017):
 *   ./cmake-build/src/rust/examples/mongoc-async-bench-bulk-insert [N_DOCS]
 */

/* mongoc.h must precede mongoc-rust-private.h: mongoc-rust-private.h
 * includes bson/bson.h, and libbson requires the umbrella header to
 * be entered first so its internal guards are set. */
#include <mongoc/mongoc.h>
#include <mongoc/mongoc-rust-private.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DEFAULT_N_DOCS 10000
#define URI            "mongodb://localhost:27017"
#define DB_NAME        "perf"
#define COLL_NAME      "bench"
#define NS             DB_NAME "." COLL_NAME

/* ── timing ──────────────────────────────────────────────────────────── */

static double
now_sec (void)
{
   struct timespec ts;
   clock_gettime (CLOCK_MONOTONIC, &ts);
   return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* ── shared helpers ──────────────────────────────────────────────────── */

static void
drop_collection (void)
{
   mongoc_client_t *client = mongoc_client_new (URI);
   mongoc_database_t *db = mongoc_client_get_database (client, DB_NAME);
   mongoc_collection_t *coll = mongoc_database_get_collection (db, COLL_NAME);
   mongoc_collection_drop (coll, NULL);
   mongoc_collection_destroy (coll);
   mongoc_database_destroy (db);
   mongoc_client_destroy (client);
}

static void
print_result (const char *label, int n, double build_sec, double exec_sec)
{
   double total = build_sec + exec_sec;
   printf ("%-6s  build %6.1f ms  execute %6.1f ms  total %6.1f ms  "
           "(%6.0f docs/s)\n",
           label,
           build_sec * 1e3,
           exec_sec * 1e3,
           total * 1e3,
           n / total);
}

/* ── Rust async driver ───────────────────────────────────────────────── */
/*
 * The entire operation list must be encoded as a single BSON document:
 *   { models: [ { insertOne: { namespace: NS, document: {x: i} } }, ... ] }
 * before handing it to the driver.  That encoding is the "build" phase.
 */
static void
bench_rust (int n)
{
   mongoc_async_client_t *client = mongoc_async_client_new (URI);
   if (!client) {
      fprintf (stderr, "rust: failed to create client\n");
      return;
   }

   /* ── build phase ─────────────────────────────────────────────────── */
   double t_build0 = now_sec ();

   bson_t *args = bson_new ();
   bson_t models_arr;
   bson_append_array_unsafe_begin (args, "models", -1, &models_arr);

   for (int i = 0; i < n; i++) {
      char key[16];
      const char *key_ptr;
      bson_uint32_to_string ((uint32_t) i, &key_ptr, key, sizeof key);

      bson_t model, insert_one, doc;
      bson_append_document_begin (&models_arr, key_ptr, -1, &model);
      bson_append_document_begin (&model, "insertOne", -1, &insert_one);
      BSON_APPEND_UTF8 (&insert_one, "namespace", NS);
      bson_append_document_begin (&insert_one, "document", -1, &doc);
      BSON_APPEND_INT32 (&doc, "x", i);
      bson_append_document_end (&insert_one, &doc);
      bson_append_document_end (&model, &insert_one);
      bson_append_document_end (&models_arr, &model);
   }
   bson_append_array_end (args, &models_arr);

   double t_build = now_sec () - t_build0;

   /* ── execute phase ───────────────────────────────────────────────── */
   bson_unowned_t args_view = {.data = bson_get_data (args), .len = args->len};

   double t_exec0 = now_sec ();
   mongoc_async_error_t *err = NULL;
   bson_owned_t *partial = NULL;
   bson_owned_t *result =
      mongoc_async_client_bulk_write_await (client, args_view, &err, &partial);
   double t_exec = now_sec () - t_exec0;

   if (!result) {
      fprintf (stderr, "rust: bulk write failed: %s\n",
               err ? mongoc_async_error_get_message (err) : "(unknown)");
   } else {
      print_result ("rust", n, t_build, t_exec);
      bson_owned_destroy (result);
   }

   mongoc_async_error_destroy (err);
   if (partial) {
      bson_owned_destroy (partial);
   }
   bson_destroy (args);
   mongoc_async_client_destroy (client);
}

/* ── Classic sync C driver ──────────────────────────────────────────── */
/*
 * Operations are appended one at a time via mongoc_bulkwrite_append_insertone,
 * then sent in a single mongoc_bulkwrite_execute call.
 */
static void
bench_c (int n)
{
   mongoc_client_t *client = mongoc_client_new (URI);
   if (!client) {
      fprintf (stderr, "c: failed to create client\n");
      return;
   }

   /* ── build phase ─────────────────────────────────────────────────── */
   double t_build0 = now_sec ();

   mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);
   for (int i = 0; i < n; i++) {
      bson_t doc;
      bson_init (&doc);
      BSON_APPEND_INT32 (&doc, "x", i);
      bson_error_t error;
      bool ok = mongoc_bulkwrite_append_insertone (bw, NS, &doc, NULL, &error);
      bson_destroy (&doc);
      if (!ok) {
         fprintf (stderr, "c: append_insertone failed: %s\n", error.message);
         mongoc_bulkwrite_destroy (bw);
         mongoc_client_destroy (client);
         return;
      }
   }

   double t_build = now_sec () - t_build0;

   /* ── execute phase ───────────────────────────────────────────────── */
   double t_exec0 = now_sec ();
   mongoc_bulkwritereturn_t ret = mongoc_bulkwrite_execute (bw, NULL);
   double t_exec = now_sec () - t_exec0;

   if (ret.exc) {
      bson_error_t error;
      mongoc_bulkwriteexception_error (ret.exc, &error);
      fprintf (stderr, "c: bulk write failed: %s\n", error.message);
   } else {
      print_result ("c", n, t_build, t_exec);
   }

   mongoc_bulkwriteresult_destroy (ret.res);
   mongoc_bulkwriteexception_destroy (ret.exc);
   mongoc_bulkwrite_destroy (bw);
   mongoc_client_destroy (client);
}

/* ── main ────────────────────────────────────────────────────────────── */

int
main (int argc, char *argv[])
{
   int n = DEFAULT_N_DOCS;
   if (argc > 1) {
      n = atoi (argv[1]);
      if (n <= 0) {
         fprintf (stderr, "usage: %s [n_docs]\n", argv[0]);
         return EXIT_FAILURE;
      }
   }

   mongoc_init ();

   printf ("bulk insert benchmark  n=%d\n", n);
   printf ("%-6s  %-22s  %-22s  %-22s  %s\n",
           "driver", "build", "execute", "total", "docs/s");
   printf ("------  ----------------------  "
           "----------------------  ----------------------  ------\n");

   drop_collection ();
   bench_rust (n);

   drop_collection ();
   bench_c (n);

   mongoc_cleanup ();
   return EXIT_SUCCESS;
}
