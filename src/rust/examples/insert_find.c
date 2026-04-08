/**
 * insert_find.c — mongoc-async example: concurrent inserts then find.
 *
 * Demonstrates the async future API:
 *   1. Start two inserts without blocking — each returns a future immediately.
 *   2. Drive both futures to completion with a shared poll loop.
 *   3. Find all inserted documents using the blocking await API.
 *
 * Build:
 *   cmake --build cmake-build --target mongoc-async-example-insert-find
 *
 * Run (requires mongod on localhost:27017):
 *   ./cmake-build/src/rust/examples/mongoc-async-example-insert-find
 */

#include <mongoc/mongoc-rust-private.h>

#include <stdio.h>
#include <stdlib.h>

int
main (void)
{
   /* ── connect ─────────────────────────────────────────────────────── */
   mongoc_async_client_t *client =
      mongoc_async_client_new ("mongodb://localhost:27017");
   if (!client) {
      fprintf (stderr, "error: failed to create client\n");
      return EXIT_FAILURE;
   }

   mongoc_async_database_t *db =
      mongoc_async_client_get_database (client, "example");
   mongoc_async_collection_t *coll =
      mongoc_async_database_get_collection (db, "people");

   /* Drop so each run starts from an empty collection. */
   mongoc_async_collection_drop_await (coll);

   /* ── start two inserts without waiting ──────────────────────────── */
   /* Build the two documents to insert. */
   bson_t *doc1 = bson_new ();
   BSON_APPEND_UTF8 (doc1, "name", "Alice");
   BSON_APPEND_INT32 (doc1, "score", 42);

   bson_t *doc2 = bson_new ();
   BSON_APPEND_UTF8 (doc2, "name", "Bob");
   BSON_APPEND_INT32 (doc2, "score", 99);

   bson_unowned_t view1 = {.data = bson_get_data (doc1), .len = doc1->len};
   bson_unowned_t view2 = {.data = bson_get_data (doc2), .len = doc2->len};

   /* Neither call blocks — both return a future immediately.
    * The inserts are now in flight concurrently on the Tokio runtime. */
   mongoc_async_future_t *f1 = mongoc_async_collection_insert_one (coll, view1, NULL);
   mongoc_async_future_t *f2 = mongoc_async_collection_insert_one (coll, view2, NULL);

   bson_destroy (doc1);
   bson_destroy (doc2);

   /* ── drive both futures to completion ───────────────────────────── */
   /* Poll both in a loop.  make_progress drives the shared Tokio runtime,
    * so a single call can advance work for either future. */
   for (;;) {
      bool done1 = mongoc_async_future_poll (f1);
      bool done2 = mongoc_async_future_poll (f2);
      if (done1 && done2) {
         break;
      }
      mongoc_async_future_make_progress (f1);
   }

   /* ── collect results ─────────────────────────────────────────────── */
   bool ok1 = mongoc_async_future_get_void (f1);
   bool ok2 = mongoc_async_future_get_void (f2);
   mongoc_async_future_destroy (f1);
   mongoc_async_future_destroy (f2);

   if (!ok1 || !ok2) {
      fprintf (stderr, "error: one or more inserts failed\n");
      mongoc_async_collection_destroy (coll);
      mongoc_async_database_destroy (db);
      mongoc_async_client_destroy (client);
      return EXIT_FAILURE;
   }
   printf ("both inserts complete\n");

   /* ── find all documents and print ───────────────────────────────── */
   bson_t empty = BSON_INITIALIZER;
   bson_unowned_t filter = {.data = bson_get_data (&empty), .len = empty.len};

   mongoc_async_error_t *err = NULL;
   mongoc_async_cursor_t *cursor =
      mongoc_async_collection_find_await (coll, filter, NULL, &err);
   if (!cursor) {
      fprintf (stderr, "find error: %s\n",
               err ? mongoc_async_error_get_message (err) : "(unknown)");
      mongoc_async_error_destroy (err);
      mongoc_async_collection_destroy (coll);
      mongoc_async_database_destroy (db);
      mongoc_async_client_destroy (client);
      return EXIT_FAILURE;
   }

   while (mongoc_async_cursor_next_await (cursor, &err)) {
      bson_unowned_t cur = mongoc_async_cursor_current (cursor);
      bson_t view;
      bson_init_static (&view, (const uint8_t *) cur.data, cur.len);
      char *json = bson_as_relaxed_extended_json (&view, NULL);
      printf ("found: %s\n", json);
      bson_free (json);
   }
   if (err) {
      fprintf (stderr, "cursor error: %s\n", mongoc_async_error_get_message (err));
      mongoc_async_error_destroy (err);
   }

   /* ── cleanup ─────────────────────────────────────────────────────── */
   mongoc_async_cursor_destroy (cursor);
   mongoc_async_collection_destroy (coll);
   mongoc_async_database_destroy (db);
   mongoc_async_client_destroy (client);

   return EXIT_SUCCESS;
}
