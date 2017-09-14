#include <bson.h>
#include <mongoc.h>

int main() {
   bson_t* doc;
   bson_error_t err;
   mongoc_client_t* client;
   mongoc_collection_t* coll;
   mongoc_change_stream_t* stream;

   mongoc_init();

   client = mongoc_client_new("mongodb://localhost:27017,localhost:27018,localhost:27019/db?replicaSet=rs0");
   if (!client) {
      printf("Could not connect to replica set\n");
      return 1;
   }

   coll = mongoc_client_get_collection(client, "db", "coll");
   stream = mongoc_collection_watch(coll, NULL, NULL);

   while (mongoc_change_stream_next(stream, &doc)) {
      printf("Got document: %s\n", bson_as_json(doc, NULL));
   }

   if (mongoc_change_stream_error(stream, &err)) {
      printf("Error: %s\n", err.message);
      return 1;
   }

   mongoc_change_stream_destroy(stream);
   mongoc_collection_destroy (coll);
   mongoc_client_destroy (client);
   mongoc_cleanup();
}