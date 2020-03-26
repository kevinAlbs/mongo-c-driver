/* gcc example-client.c -o example-client $(pkg-config --cflags --libs
 * libmongoc-1.0) */

/* ./example-client [CONNECTION_STRING [COLLECTION_NAME]] */

#include <mongoc/mongoc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main (int argc, char *argv[])
{
   mongoc_client_t *client;
   mongoc_collection_t *collection;
   bson_t doc;
   char *str;
   bson_t reply;
   bson_error_t error;

   mongoc_init ();

   client = mongoc_client_new ("mongodb://localhost:27017,localhost:27018");
   if (!client) {
      return EXIT_FAILURE;
   }

   mongoc_client_set_error_api (client, 2);

   bson_init (&doc);
   BCON_APPEND (&doc, "x", BCON_INT32 (1));
   collection = mongoc_client_get_collection (client, "test", "test");

   while (true) {
      printf ("inserting\n");
      mongoc_collection_insert_one (collection, &doc, NULL /* opts */, &reply, &error);
      str = bson_as_json (&reply, NULL);
      printf ("reply=%s\n", str);
      sleep (1);
   }

   bson_destroy (&doc);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   mongoc_cleanup ();

   return EXIT_SUCCESS;
}
