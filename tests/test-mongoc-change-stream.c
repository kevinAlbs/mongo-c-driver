#include <mongoc.h>
#include "mongoc-client-private.h"
#include "mock_server/mock-server.h"
#include "mock_server/future.h"
#include "mock_server/future-functions.h"
#include "TestSuite.h"
#include "test-conveniences.h"

#define DESTROY_CHANGE_STREAM(cursor_id)                                  \
   future = future_change_stream_destroy (stream);                        \
   request = mock_server_receives_command (                               \
      server,                                                             \
      "db",                                                               \
      MONGOC_QUERY_SLAVE_OK,                                              \
      "{ 'killCursors' : 'coll', 'cursors' : [ " cursor_id " ] }");       \
   mock_server_replies_simple (request,                                   \
                               "{ 'cursorsKilled': [ " cursor_id " ] }"); \
   future_wait (future);                                                  \
   future_destroy (future);                                               \
   request_destroy (request);


/* Test a basic unadorned change_stream */
static void
test_change_stream_watch ()
{
   mock_server_t *server;
   request_t *request;
   future_t *future;
   mongoc_client_t *client;
   mongoc_collection_t *coll;
   const bson_t *next_doc = NULL;
   bson_t empty = BSON_INITIALIZER;

   /* TODO, opcode not supported if using the WIRE_VERSION_MAX */
   server = mock_server_with_autoismaster (5);
   mock_server_run (server);

   client = mongoc_client_new_from_uri (mock_server_get_uri (server));
      ASSERT (client);

   coll = mongoc_client_get_collection (client, "db", "coll");
      ASSERT (coll);

   mongoc_change_stream_t *stream =
      mongoc_collection_watch (coll, &empty, NULL);
   future = future_change_stream_next (stream, &next_doc);

   request = mock_server_receives_command (server,
                                           "db",
                                           MONGOC_QUERY_SLAVE_OK,
                                           "{"
                                              "'aggregate' : 'coll',"
                                              "'pipeline' : "
                                              "   [ { '$changeStream':{ 'fullDocument' : 'default' } } ],"
                                              "'cursor' : {}"
                                              "}");

   mock_server_replies_simple (
      request,
      "{'cursor' : {'id' : 123,'ns' : 'db.coll','firstBatch' : []},'ok' : 1 }");
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'getMore' : 123, 'collection' : 'coll' }");
   mock_server_replies_simple (request,
                               "{ 'cursor' : { 'nextBatch' : [] }, 'ok': 1 }");

   future_wait (future);
      ASSERT (!future_get_bool (future));
      ASSERT (!mongoc_change_stream_error_document (stream, NULL, NULL));
      ASSERT (next_doc == NULL);

   /* Another call to next should produce another getMore */
   future = future_change_stream_next (stream, &next_doc);
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'getMore' : 123, 'collection' : 'coll' }");
   mock_server_replies_simple (request,
                               "{ 'cursor' : { 'nextBatch' : [] }, 'ok': 1 }");
   future_wait (future);

      ASSERT (!future_get_bool (future));
      ASSERT (!mongoc_change_stream_error_document (stream, NULL, NULL));
      ASSERT (next_doc == NULL);

   future = future_change_stream_destroy (stream);

   mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'killCursors' : 'coll', 'cursors' : [ 123 ] }");
   mock_server_replies_simple (request, "{ 'cursorsKilled': [123] }");

   future_wait (future);

   future_destroy (future);
   request_destroy (request);
   mongoc_client_destroy (client);
   mock_server_destroy (server);
}

static void
test_example ()
{
   // A special place where I can play.

   // The mock server solves the problem of reliably testing client/server
   // interaction.
   // Using the mock server, we can have exact control of what messages the
   // server returns and when.
   // This allows us to reproduce cases that would be near impossible to
   // reproduce with a live
   // mongod process. E.g. we perform an insert
   mock_server_t *server = mock_server_new (); // using with_automaster will
   // automatically reply to
   // {ismaster}
   mock_server_run (server);

   // The problem is that operations which require a response from the server
   // are blocking.
   // If we do mongoc_collection_insert, this returns after a server response.
   // In order to do this interaction the mock server can create separate
   // threads.


   // The client will not send an ismaster until the first command I believe.
   mongoc_client_t *client =
      mongoc_client_new_from_uri (mock_server_get_uri (server));

   // Let's trigger our client to send an { ismaster: 1 }
   future_t *future = future_client_select_server (client, true, NULL, NULL);

   // This will block until the mock server receives the ismaster request.
   request_t *request = mock_server_receives_ismaster (server);
      ASSERT (request);

   // Now we can use the server request to check what the client sent.
   const bson_t *bson = request_get_doc (request, 0);
   printf ("%s\n", bson_as_json (bson, NULL));
   bson_iter_t iter;
      ASSERT (bson_iter_init_find (&iter, bson, "isMaster"));
   printf ("%s\n", bson_as_json (bson, NULL));

   // The request_t has client specific data, so we need to use it to reply.
   mock_server_replies_simple (request, "{ 'ismaster': 1 }");

   // Now the original call to future_client_select_server is able to finish.

   future_wait (future);
   printf ("Done.\n");
   request_destroy (request);
}


/* 1. $changeStream must be the first stage in a change stream pipeline sent
 * to the server */
static void
test_change_stream_pipeline ()
{
   mock_server_t *server;
   request_t *request;
   future_t *future;
   mongoc_client_t *client;
   mongoc_collection_t *coll;
   const bson_t *next_doc = NULL;
   bson_t empty_pipeline = BSON_INITIALIZER;
   bson_t *nonempty_pipeline = BCON_NEW ("pipeline",
                                         "[",
                                         "{",
                                         "$project",
                                         "{",
                                         "ns",
                                         BCON_BOOL (false),
                                         "}",
                                         "}",
                                         "]");
   mongoc_change_stream_t *stream;

   server = mock_server_with_autoismaster (5);
   mock_server_run (server);

   client = mongoc_client_new_from_uri (mock_server_get_uri (server));
   ASSERT (client);

   coll = mongoc_client_get_collection (client, "db", "coll");
   ASSERT (coll);

   stream = mongoc_collection_watch (coll, &empty_pipeline, NULL);
   bson_destroy (&empty_pipeline);
   future = future_change_stream_next (stream, &next_doc);

   request = mock_server_receives_command (server,
                                           "db",
                                           MONGOC_QUERY_SLAVE_OK,
                                           "{"
                                           "'aggregate' : 'coll',"
                                           "'pipeline' : "
                                           "   ["
                                           "      { '$changeStream':{ 'fullDocument' : 'default' } }"
                                           "   ],"
                                           "'cursor' : {}"
                                           "}");

   mock_server_replies_simple (
      request,
      "{'cursor' : {'id' : 123,'ns' : 'db.coll','firstBatch' : []},'ok' : 1 }");
   request_destroy (request);
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'getMore' : 123, 'collection' : 'coll' }");
   mock_server_replies_simple (request,
                               "{ 'cursor' : { 'nextBatch' : [] }, 'ok': 1 }");
   future_wait (future);
   ASSERT (!future_get_bool (future));
   ASSERT (!mongoc_change_stream_error_document (stream, NULL, NULL));
   ASSERT (next_doc == NULL);
   future_destroy (future);
   request_destroy (request);

   /* Another call to next should produce another getMore */
   future = future_change_stream_next (stream, &next_doc);
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'getMore' : 123, 'collection' : 'coll' }");
   mock_server_replies_simple (request,
                               "{ 'cursor' : { 'nextBatch' : [] }, 'ok': 1 }");
   future_wait (future);

   ASSERT (!future_get_bool (future));
   ASSERT (!mongoc_change_stream_error_document (stream, NULL, NULL));
   ASSERT (next_doc == NULL);
   future_destroy (future);
   request_destroy (request);

   DESTROY_CHANGE_STREAM ("123");

   /* Test non-empty pipeline */
   stream = mongoc_collection_watch (coll, nonempty_pipeline, NULL);
   future = future_change_stream_next (stream, &next_doc);

   request =
      mock_server_receives_command (server,
                                    "db",
                                    MONGOC_QUERY_SLAVE_OK,
                                    "{"
                                    "'aggregate' : 'coll',"
                                    "'pipeline' : "
                                    "   ["
                                    "      { '$changeStream':{ 'fullDocument' : 'default' } },"
                                    "      { '$project': { 'ns': false } }"
                                    "   ],"
                                    "'cursor' : {}"
                                    "}");
   mock_server_replies_simple (request, "{'cursor' : {'id' : 123, 'ns' : "
                                        "'db.coll','firstBatch' : []},'ok' : 1 "
                                        "}");
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'getMore' : 123, 'collection' : 'coll' }");
   mock_server_replies_simple (request,
                               "{ 'cursor' : { 'nextBatch' : [] }, 'ok': 1 }");
   future_wait (future);
   ASSERT (!future_get_bool (future));
   ASSERT (!mongoc_change_stream_error_document (stream, NULL, NULL));
   ASSERT (next_doc == NULL);
   future_destroy (future);
   request_destroy (request);

   DESTROY_CHANGE_STREAM ("123");

   mongoc_client_destroy (client);
   mongoc_collection_destroy (coll);
   mock_server_destroy (server);
}

/* 2. The watch helper must not throw a custom exception when executed against a
 * single server topology, but instead depend on a server error */
static void
test_change_stream_single_server ()
{
   mock_server_t *server;
   request_t *request;
   future_t *future;
   mongoc_client_t *client;
   mongoc_collection_t *coll;
   const bson_t *next_doc = NULL;
   bson_t empty_pipeline = BSON_INITIALIZER;
   mongoc_change_stream_t *stream;

   server = mock_server_with_autoismaster (5);
   mock_server_run (server);

   client = mongoc_client_new_from_uri (mock_server_get_uri (server));
   ASSERT (client);

   coll = mongoc_client_get_collection (client, "db", "coll");
   ASSERT (coll);

   stream = mongoc_collection_watch (coll, &empty_pipeline, NULL);
   future = future_change_stream_next (stream, &next_doc);

   request = mock_server_receives_command (server,
                                           "db",
                                           MONGOC_QUERY_SLAVE_OK,
                                           "{"
                                           "'aggregate' : 'coll',"
                                           "'pipeline' : "
                                           "   ["
                                           "      { '$changeStream':{ 'fullDocument' : 'default' } }"
                                           "   ],"
                                           "'cursor' : {}"
                                           "}");

   mock_server_replies_simple (request, "{'errmsg' : 'The $changeStream stage "
                                        "is only supported on replica sets', "
                                        "'code': 40573,'ok' : 0 }");
   future_wait (future);
   ASSERT (!future_get_bool (future));
   ASSERT (mongoc_change_stream_error_document (stream, NULL, NULL));
   ASSERT (next_doc == NULL);
   future_destroy (future);
   request_destroy (request);

   /* Destroying change stream will not issue a kill cursor. */
   future = future_change_stream_destroy (stream);
   future_wait (future);
   future_destroy (future);

   mongoc_client_destroy (client);
   mongoc_collection_destroy (coll);
   mock_server_destroy (server);
}


/* 3. ChangeStream must continuously track the last seen resumeToken */
static void
test_change_stream_track_resume_token ()
{
   mock_server_t *server;
   request_t *request;
   future_t *future;
   mongoc_client_t *client;
   mongoc_collection_t *coll;
   const bson_t *next_doc = NULL;
   bson_t empty_pipeline = BSON_INITIALIZER;
   mongoc_change_stream_t *stream;

   server = mock_server_with_autoismaster (5);
   mock_server_run (server);

   client = mongoc_client_new_from_uri (mock_server_get_uri (server));
      ASSERT (client);

   coll = mongoc_client_get_collection (client, "db", "coll");
      ASSERT (coll);

   stream = mongoc_collection_watch (coll, &empty_pipeline, NULL);
   future = future_change_stream_next (stream, &next_doc);

   request = mock_server_receives_command (server,
                                           "db",
                                           MONGOC_QUERY_SLAVE_OK,
                                           "{"
                                              "'aggregate' : 'coll',"
                                              "'pipeline' : "
                                              "   ["
                                              "      { '$changeStream':{ 'fullDocument' : 'default' } }"
                                              "   ],"
                                              "'cursor' : {}"
                                              "}");

   mock_server_replies_simple (
      request,
      "{'cursor' : {'id' : 123,'ns' : 'db.coll', 'firstBatch' : [ { '_id': { 'resumeToken': 'test_1' } } ]},'ok' : 1 }");
   future_wait (future);
      ASSERT (future_get_bool (future));
      ASSERT (!mongoc_change_stream_error_document (stream, NULL, NULL));
   ASSERT_MATCH(next_doc, "{ '_id': { 'resumeToken': 'test_1' } }");
   future_destroy (future);
   request_destroy (request);

   /* Get the next batched document */
   future = future_change_stream_next (stream, next_doc);
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'getMore' : 123, 'collection' : 'coll' }");
   mock_server_replies_simple (request,
                               "{ 'cursor' : { 'nextBatch' : [ { '_id': {'resumeToken': 'test_2' } } ] }, 'ok': 1 }");

   future_wait (future);
      ASSERT (future_get_bool (future));
      ASSERT (!mongoc_change_stream_error_document (stream, NULL, NULL));
   ASSERT_MATCH(next_doc, "{ '_id': {'resumeToken': 'test_2' } }");

   /* Have the client send the resumeAfter token by giving a resumable error. */
   future = future_change_stream_next(stream, &next_doc);
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'getMore' : 123, 'collection' : 'coll' }");
   mock_server_replies_simple (
      request, "{ 'code': 10107, 'errmsg': 'not master', 'ok': 0 }");

   request_destroy (request);

   /* Kill cursors will occur since cursor was created */

   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'killCursors' : 'coll', 'cursors' : [ 123 ] }");
   mock_server_replies_simple (request, "{ 'cursorsKilled': [123] }");

   request_destroy (request);

   request = mock_server_receives_command (server, "db", MONGOC_QUERY_SLAVE_OK, "{"
      "'aggregate' : 'coll',"
      "'pipeline' : "
      "   ["
      "      { '$changeStream':{ 'resumeAfter': { 'resumeToken': 'test_2' } } }"
      "   ],"
      "'cursor' : {}"
      "}");
   printf("2\n");
   mock_server_replies_simple (
      request,
      "{'cursor' : {'id' : 123,'ns' : 'db.coll', 'firstBatch' : [ { '_id': { 'resumeToken': 'test_3' } } ]},'ok' : 1 }");

   future_wait(future);
   future_destroy (future);
   request_destroy (request);

   DESTROY_CHANGE_STREAM ("123");

   mongoc_client_destroy (client);
   mongoc_collection_destroy (coll);
   mock_server_destroy (server);
}

/* 4. ChangeStream will throw an exception if the server response is missing the
 * resume token. (In the C driver case, return an error) */
static void
test_change_stream_missing_resume_token () {
   mock_server_t *server;
   request_t *request;
   future_t *future;
   mongoc_client_t *client;
   mongoc_collection_t *coll;
   const bson_t *next_doc = NULL;
   bson_t empty_pipeline = BSON_INITIALIZER;
   mongoc_change_stream_t *stream;

   server = mock_server_with_autoismaster (5);
   mock_server_run (server);

   client = mongoc_client_new_from_uri (mock_server_get_uri (server));
      ASSERT (client);

   coll = mongoc_client_get_collection (client, "db", "coll");
      ASSERT (coll);

   stream = mongoc_collection_watch (coll, &empty_pipeline, NULL);
   future = future_change_stream_next (stream, &next_doc);

   request = mock_server_receives_command (server,
                                           "db",
                                           MONGOC_QUERY_SLAVE_OK,
                                           "{"
                                              "'aggregate' : 'coll',"
                                              "'pipeline' : "
                                              "   ["
                                              "      { '$changeStream':{ 'fullDocument' : 'default' } }"
                                              "   ],"
                                              "'cursor' : {}"
                                              "}");

   mock_server_replies_simple (
      request,
      "{'cursor' : {'id' : 123,'ns' : 'db.coll', 'firstBatch' : [ { 'x': 0 } ]},'ok' : 1 }");
   future_wait (future);
      ASSERT (!future_get_bool (future));
      ASSERT (mongoc_change_stream_error_document (stream, NULL, NULL));
   ASSERT_MATCH(next_doc, "{'x': 0}");
   future_destroy (future);
   request_destroy (request);

   DESTROY_CHANGE_STREAM ("123");

   mongoc_client_destroy (client);
   mongoc_collection_destroy (coll);
   mock_server_destroy (server);
}

/* 5. ChangeStream will automatically resume one time on a resumable error
 * (including not master) with the initial pipeline and options, except for the
 * addition/update of a resumeToken
 * 9. The killCursors command sent during the “Resume Process” must not be
 * allowed to throw an exception.
 */
static void
test_change_stream_resumable_error ()
{
   mock_server_t *server;
   request_t *request;
   future_t *future;
   mongoc_client_t *client;
   bson_t empty = BSON_INITIALIZER;
   const bson_t *next_doc = NULL;

   server = mock_server_with_autoismaster (5);
   mock_server_run (server);

   client = mongoc_client_new_from_uri (mock_server_get_uri (server));
      ASSERT (client);

   mongoc_collection_t *coll =
      mongoc_client_get_collection (client, "db", "coll");


   mongoc_change_stream_t *stream =
      mongoc_collection_watch (coll, &empty, NULL);

   future = future_change_stream_next (stream, &next_doc);

   request = mock_server_receives_command (
      server, "db", MONGOC_QUERY_SLAVE_OK, "{ 'aggregate' : 'coll', 'pipeline' "
         ": [ { '$changeStream' : { 'fullDocument' : 'default' } } ], "
         "'cursor' : {  } }");

   mock_server_replies_simple (request, "{'cursor' : {'id' : 123, 'ns' : "
      "'db.coll','firstBatch' : []},'ok' : 1 "
      "}");

   request_destroy(request);
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'getMore' : 123, 'collection' : 'coll' }");
   mock_server_replies_simple (
      request, "{ 'code': 10107, 'errmsg': 'not master', 'ok': 0 }");
   request_destroy(request);
   /* On a resumable error, the change stream will first attempt to kill the
    * cursor and establish a new one with the same command.
    */

   /* Kill cursor */
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'killCursors' : 'coll', 'cursors' : [ 123 ] }");
   mock_server_replies_simple (request, "{ 'cursorsKilled': [123] }");

   /* Retry command */
   request = mock_server_receives_command (
      server, "db", MONGOC_QUERY_SLAVE_OK, "{ 'aggregate' : 'coll', 'pipeline' "
         ": [ { '$changeStream' : { 'fullDocument' : 'default' } } ], "
         "'cursor' : {  } }");
   mock_server_replies_simple (
      request,
      "{'cursor' : {'id' : 124,'ns' : 'db.coll','firstBatch' : []},'ok' : 1 }");
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'getMore' : 124, 'collection' : 'coll' }");
   mock_server_replies_simple (request,
                               "{ 'cursor' : { 'nextBatch' : [] }, 'ok': 1 }");

   future_wait (future);
      ASSERT (!future_get_bool (future));
      ASSERT (!mongoc_change_stream_error_document (stream, NULL, NULL));
      ASSERT (next_doc == NULL);

   /* Now test resumable error that occurs twice in a row */
   future = future_change_stream_next (stream, &next_doc);
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'getMore' : 124, 'collection' : 'coll' }");
   mock_server_replies_simple (
      request, "{ 'code': 10107, 'errmsg': 'not master', 'ok': 0 }");

   /* Kill cursor */
   mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'killCursors' : 'coll', 'cursors' : [ 124 ] }");
   mock_server_replies_simple (request, "{ 'cursorsKilled': [124] }");

   /* Retry command */
   request = mock_server_receives_command (
      server, "db", MONGOC_QUERY_SLAVE_OK, "{ 'aggregate' : 'coll', 'pipeline' "
         ": [ { '$changeStream' : { 'fullDocument' : 'default' } } ], "
         "'cursor' : {  } }");
   mock_server_replies_simple (request, "{'cursor' : {'id' : 125, 'ns' : "
      "'db.coll','firstBatch' : []},'ok' : 1 "
      "}");
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'getMore' : 125, 'collection' : 'coll' }");
   mock_server_replies_simple (
      request, "{ 'code': 10107, 'errmsg': 'not master', 'ok': 0 }");

   /* Check that error is returned */
   future_wait (future);
      ASSERT (!future_get_bool (future));
      ASSERT (mongoc_change_stream_error_document (stream, NULL, NULL));
      ASSERT (next_doc == NULL);

   future = future_change_stream_destroy (stream);

   mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'killCursors' : 'coll', 'cursors' : [ 125 ] }");
   mock_server_replies_simple (request, "{ 'cursorsKilled': [125] }");

   future_wait (future);

   future_destroy (future);
   request_destroy (request);
   mongoc_client_destroy (client);
   mock_server_destroy (server);
}

/* 6. ChangeStream will not attempt to resume on a server error */
static void
test_change_stream_nonresumable_error ()
{
   mock_server_t *server;
   request_t *request;
   future_t *future;
   mongoc_client_t *client;
   bson_t empty = BSON_INITIALIZER;
   const bson_t *next_doc = NULL;

   server = mock_server_with_autoismaster (5);
   mock_server_run (server);

   client = mongoc_client_new_from_uri (mock_server_get_uri (server));
      ASSERT (client);

   mongoc_collection_t *coll =
      mongoc_client_get_collection (client, "db", "coll");

   mongoc_change_stream_t *stream =
      mongoc_collection_watch (coll, &empty, NULL);

   future = future_change_stream_next (stream, &next_doc);

   request = mock_server_receives_command (
      server, "db", MONGOC_QUERY_SLAVE_OK, "{ 'aggregate' : 'coll', 'pipeline' "
         ": [ { '$changeStream' : { 'fullDocument' : 'default' } } ], "
         "'cursor' : {  } }");

   mock_server_replies_simple (request, "{'cursor' : {'id' : 123, 'ns' : "
      "'db.coll','firstBatch' : []},'ok' : 1 "
      "}");

   request_destroy(request);
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'getMore' : 123, 'collection' : 'coll' }");
   mock_server_replies_simple (
      request, "{ 'code': 1, 'errmsg': 'Internal Error', 'ok': 0 }");
   request_destroy(request);

   future_wait(future);

   ASSERT (!future_get_bool (future));
   ASSERT (mongoc_change_stream_error_document (stream, NULL, NULL));
   ASSERT (next_doc == NULL);

   mongoc_collection_destroy (coll);
   mongoc_client_destroy (client);
   mock_server_destroy (server);
}

/* 7. ChangeStream will perform server selection before attempting to resume,
 * using initial readPreference */
void test_change_stream_server_selection (void) {
   /* TODO: how should I check that the correct read preferences are being used? */
   printf("here\n");
//   mock_server_t *server;
//   request_t *request;
//   future_t *future;
//   mongoc_client_t *client;
//   bson_t empty = BSON_INITIALIZER;
//   const bson_t *next_doc = NULL;
//
//   server = mock_server_with_autoismaster (5);
//   mock_server_run (server);
//
//   client = mongoc_client_new_from_uri (mock_server_get_uri (server));
//      ASSERT (client);
//
//   mongoc_collection_t *coll =
//      mongoc_client_get_collection (client, "db", "coll");
//
//   mongoc_read_prefs_t* prefs = mongoc_read_prefs_copy(mongoc_collection_get_read_prefs(coll));
//   mongoc_read_prefs_set_mode (prefs, MONGOC_READ_SECONDARY);
//   mongoc_collection_set_read_prefs(coll, prefs);
//
//   mongoc_change_stream_t *stream =
//      mongoc_collection_watch (coll, &empty, NULL);
//
//   future = future_change_stream_next (stream, &next_doc);
//
//   request = mock_server_receives_command (
//      server, "db", MONGOC_QUERY_SLAVE_OK, "{ 'aggregate' : 'coll', 'pipeline' "
//         ": [ { '$changeStream' : { 'fullDocument' : 'default' } } ], "
//         "'cursor' : {  } }");
//
//   mock_server_replies_simple (request, "{'cursor' : {'id' : 123, 'ns' : "
//      "'db.coll','firstBatch' : []},'ok' : 1 "
//      "}");
//
//   request_destroy(request);
//   request = mock_server_receives_command (
//      server,
//      "db",
//      MONGOC_QUERY_SLAVE_OK,
//      "{ 'getMore' : 123, 'collection' : 'coll' }");
//   mock_server_replies_simple (
//      request, "{ 'code': 1, 'errmsg': 'Internal Error', 'ok': 0 }");
//   request_destroy(request);
//
//   future_wait(future);
//
//      ASSERT (!future_get_bool (future));
//      ASSERT (mongoc_change_stream_error_document (stream, NULL, NULL));
//      ASSERT (next_doc == NULL);
//
//   mongoc_collection_destroy (coll);
//   mongoc_client_destroy (client);
//   mock_server_destroy (server);
}

void test_change_stream_options (void)
{
   mock_server_t *server;
   request_t *request;
   future_t *future;
   mongoc_client_t *client;
   mongoc_collection_t *coll;
   const bson_t *next_doc = NULL;
   bson_t empty = BSON_INITIALIZER;

   server = mock_server_with_autoismaster (5);
   mock_server_run (server);

   client = mongoc_client_new_from_uri (mock_server_get_uri (server));
      ASSERT (client);

   coll = mongoc_client_get_collection (client, "db", "coll");
      ASSERT (coll);


   /*
    * fullDocument: 'default'|'updateLookup', passed to $changeStream stage
    * resumeAfter: optional<Doc>, passed to $changeStream stage
    * maxAwaitTimeMS: Optional<Int64>, passed to cursor
    * batchSize: Optional<Int32>, passed as agg option, {cursor: { batchSize: }}
    * collation: Optional<Document>, passed as agg option
    */

   /* fullDocument */
   bson_t *opts = BCON_NEW("fullDocument", "updateLookup");
   mongoc_change_stream_t *stream =
      mongoc_collection_watch (coll, &empty, opts);
   future = future_change_stream_next (stream, &next_doc);

   request = mock_server_receives_command (server,
                                           "db",
                                           MONGOC_QUERY_SLAVE_OK,
                                           "{"
                                              "'aggregate' : 'coll',"
                                              "'pipeline' : "
                                              "   ["
                                              "      { '$changeStream':{ 'fullDocument' : 'updateLookup' } }"
                                              "   ],"
                                              "'cursor' : { }"
                                              "}");

   mock_server_replies_simple (
      request,
      "{'cursor' : {'id' : 123,'ns' : 'db.coll','firstBatch' : []},'ok' : 1 }");
   request_destroy (request);
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'getMore' : 123, 'collection' : 'coll' }");
   mock_server_replies_simple (request,
                               "{ 'cursor' : { 'nextBatch' : [] }, 'ok': 1 }");
   request_destroy (request);

   future_wait (future);
      ASSERT (!future_get_bool (future));
      ASSERT (!mongoc_change_stream_error_document (stream, NULL, NULL));
      ASSERT (next_doc == NULL);

   DESTROY_CHANGE_STREAM ("123");
   bson_destroy(opts);

   /* resumeAfter */
   opts = BCON_NEW("resumeAfter", "{", "_id", BCON_UTF8("test_1"), "}");
   stream = mongoc_collection_watch (coll, &empty, opts);
   future = future_change_stream_next (stream, &next_doc);

   request = mock_server_receives_command (server,
                                           "db",
                                           MONGOC_QUERY_SLAVE_OK,
                                           "{"
                                              "'aggregate' : 'coll',"
                                              "'pipeline' : "
                                              "   ["
                                              "      { '$changeStream':{ 'fullDocument' : 'default', 'resumeAfter': {'_id': 'test_1'} } }"
                                              "   ],"
                                              "'cursor' : { }"
                                              "}");

   mock_server_replies_simple (
      request,
      "{'cursor' : {'id' : 123,'ns' : 'db.coll','firstBatch' : []},'ok' : 1 }");
   request_destroy (request);
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'getMore' : 123, 'collection' : 'coll' }");
   mock_server_replies_simple (request,
                               "{ 'cursor' : { 'nextBatch' : [] }, 'ok': 1 }");
   request_destroy (request);

   future_wait (future);
      ASSERT (!future_get_bool (future));
      ASSERT (!mongoc_change_stream_error_document (stream, NULL, NULL));
      ASSERT (next_doc == NULL);

   DESTROY_CHANGE_STREAM ("123");
   bson_destroy(opts);

   /* maxAwaitTimeMS */
   opts = BCON_NEW("maxAwaitTimeMS", BCON_INT64(5000));
   stream = mongoc_collection_watch (coll, &empty, opts);
   future = future_change_stream_next (stream, &next_doc);

   request = mock_server_receives_command (server,
                                           "db",
                                           MONGOC_QUERY_SLAVE_OK,
                                           "{"
                                              "'aggregate' : 'coll',"
                                              "'pipeline' : "
                                              "   ["
                                              "      { '$changeStream':{ 'fullDocument' : 'default' } }"
                                              "   ],"
                                              "'cursor' : { }"
                                              "}");

   mock_server_replies_simple (
      request,
      "{'cursor' : {'id' : 123,'ns' : 'db.coll','firstBatch' : []},'ok' : 1 }");
   request_destroy (request);
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'getMore' : 123, 'collection' : 'coll', 'maxTimeMS': 5000}");
   mock_server_replies_simple (request,
                               "{ 'cursor' : { 'nextBatch' : [] }, 'ok': 1 }");
   request_destroy (request);

   future_wait (future);
      ASSERT (!future_get_bool (future));
      ASSERT (!mongoc_change_stream_error_document (stream, NULL, NULL));
      ASSERT (next_doc == NULL);

   DESTROY_CHANGE_STREAM ("123");
   bson_destroy(opts);

   /* batchSize */
   opts = BCON_NEW("batchSize", BCON_INT32(10));
   stream = mongoc_collection_watch (coll, &empty, opts);
   future = future_change_stream_next (stream, &next_doc);

   request = mock_server_receives_command (server,
                                           "db",
                                           MONGOC_QUERY_SLAVE_OK,
                                           "{"
                                              "'aggregate' : 'coll',"
                                              "'pipeline' : "
                                              "   ["
                                              "      { '$changeStream':{ 'fullDocument' : 'default' } }"
                                              "   ],"
                                              "'cursor' : { 'batchSize': 10 }"
                                              "}");

   mock_server_replies_simple (
      request,
      "{'cursor' : {'id' : 123,'ns' : 'db.coll','firstBatch' : []},'ok' : 1 }");
   request_destroy (request);
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'getMore' : 123, 'collection' : 'coll' }");
   mock_server_replies_simple (request,
                               "{ 'cursor' : { 'nextBatch' : [] }, 'ok': 1 }");
   request_destroy (request);

   future_wait (future);
      ASSERT (!future_get_bool (future));
      ASSERT (!mongoc_change_stream_error_document (stream, NULL, NULL));
      ASSERT (next_doc == NULL);

   DESTROY_CHANGE_STREAM ("123");


   /* collation */
   opts = BCON_NEW("collation", "{", "locale", BCON_UTF8("en"), "}");
   stream = mongoc_collection_watch (coll, &empty, opts);
   future = future_change_stream_next (stream, &next_doc);

   request = mock_server_receives_command (server,
                                           "db",
                                           MONGOC_QUERY_SLAVE_OK,
                                           "{"
                                              "'aggregate' : 'coll',"
                                              "'pipeline' : "
                                              "   ["
                                              "      { '$changeStream':{ 'fullDocument' : 'default' } }"
                                              "   ],"
                                              "'cursor' : {},"
                                              "'collation': { 'locale': 'en' }"
                                              "}");

   mock_server_replies_simple (
      request,
      "{'cursor' : {'id' : 123,'ns' : 'db.coll','firstBatch' : []},'ok' : 1 }");
   request_destroy (request);
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{ 'getMore' : 123, 'collection' : 'coll' }");
   mock_server_replies_simple (request,
                               "{ 'cursor' : { 'nextBatch' : [] }, 'ok': 1 }");
   request_destroy (request);

   future_wait (future);
      ASSERT (!future_get_bool (future));
      ASSERT (!mongoc_change_stream_error_document (stream, NULL, NULL));
      ASSERT (next_doc == NULL);

   DESTROY_CHANGE_STREAM ("123");


   mongoc_collection_destroy (coll);
   mongoc_client_destroy (client);
   mock_server_destroy (server);
}

void
test_change_stream_install (TestSuite *testSuite)
{
   TestSuite_AddMockServerTest (
      testSuite, "/changestream/pipeline", test_change_stream_pipeline);

   TestSuite_AddMockServerTest (testSuite,
                                "/changestream/single_server",
                                test_change_stream_single_server);

   TestSuite_AddMockServerTest (testSuite,
                                "/changestream/track_resume_token",
                                test_change_stream_track_resume_token);

   TestSuite_AddMockServerTest (testSuite,
                                "/changestream/missing_resume_token",
                                test_change_stream_missing_resume_token);

   TestSuite_AddMockServerTest (testSuite,
                                "/changestream/resumable_error",
                                test_change_stream_resumable_error);

   TestSuite_AddMockServerTest (testSuite,
                                "/changestream/nonresumable_error",
                                test_change_stream_nonresumable_error);

   /* TODO */
   TestSuite_AddMockServerTest (testSuite,
                                "/changestream/server_selection",
                                test_change_stream_server_selection);

   TestSuite_AddMockServerTest (testSuite,
                               "/changestream/options",
                               test_change_stream_options);

//   TestSuite_AddMockServerTest (testSuite,
//                               "/changestream/test_change_stream_initial_empty_batch",
//                               test_change_stream_initial_empty_batch);

//   TestSuite_AddMockServerTest (
//      testSuite, "/changestream/watch", test_change_stream_watch);
//


   // TestSuite_AddMockServerTest (testSuite, "/changestream/playing",
   // test_example);
}