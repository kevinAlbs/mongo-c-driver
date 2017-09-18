#include <mongoc.h>
#include "mongoc-client-private.h"
#include "mock_server/mock-server.h"
#include "mock_server/future.h"
#include "mock_server/future-functions.h"
#include "TestSuite.h"

/* Test a basic unadorned change_stream */
static void test_change_stream_watch () {
   mock_server_t *server;
   request_t *request;
   future_t *future;
   mongoc_client_t *client;
   mongoc_collection_t *coll;
   const bson_t* next_doc = NULL;
   bson_t empty = BSON_INITIALIZER;

   /* TODO, opcode not supported if using the WIRE_VERSION_MAX */
   server = mock_server_with_autoismaster(5);
   mock_server_run(server);

   client = mongoc_client_new_from_uri (mock_server_get_uri (server));
      ASSERT (client);

   coll = mongoc_client_get_collection(client, "db", "coll");
      ASSERT (coll);

   mongoc_change_stream_t* stream = mongoc_collection_watch(coll, &empty, NULL);
   future = future_change_stream_next (stream, &next_doc);

   request = mock_server_receives_command (server,
                                           "db",
                                           MONGOC_QUERY_SLAVE_OK,
                                            "{"
                                            "'aggregate' : 'coll',"
                                            "'pipeline' : "
                                            "   [ { '$changeStream':{} } ],"
                                            "'cursor' : {}"
                                            "}");

   mock_server_replies_simple (request, "{'cursor' : {'id' : 123,'ns' : 'db.coll','firstBatch' : []},'ok' : 1 }");
   request = mock_server_receives_command (server, "db", MONGOC_QUERY_SLAVE_OK, "{ 'getMore' : 123, 'collection' : 'coll' }");
   mock_server_replies_simple (request, "{ 'cursor' : { 'nextBatch' : [] }, 'ok': 1 }");

   future_wait(future);
   ASSERT (!future_get_bool (future));
   ASSERT (!mongoc_change_stream_error_document(stream, NULL, NULL));
   ASSERT (next_doc == NULL);

   /* Another call to next should produce another getMore */
   future = future_change_stream_next (stream, &next_doc);
   request = mock_server_receives_command (server, "db", MONGOC_QUERY_SLAVE_OK, "{ 'getMore' : 123, 'collection' : 'coll' }");
   mock_server_replies_simple (request, "{ 'cursor' : { 'nextBatch' : [] }, 'ok': 1 }");
   future_wait(future);

   ASSERT (!future_get_bool (future));
   ASSERT (!mongoc_change_stream_error_document(stream, NULL, NULL));
   ASSERT (next_doc == NULL);

   future = future_change_stream_destroy (stream);

   mock_server_receives_command(server, "db", MONGOC_QUERY_SLAVE_OK, "{ 'killCursors' : 'coll', 'cursors' : [ 123 ] }");
   mock_server_replies_simple(request, "{ 'cursorsKilled': [123] }");

   future_wait(future);

   future_destroy (future);
   request_destroy (request);
   mongoc_client_destroy (client);
   mock_server_destroy (server);
}

/* Test behavior on a simple resumable error */
static void test_change_stream_resumable_error () {
   mock_server_t *server;
   request_t *request;
   future_t *future;
   mongoc_client_t *client;
   bson_t empty = BSON_INITIALIZER;
   const bson_t* next_doc = NULL;

   server = mock_server_with_autoismaster(5);
   mock_server_run(server);

   client = mongoc_client_new_from_uri (mock_server_get_uri (server));
   ASSERT (client);

   mongoc_collection_t* coll = mongoc_client_get_collection(client, "db", "coll");


   mongoc_change_stream_t* stream = mongoc_collection_watch(coll, &empty, NULL);

   future = future_change_stream_next (stream, &next_doc);

   request = mock_server_receives_command (server,
                                           "db",
                                           MONGOC_QUERY_SLAVE_OK,
                                           "{ 'aggregate' : 'coll', 'pipeline' : [ { '$changeStream' : {  } } ], 'cursor' : {  } }");

   mock_server_replies_simple (request, "{'cursor' : {'id' : 123,'ns' : 'db.coll','firstBatch' : []},'ok' : 1 }");
   request = mock_server_receives_command (server, "db", MONGOC_QUERY_SLAVE_OK, "{ 'getMore' : 123, 'collection' : 'coll' }");
   mock_server_replies_simple (request, "{ 'code': 10107, 'errmsg': 'not master', 'ok': 0 }");

   /* On a resumable error, the change stream will first attempt to kill the
    * cursor and establish a new one with the same command.
    */

   mock_server_receives_command(server, "db", MONGOC_QUERY_SLAVE_OK, "{ 'killCursors' : 'coll', 'cursors' : [ 123 ] }");
   mock_server_replies_simple(request, "{ 'cursorsKilled': [123] }");

   request = mock_server_receives_command (server,
                                           "db",
                                           MONGOC_QUERY_SLAVE_OK,
                                           "{ 'aggregate' : 'coll', 'pipeline' : [ { '$changeStream' : {  } } ], 'cursor' : {  } }");
   mock_server_replies_simple (request, "{'cursor' : {'id' : 124,'ns' : 'db.coll','firstBatch' : []},'ok' : 1 }");
   request = mock_server_receives_command (server, "db", MONGOC_QUERY_SLAVE_OK, "{ 'getMore' : 124, 'collection' : 'coll' }");
   mock_server_replies_simple (request, "{ 'cursor' : { 'nextBatch' : [] }, 'ok': 1 }");

   future_wait(future);
   ASSERT (!future_get_bool (future));
   ASSERT (!mongoc_change_stream_error_document(stream, NULL, NULL));
   ASSERT (next_doc == NULL);

   /* Now test resumable error that occurs twice in a row */
   future = future_change_stream_next (stream, &next_doc);
   request = mock_server_receives_command(server, "db", MONGOC_QUERY_SLAVE_OK, "{ 'getMore' : 124, 'collection' : 'coll' }");
   mock_server_replies_simple (request, "{ 'code': 10107, 'errmsg': 'not master', 'ok': 0 }");

   /* Kill cursor */
   mock_server_receives_command(server, "db", MONGOC_QUERY_SLAVE_OK, "{ 'killCursors' : 'coll', 'cursors' : [ 124 ] }");
   mock_server_replies_simple(request, "{ 'cursorsKilled': [124] }");

   /* Re-establish connection */
   request = mock_server_receives_command (server,
                                           "db",
                                           MONGOC_QUERY_SLAVE_OK,
                                           "{ 'aggregate' : 'coll', 'pipeline' : [ { '$changeStream' : {  } } ], 'cursor' : {  } }");
   mock_server_replies_simple (request, "{'cursor' : {'id' : 125,'ns' : 'db.coll','firstBatch' : []},'ok' : 1 }");
   request = mock_server_receives_command (server, "db", MONGOC_QUERY_SLAVE_OK, "{ 'getMore' : 125, 'collection' : 'coll' }");
   mock_server_replies_simple (request, "{ 'code': 10107, 'errmsg': 'not master', 'ok': 0 }");

   /* Check that error is returned */
   future_wait(future);
   ASSERT (!future_get_bool (future));
   ASSERT (mongoc_change_stream_error_document(stream, NULL, NULL));
   ASSERT (next_doc == NULL);

   future = future_change_stream_destroy (stream);

   mock_server_receives_command(server, "db", MONGOC_QUERY_SLAVE_OK, "{ 'killCursors' : 'coll', 'cursors' : [ 125 ] }");
   mock_server_replies_simple(request, "{ 'cursorsKilled': [125] }");

   future_wait(future);

   future_destroy (future);
   request_destroy (request);
   mongoc_client_destroy (client);
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
   ASSERT(bson_iter_init_find(&iter, bson, "isMaster"));
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
   TestSuite_AddMockServerTest (testSuite, "/changestream/resumable_error", test_change_stream_resumable_error);
   // TestSuite_AddMockServerTest (testSuite, "/changestream/playing", test_example);
}