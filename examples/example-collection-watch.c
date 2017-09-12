#include <mongoc.h>

int main(int argc, char** argv) {
   mongoc_init();
   mongoc_client_t* client = mongoc_client_new("localhost:27017");
   if (!client) printf("Could not get client\n");
   mongoc_collection_t* coll = mongoc_client_get_collection(client, "testdb", "testcoll");

   bson_t pipeline = BSON_INITIALIZER;
   bson_t opts = BSON_INITIALIZER;
   mongoc_change_stream_t* stream = mongoc_collection_watch(coll, &pipeline, &opts);
   bson_destroy(&pipeline);
   bson_destroy(&opts);

   mongoc_change_stream_destroy(stream);

   mongoc_client_destroy (client);
   mongoc_cleanup();
}