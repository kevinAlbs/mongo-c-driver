#include <mongoc.h>
#include "mock_server/mock-server.h"
#include "mock_server/future.h"
#include "mock_server/future-functions.h"
#include "TestSuite.h"

static void test_change_stream_watch () {
   mock_server_t *server;
   request_t *request;
   mongoc_uri_t *uri;
   future_t *future;
   mongoc_client_t *client;

   const int maxWireVersion = 5; // TODO. found in mongoreplay
   server = mock_server_with_autoismaster(maxWireVersion);
   mock_server_run(server);

   client = mongoc_client_new_from_uri (mock_server_get_uri (server));
   ASSERT (client);

   // Test that an empty change stream will send a command with the $changeStream prepended.

   // I believe this will not incur any communication.
   mongoc_collection_t* coll = mongoc_client_get_collection(client, "testdb", "testcoll");

   future = future_collection_watch(coll, NULL, NULL);

   request = mock_server_receives_command (server,
                                           "testdb",
                                           MONGOC_QUERY_TAILABLE_CURSOR | MONGOC_QUERY_AWAIT_DATA,
                                           "{ aggregate: 'testcoll', pipeline: [ {$changeStream: {} } ] }");

   mock_server_replies_simple (request, "[]");

   future_wait(future);

   mongoc_change_stream_t* change_stream = future_get_mongoc_change_stream_ptr (future);

   mongoc_change_stream_destroy(change_stream);

   future_destroy (future);
   request_destroy (request);
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
   mock_server_destroy (server);
}

static void test_example() {
   // A special place where I can play.

   // The mock server solves the problem of reliably testing client/server interaction.
   // Using the mock server, we can have exact control of what messages the server returns and when.
   // This allows us to reproduce cases that would be near impossible to reproduce with a live
   // mongod process. E.g. we perform an insert
   mock_server_t *server = mock_server_new(); // using with_automaster will automatically reply to {ismaster}
   mock_server_run(server);

   // The problem is that operations which require a response from the server are blocking.
   // If we do mongoc_collection_insert, this returns after a server response.
   // In order to do this interaction the mock server can create separate threads.


   // The client will not send an ismaster until the first command I believe.
   mongoc_client_t* client = mongoc_client_new_from_uri (mock_server_get_uri(server));

   // Let's trigger our client to send an { ismaster: 1 }
   future_t* future = future_client_select_server(client, true, NULL, NULL);

   // This will block until the mock server receives the ismaster request.
   request_t* request = mock_server_receives_ismaster (server);
   ASSERT (request);

   // Now we can use the server request to check what the client sent.
   const bson_t* bson = request_get_doc(request, 0);
   printf("%s\n", bson_as_json(bson, NULL));
   bson_iter_t iter;
   BSON_ASSERT(bson_iter_init_find(&iter, bson, "isMaster"));
   printf("%s\n", bson_as_json(bson, NULL));

   // The request_t has client specific data, so we need to use it to reply.
   mock_server_replies_simple (request, "{ 'ismaster': 1 }");

   // Now the original call to future_client_select_server is able to finish.

   future_wait(future);
   printf("Done.\n");
   request_destroy (request);
}

void test_change_stream_install (TestSuite* testSuite) {
   TestSuite_AddMockServerTest (testSuite, "/changestream/watch", test_change_stream_watch);
 // TestSuite_AddMockServerTest (testSuite, "/changestream/playing", test_example);
}