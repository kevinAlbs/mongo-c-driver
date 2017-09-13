#include <bson.h>
#include <mongoc.h>

int main() {
   bson_t* doc;
   bson_error_t err;
   mongoc_client_t* client;
   mongoc_collection_t* coll;
   mongoc_change_stream_t* stream;

   mongoc_init();

   /* Assumes a three node replica set named rs0 all on localhost.
    * Use mtools to make a simple replica set.
    * mlaunch init --replicaset --nodes 3 --name rs0 --priority --hostname localhost --dir replsetDir
    */
   client = mongoc_client_new("mongodb://localhost:27017,localhost:27018,localhost:27019/db?replicaSet=rs0");
   if (!client) return 1;

   printf("1\n");

   coll = mongoc_client_get_collection(client, "db", "coll");

   // Test with a long await time. Insert into db.coll to see messages.
   bson_t* opts = BCON_NEW("maxAwaitTimeMS", BCON_INT32(10000));

   printf("2\n");
   stream = mongoc_collection_watch(coll, NULL, opts);

   printf("3\n");

   while (mongoc_change_stream_next(stream, &doc)) {
      printf("4\n");
      printf("Got document: %s\n", bson_as_json(doc, NULL));
   }

   printf("5\n");
   if (mongoc_change_stream_error(stream, &err)) {
      printf("Error: %s\n", err.message);
      return 1;
   }
   printf("6\n");

   bson_destroy (opts);

   mongoc_change_stream_destroy(stream);

   mongoc_client_destroy (client);
   mongoc_cleanup();
}