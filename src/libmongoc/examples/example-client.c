#include <mongoc/mongoc.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

int
main (int argc, char *argv[])
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   mongoc_uri_t *uri;
   int64_t iterations;
   bson_t ping;
   bson_t reply;
   bson_error_t error;

   mongoc_init ();

   uri = mongoc_uri_new ("mongodb://localhost:27017");

   iterations = 0;
   while (true) {
      MONGOC_DEBUG ("Iteration %d start", iterations);
      pool = mongoc_client_pool_new (uri);
      client = mongoc_client_pool_pop (pool);

      iterations++;

      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
      MONGOC_DEBUG ("Iteration end");
   }

   mongoc_uri_destroy (uri);
   mongoc_client_destroy (client);
   mongoc_cleanup ();

   return EXIT_SUCCESS;
}
