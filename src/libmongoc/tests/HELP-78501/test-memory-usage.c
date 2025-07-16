#include <mongoc/mongoc.h>

static void
create_big_collection (mongoc_client_t *client, const char *db, const char *coll)
{
   mongoc_collection_t *collection = mongoc_client_get_collection (client, db, coll);
   bson_error_t error;

   // Drop collection if already exists:
   mongoc_collection_drop (collection, NULL);

   bson_t large_doc = BSON_INITIALIZER;

   // Create large document:
   {
      char *large_data = bson_malloc (1024 * 1024); // 1 MB of data
      memset (large_data, 'x', 1024 * 1024 - 1);
      large_data[1024 * 1024 - 1] = '\0'; // Null-terminate;
      BSON_APPEND_UTF8 (&large_doc, "large_data", large_data);
      bson_free (large_data);
   }

   for (size_t i = 0; i < 100; i++) {
      if (!mongoc_collection_insert_one (collection, &large_doc, NULL, NULL, &error)) {
         fprintf (stderr, "Error inserting document: %s\n", error.message);
         abort ();
      }
   }

   bson_destroy (&large_doc);
   mongoc_collection_destroy (collection);
}

static void
find_all_documents (mongoc_client_t *client, const char *db, const char *coll)
{
   mongoc_collection_t *collection = mongoc_client_get_collection (client, db, coll);
   bson_t filter = BSON_INITIALIZER;
   mongoc_cursor_t *cursor = mongoc_collection_find_with_opts (collection, &filter, NULL, NULL);
   const bson_t *doc;
   size_t count = 0;
   while (mongoc_cursor_next (cursor, &doc)) {
      count++;
   }
   printf ("Retrieved %zu documents\n", count);
   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (collection);
}

int
main (int argc, char *argv[])
{
   bson_mem_vtable_t vtable = {
      .malloc = malloc, .calloc = calloc, .realloc = realloc, .free = free, .aligned_alloc = aligned_alloc};
   bson_mem_set_vtable (&vtable);

   mongoc_init ();
   mongoc_uri_t *uri = mongoc_uri_new ("mongodb://localhost:27017");
   mongoc_client_pool_t *pool = mongoc_client_pool_new (uri);

   // Insert large documents:
   {
      mongoc_client_t *client = mongoc_client_pool_pop (pool);
      create_big_collection (client, "db", "big_collection");
      mongoc_client_pool_push (pool, client);
   }

   // Pop 100 clients and find all documents:
   {
      mongoc_client_t *client[100];
      for (int i = 0; i < 100; i++) {
         client[i] = mongoc_client_pool_pop (pool);
      }

      printf ("Pausing 5 seconds to show stable memory usage: about to create cursors\n");
      sleep (5);

      for (int i = 0; i < 100; i++) {
         // Server replies may be 16MB each.
         find_all_documents (client[i], "db", "big_collection");
      }

      for (int i = 0; i < 100; i++) {
         mongoc_client_pool_push (pool, client[i]);
      }
   }

   printf ("Pausing 5 seconds to show stable memory usage: about to destroy pool\n");
   sleep (5);
   mongoc_client_pool_destroy (pool); // Does not destroy client connections!
   printf ("Pausing 5 seconds to show stable memory usage: about to exit\n");
   sleep (5);

   mongoc_uri_destroy (uri);
   mongoc_cleanup ();


   return EXIT_SUCCESS;
}
