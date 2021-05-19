/* Conversation in:
 * Compile with:
 * clang -o long_runner long_runner.c $(pkg-config --cflags --libs libmongoc-1.0) -pthread
 */

#include <mongoc/mongoc.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define URI "mongodb://localhost:27017"
#define DB "test"
#define COLL "coll"

typedef struct {
   int tid;
   mongoc_client_pool_t *pool;
} thread_args_t;

/* thread_find repeatedly runs find with a filter {_id: 0} */
void *thread_find (void *arg) {
   mongoc_client_pool_t *pool;
   bson_t filter;
   int64_t ops = 0;
   int64_t running_ops = 0;
   bson_error_t error;
   thread_args_t *args = (thread_args_t*) arg;

   pool = args->pool;

   bson_init (&filter);
   BSON_APPEND_INT32 (&filter, "_id", 0);
   
   while (true) {
      mongoc_client_t* client;
      mongoc_collection_t *coll;
      mongoc_cursor_t *cursor;

      client = mongoc_client_pool_pop (pool);
      coll = mongoc_client_get_collection (client, DB, COLL);
      cursor = mongoc_collection_find_with_opts (coll, &filter, NULL /* opts */, NULL /* read_prefs */);
      if (mongoc_cursor_error (cursor, &error)) {
         MONGOC_ERROR ("[tid=%d] find returned error: %s", args->tid, error.message);
         return NULL;
      }
      ops += 1;
      if (ops >= 1000000) {
         running_ops += ops;
         ops = 0;
         MONGOC_INFO ("[tid=%d] ran %" PRId64 " ops", args->tid, running_ops);
      }

      mongoc_collection_destroy (coll);
      mongoc_client_pool_push (pool, client);
   }
   return NULL;
}

int
main (int argc, char *argv[])
{
   mongoc_client_pool_t *pool;
   mongoc_uri_t *uri;
   pthread_t* threads;
   thread_args_t* thread_args;
   int n = 10;
   int i;

   mongoc_init ();

   uri = mongoc_uri_new (URI);
   if (!uri) {
      MONGOC_ERROR ("invalid URI %s", URI);
      return 1;
   }
   pool = mongoc_client_pool_new (uri);

   if (argc == 2) {
      n = atoi (argv[1]);
      if (n <= 0) {
         MONGOC_ERROR ("invalid thread count: %d", n);
         return 1;
      }
   }

   threads = bson_malloc0 (sizeof(pthread_t) * n);
   thread_args = bson_malloc0 (sizeof(thread_args_t) * n);

   for (i = 0; i < n; i++) {
      thread_args[i].pool = pool;
      thread_args[i].tid = i;
      pthread_create(threads + i, NULL /* pthread_attr_t */, thread_find, thread_args + i);
   }

   MONGOC_INFO ("running with %d threads", n);

   for (i = 0; i < n; i++) {
      pthread_join (threads[i], NULL);
   }
   
   mongoc_cleanup ();
}
