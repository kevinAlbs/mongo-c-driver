/*
 * Copyright 2024-present MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <mongoc/mongoc.h>
#include <test-libmongoc.h>
#include <TestSuite.h>
#include <test-conveniences.h>
#include <mongoc-bulkwrite.h>

static void
test_bulkwrite_insert (void *unused)
{
   BSON_UNUSED (unused);

   bson_error_t error;
   bool ok;
   mongoc_client_t *client = test_framework_new_default_client ();

   // Drop prior data.
   {
      mongoc_collection_t *coll = mongoc_client_get_collection (client, "db", "coll");
      mongoc_collection_drop (coll, NULL); // Ignore return.
      mongoc_collection_destroy (coll);
   }

   // Insert two documents with verbose results.
   mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);
   ok = mongoc_bulkwrite_append_insertone (bw, "db.coll", -1, tmp_bson ("{'_id': 123}"), NULL /* opts */, &error);
   ASSERT_OR_PRINT (ok, error);
   ok = mongoc_bulkwrite_append_insertone (bw, "db.coll", -1, tmp_bson ("{'_id': 456}"), NULL /* opts */, &error);
   ASSERT_OR_PRINT (ok, error);

   // Do the bulk write.
   mongoc_bulkwriteopts_t *opts = mongoc_bulkwriteopts_new ();
   mongoc_bulkwriteopts_set_verboseresults (opts, true);
   mongoc_bulkwritereturn_t bwr = mongoc_bulkwrite_execute (bw, opts);

   ASSERT_NO_BULKWRITEEXCEPTION (bwr);

   // Ensure results report IDs inserted.
   {
      ASSERT (bwr.res);
      const bson_t *insertResults = mongoc_bulkwriteresult_insertresults (bwr.res);
      ASSERT (insertResults);
      ASSERT_MATCH (insertResults, BSON_STR ({"0" : {"insertedId" : 123}, "1" : {"insertedId" : 456}}));
   }

   mongoc_bulkwriteexception_destroy (bwr.exc);
   mongoc_bulkwriteresult_destroy (bwr.res);
   mongoc_bulkwrite_destroy (bw);
   mongoc_bulkwriteopts_destroy (opts);
   mongoc_client_destroy (client);
}

static void
test_bulkwrite_writeError (void *unused)
{
   BSON_UNUSED (unused);

   bson_error_t error;
   bool ok;
   mongoc_client_t *client = test_framework_new_default_client ();

   // Drop prior data.
   {
      mongoc_collection_t *coll = mongoc_client_get_collection (client, "db", "coll");
      mongoc_collection_drop (coll, NULL); // Ignore return.
      mongoc_collection_destroy (coll);
   }

   // Insert two documents with verbose results.
   mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);
   ok = mongoc_bulkwrite_append_insertone (bw, "db.coll", -1, tmp_bson ("{'_id': 123}"), NULL /* opts */, &error);
   ASSERT_OR_PRINT (ok, error);
   ok = mongoc_bulkwrite_append_insertone (bw, "db.coll", -1, tmp_bson ("{'_id': 123}"), NULL /* opts */, &error);
   ASSERT_OR_PRINT (ok, error);

   // Do the bulk write.
   mongoc_bulkwriteopts_t *opts = mongoc_bulkwriteopts_new ();
   mongoc_bulkwriteopts_set_verboseresults (opts, true);
   mongoc_bulkwritereturn_t bwr = mongoc_bulkwrite_execute (bw, opts);

   // Expect an error.
   ASSERT (bwr.exc);
   const bson_t *ed = mongoc_bulkwriteexception_writeerrors (bwr.exc);
   ASSERT_MATCH (ed, BSON_STR ({
                    "1" : {
                       "code" : 11000,
                       "message" : "E11000 duplicate key error collection: db.coll index: _id_ dup key: { _id: 123 }",
                       "details" : {}
                    }
                 }));

   // Ensure results report only one ID inserted.
   ASSERT (bwr.res);
   const bson_t *insertResults = mongoc_bulkwriteresult_insertresults (bwr.res);
   ASSERT (insertResults);
   ASSERT_MATCH (insertResults, BSON_STR ({"0" : {"insertedId" : 123}}));

   mongoc_bulkwriteexception_destroy (bwr.exc);
   mongoc_bulkwriteresult_destroy (bwr.res);
   mongoc_bulkwrite_destroy (bw);
   mongoc_bulkwriteopts_destroy (opts);
   mongoc_client_destroy (client);
}

static void
test_bulkwrite_unacknowledged (void *ctx)
{
   mongoc_client_t *client;
   BSON_UNUSED (ctx);
   bool ok;
   bson_error_t error;
   mongoc_write_concern_t *wc = mongoc_write_concern_new ();

   mongoc_write_concern_set_w (wc, MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED);

   client = test_framework_new_default_client ();
   mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);
   mongoc_bulkwriteopts_t *opts = mongoc_bulkwriteopts_new ();
   mongoc_bulkwriteopts_set_writeconcern (opts, wc);

   ok = mongoc_bulkwrite_append_insertone (bw, "db.coll", -1, tmp_bson ("{}"), NULL, &error);
   ASSERT_OR_PRINT (ok, error);
   mongoc_bulkwritereturn_t ret = mongoc_bulkwrite_execute (bw, opts);
   // Expect no result.
   ASSERT (!ret.res);
   ASSERT_NO_BULKWRITEEXCEPTION (ret);
   mongoc_bulkwriteresult_destroy (ret.res);
   mongoc_bulkwriteexception_destroy (ret.exc);
   mongoc_bulkwriteopts_destroy (opts);
   mongoc_bulkwrite_destroy (bw);
   mongoc_client_destroy (client);
   mongoc_write_concern_destroy (wc);
}

static void
test_bulkwrite_session_with_unacknowledged (void *ctx)
{
   mongoc_client_t *client;
   BSON_UNUSED (ctx);
   bool ok;
   bson_error_t error;
   mongoc_write_concern_t *wc = mongoc_write_concern_new ();

   mongoc_write_concern_set_w (wc, MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED);

   client = test_framework_new_default_client ();
   mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);

   mongoc_client_session_t *session = mongoc_client_start_session (client, NULL, &error);
   ASSERT_OR_PRINT (session, error);
   mongoc_bulkwriteopts_t *opts = mongoc_bulkwriteopts_new ();
   mongoc_bulkwriteopts_set_writeconcern (opts, wc);
   mongoc_bulkwriteopts_set_session (opts, session);

   ok = mongoc_bulkwrite_append_insertone (bw, "db.coll", -1, tmp_bson ("{}"), NULL, &error);
   ASSERT_OR_PRINT (ok, error);
   mongoc_bulkwritereturn_t ret = mongoc_bulkwrite_execute (bw, opts);
   // Expect no result.
   ASSERT (!ret.res);
   ASSERT (ret.exc);
   ASSERT (mongoc_bulkwriteexception_error (ret.exc, &error));
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_COMMAND,
                          MONGOC_ERROR_COMMAND_INVALID_ARG,
                          "Cannot use client session with unacknowledged command");
   mongoc_bulkwriteresult_destroy (ret.res);
   mongoc_bulkwriteexception_destroy (ret.exc);
   mongoc_client_session_destroy (session);
   mongoc_bulkwriteopts_destroy (opts);
   mongoc_bulkwrite_destroy (bw);
   mongoc_client_destroy (client);
   mongoc_write_concern_destroy (wc);
}

static void
test_bulkwrite_double_execute (void *ctx)
{
   mongoc_client_t *client;
   BSON_UNUSED (ctx);
   bool ok;
   bson_error_t error;

   client = test_framework_new_default_client ();
   mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);
   ok = mongoc_bulkwrite_append_insertone (bw, "db.coll", -1, tmp_bson ("{}"), NULL, &error);
   ASSERT_OR_PRINT (ok, error);
   // Execute.
   {
      mongoc_bulkwritereturn_t bwr = mongoc_bulkwrite_execute (bw, NULL);
      ASSERT_NO_BULKWRITEEXCEPTION (bwr);
      mongoc_bulkwriteresult_destroy (bwr.res);
      mongoc_bulkwriteexception_destroy (bwr.exc);
   }

   // Expect an error on reuse.
   ASSERT (!mongoc_bulkwrite_append_insertone (bw, "db.coll", -1, tmp_bson ("{}"), NULL, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "bulk write already executed");
   memset (&error, 0, sizeof (error));

   ASSERT (!mongoc_bulkwrite_append_updateone (bw, "db.coll", -1, tmp_bson ("{}"), tmp_bson ("{}"), NULL, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "bulk write already executed");
   memset (&error, 0, sizeof (error));

   ASSERT (!mongoc_bulkwrite_append_updatemany (bw, "db.coll", -1, tmp_bson ("{}"), tmp_bson ("{}"), NULL, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "bulk write already executed");
   memset (&error, 0, sizeof (error));

   ASSERT (!mongoc_bulkwrite_append_replaceone (bw, "db.coll", -1, tmp_bson ("{}"), tmp_bson ("{}"), NULL, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "bulk write already executed");
   memset (&error, 0, sizeof (error));

   ASSERT (!mongoc_bulkwrite_append_deleteone (bw, "db.coll", -1, tmp_bson ("{}"), NULL, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "bulk write already executed");
   memset (&error, 0, sizeof (error));

   ASSERT (!mongoc_bulkwrite_append_deletemany (bw, "db.coll", -1, tmp_bson ("{}"), NULL, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "bulk write already executed");
   memset (&error, 0, sizeof (error));

   {
      mongoc_bulkwritereturn_t bwr = mongoc_bulkwrite_execute (bw, NULL);
      ASSERT (bwr.exc);
      ASSERT (mongoc_bulkwriteexception_error (bwr.exc, &error));
      ASSERT_ERROR_CONTAINS (
         error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "bulk write already executed");
      mongoc_bulkwriteexception_destroy (bwr.exc);
      mongoc_bulkwriteresult_destroy (bwr.res);
   }

   mongoc_bulkwrite_destroy (bw);
   mongoc_client_destroy (client);
}

static void
capture_last_bulkWrite_serverid (const mongoc_apm_command_started_t *event)
{
   if (0 == strcmp (mongoc_apm_command_started_get_command_name (event), "bulkWrite")) {
      uint32_t *last_captured = mongoc_apm_command_started_get_context (event);
      *last_captured = mongoc_apm_command_started_get_server_id (event);
   }
}

static void
test_bulkwrite_serverid (void *ctx)
{
   mongoc_client_t *client;
   BSON_UNUSED (ctx);
   bool ok;
   bson_error_t error;

   client = test_framework_new_default_client ();

   // Get a server ID
   uint32_t selected_serverid;
   {
      mongoc_server_description_t *sd = mongoc_client_select_server (client, true /* for_writes */, NULL, &error);
      ASSERT_OR_PRINT (sd, error);
      selected_serverid = mongoc_server_description_id (sd);
      mongoc_server_description_destroy (sd);
   }

   uint32_t last_captured = 0;
   // Set callback to capture the serverid used for the last `bulkWrite` command.
   {
      mongoc_apm_callbacks_t *cbs = mongoc_apm_callbacks_new ();
      mongoc_apm_set_command_started_cb (cbs, capture_last_bulkWrite_serverid);
      mongoc_client_set_apm_callbacks (client, cbs, &last_captured);
      mongoc_apm_callbacks_destroy (cbs);
   }


   mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);
   mongoc_bulkwriteopts_t *bwo = mongoc_bulkwriteopts_new ();
   mongoc_bulkwriteopts_set_serverid (bwo, selected_serverid);

   ok = mongoc_bulkwrite_append_insertone (bw, "db.coll", -1, tmp_bson ("{}"), NULL, &error);
   ASSERT_OR_PRINT (ok, error);
   // Execute.
   {
      mongoc_bulkwritereturn_t bwr = mongoc_bulkwrite_execute (bw, bwo);
      ASSERT_NO_BULKWRITEEXCEPTION (bwr);
      // Expect used the same server ID.
      uint32_t used_serverid = mongoc_bulkwriteresult_serverid (bwr.res);
      ASSERT_CMPUINT32 (selected_serverid, ==, used_serverid);
      mongoc_bulkwriteresult_destroy (bwr.res);
      mongoc_bulkwriteexception_destroy (bwr.exc);
   }

   ASSERT_CMPUINT32 (last_captured, ==, selected_serverid);

   mongoc_bulkwriteopts_destroy (bwo);
   mongoc_bulkwrite_destroy (bw);
   mongoc_client_destroy (client);
}

static void
capture_last_bulkWrite_command (const mongoc_apm_command_started_t *event)
{
   if (0 == strcmp (mongoc_apm_command_started_get_command_name (event), "bulkWrite")) {
      bson_t *last_captured = mongoc_apm_command_started_get_context (event);
      bson_destroy (last_captured);
      const bson_t *cmd = mongoc_apm_command_started_get_command (event);
      bson_copy_to (cmd, last_captured);
   }
}

static void
test_bulkwrite_extra (void *ctx)
{
   mongoc_client_t *client;
   BSON_UNUSED (ctx);
   bool ok;
   bson_error_t error;

   client = test_framework_new_default_client ();

   bson_t last_captured = BSON_INITIALIZER;
   // Set callback to capture the last `bulkWrite` command.
   {
      mongoc_apm_callbacks_t *cbs = mongoc_apm_callbacks_new ();
      mongoc_apm_set_command_started_cb (cbs, capture_last_bulkWrite_command);
      mongoc_client_set_apm_callbacks (client, cbs, &last_captured);
      mongoc_apm_callbacks_destroy (cbs);
   }

   mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);
   // Create bulk write.
   {
      ok = mongoc_bulkwrite_append_insertone (bw, "db.coll", -1, tmp_bson ("{}"), NULL, &error);
      ASSERT_OR_PRINT (ok, error);
   }

   mongoc_bulkwriteopts_t *bwo = mongoc_bulkwriteopts_new ();
   // Create bulk write options with extra options.
   {
      bson_t *extra = tmp_bson ("{'comment': 'foo'}");
      mongoc_bulkwriteopts_set_extra (bwo, extra);
   }

   // Execute.
   {
      mongoc_bulkwritereturn_t bwr = mongoc_bulkwrite_execute (bw, bwo);
      ASSERT_NO_BULKWRITEEXCEPTION (bwr);
      mongoc_bulkwriteresult_destroy (bwr.res);
      mongoc_bulkwriteexception_destroy (bwr.exc);
   }

   // Expect `bulkWrite` command was sent with extra option.
   ASSERT_MATCH (&last_captured, "{'comment': 'foo'}");

   mongoc_bulkwriteopts_destroy (bwo);
   mongoc_bulkwrite_destroy (bw);
   bson_destroy (&last_captured);
   mongoc_client_destroy (client);
}

static void
test_bulkwrite_no_verbose_results (void *ctx)
{
   mongoc_client_t *client;
   BSON_UNUSED (ctx);
   bool ok;
   bson_error_t error;

   client = test_framework_new_default_client ();

   mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);
   // Create bulk write.
   {
      ok = mongoc_bulkwrite_append_insertone (bw, "db.coll", -1, tmp_bson ("{}"), NULL, &error);
      ASSERT_OR_PRINT (ok, error);

      ok = mongoc_bulkwrite_append_updateone (
         bw, "db.coll", -1, tmp_bson ("{}"), tmp_bson ("{'$set': {'x': 1}}"), NULL, &error);
      ASSERT_OR_PRINT (ok, error);

      ok = mongoc_bulkwrite_append_deleteone (bw, "db.coll", -1, tmp_bson ("{}"), NULL, &error);
      ASSERT_OR_PRINT (ok, error);
   }


   // Execute.
   {
      mongoc_bulkwritereturn_t bwr = mongoc_bulkwrite_execute (bw, NULL /* opts */);
      ASSERT_NO_BULKWRITEEXCEPTION (bwr);
      // Expect no verbose results.
      ASSERT (NULL == mongoc_bulkwriteresult_insertresults (bwr.res));
      ASSERT (NULL == mongoc_bulkwriteresult_updateresults (bwr.res));
      ASSERT (NULL == mongoc_bulkwriteresult_deleteresults (bwr.res));
      mongoc_bulkwriteresult_destroy (bwr.res);
      mongoc_bulkwriteexception_destroy (bwr.exc);
   }

   mongoc_bulkwrite_destroy (bw);
   mongoc_client_destroy (client);
}

static void
capture_all_bulkWrite_commands (const mongoc_apm_command_started_t *event)
{
   if (0 == strcmp (mongoc_apm_command_started_get_command_name (event), "bulkWrite")) {
      mongoc_array_t *captured = mongoc_apm_command_started_get_context (event);
      bson_t *cmd = bson_copy (mongoc_apm_command_started_get_command (event));
      _mongoc_array_append_val (captured, cmd);
   }
}

static void
test_bulkwrite_splits_nsinfo (void *ctx)
{
   mongoc_client_t *client;
   BSON_UNUSED (ctx);
   bool ok;
   bson_error_t error;

   client = test_framework_new_default_client ();

   // Get `maxWriteBatchSize` from the server.
   int32_t maxWriteBatchSize;
   {
      bson_t reply;

      ok = mongoc_client_command_simple (client, "admin", tmp_bson ("{'hello': 1}"), NULL, &reply, &error);
      ASSERT_OR_PRINT (ok, error);

      maxWriteBatchSize = bson_lookup_int32 (&reply, "maxWriteBatchSize");
      bson_destroy (&reply);
   }

   mongoc_array_t captured;
   _mongoc_array_init (&captured, sizeof (bson_t *));
   // Set callback to capture all `bulkWrite` commands.
   {
      mongoc_apm_callbacks_t *cbs = mongoc_apm_callbacks_new ();
      mongoc_apm_set_command_started_cb (cbs, capture_all_bulkWrite_commands);
      mongoc_client_set_apm_callbacks (client, cbs, &captured);
      mongoc_apm_callbacks_destroy (cbs);
   }

   mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);
   // Create bulk write large enough to split into two batches.
   {
      // First batch only references db.coll1.
      for (int32_t i = 0; i < maxWriteBatchSize; i++) {
         ok = mongoc_bulkwrite_append_insertone (bw, "db.coll1", -1, tmp_bson ("{}"), NULL, &error);
         ASSERT_OR_PRINT (ok, error);
      }
      // Second batch only references db.coll2.
      for (int32_t i = 0; i < 1; i++) {
         ok = mongoc_bulkwrite_append_insertone (bw, "db.coll2", -1, tmp_bson ("{}"), NULL, &error);
         ASSERT_OR_PRINT (ok, error);
      }
   }


   // Execute.
   {
      mongoc_bulkwritereturn_t bwr = mongoc_bulkwrite_execute (bw, NULL /* opts */);
      ASSERT_NO_BULKWRITEEXCEPTION (bwr);
      ASSERT_CMPINT64 (mongoc_bulkwriteresult_insertedcount (bwr.res), ==, maxWriteBatchSize + 1);
      mongoc_bulkwriteresult_destroy (bwr.res);
      mongoc_bulkwriteexception_destroy (bwr.exc);
   }

   // Expect two `bulkWrite` commands were sent.
   ASSERT_CMPSIZE_T (captured.len, ==, 2);
   bson_t *first = _mongoc_array_index (&captured, bson_t *, 0);
   // Expect the first only contains the namespace for `db.coll1`.
   ASSERT_MATCH (first, BSON_STR ({"nsInfo" : [ {"ns" : "db.coll1"} ]}));
   // Expect the second only contains the namespace for `db.coll2`.
   bson_t *second = _mongoc_array_index (&captured, bson_t *, 1);
   // Expect the first only contains the namespace for `db.coll1`.
   ASSERT_MATCH (second, BSON_STR ({"nsInfo" : [ {"ns" : "db.coll2"} ]}));

   for (size_t i = 0; i < captured.len; i++) {
      bson_t *el = _mongoc_array_index (&captured, bson_t *, i);
      bson_destroy (el);
   }
   _mongoc_array_destroy (&captured);

   mongoc_bulkwrite_destroy (bw);
   mongoc_client_destroy (client);
}

static int
skip_because_slow (void)
{
   printf ("Skipping test due to being slow\n");
   return 0;
}

static void
test_bulkwrite_many_namespaces (void *ctx)
{
   mongoc_client_t *client;
   BSON_UNUSED (ctx);
   bool ok;
   bson_error_t error;

   client = test_framework_new_default_client ();

   mongoc_array_t captured;
   _mongoc_array_init (&captured, sizeof (bson_t *));
   // Set callback to capture all `bulkWrite` commands.
   {
      mongoc_apm_callbacks_t *cbs = mongoc_apm_callbacks_new ();
      mongoc_apm_set_command_started_cb (cbs, capture_all_bulkWrite_commands);
      mongoc_client_set_apm_callbacks (client, cbs, &captured);
      mongoc_apm_callbacks_destroy (cbs);
   }

   // Get `maxWriteBatchSize` from the server.
   int32_t maxWriteBatchSize;
   {
      bson_t reply;

      ok = mongoc_client_command_simple (client, "admin", tmp_bson ("{'hello': 1}"), NULL, &reply, &error);
      ASSERT_OR_PRINT (ok, error);

      maxWriteBatchSize = bson_lookup_int32 (&reply, "maxWriteBatchSize");
      bson_destroy (&reply);
   }

   mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);
   // Create bulk write large enough to split into two batches. Use a unique namespace per model.
   {
      for (int32_t i = 0; i < maxWriteBatchSize + 1; i++) {
         char *ns = bson_strdup_printf ("db.coll%" PRId32, i);
         ok = mongoc_bulkwrite_append_deleteone (bw, ns, -1, tmp_bson ("{}"), NULL, &error);
         ASSERT_OR_PRINT (ok, error);
         bson_free (ns);
      }
   }

   // Execute.
   {
      mongoc_bulkwritereturn_t bwr = mongoc_bulkwrite_execute (bw, NULL /* opts */);
      ASSERT_NO_BULKWRITEEXCEPTION (bwr);
      ASSERT_CMPINT64 (mongoc_bulkwriteresult_insertedcount (bwr.res), ==, 0);
      mongoc_bulkwriteresult_destroy (bwr.res);
      mongoc_bulkwriteexception_destroy (bwr.exc);
   }


   // Expect two `bulkWrite` commands were sent.
   ASSERT_CMPSIZE_T (captured.len, ==, 2);
   bson_t *first = _mongoc_array_index (&captured, bson_t *, 0);
   // Expect the first contains maxWriteBatchSize `nsInfo` entries:
   {
      bson_t *nsInfo = bson_lookup_bson (first, "nsInfo");
      ASSERT_CMPUINT32 (bson_count_keys (nsInfo), ==, maxWriteBatchSize);
      bson_destroy (nsInfo);
   }
   // Expect the second only contains one `nsInfo` entry:
   bson_t *second = _mongoc_array_index (&captured, bson_t *, 1);
   {
      bson_t *nsInfo = bson_lookup_bson (second, "nsInfo");
      ASSERT_CMPUINT32 (bson_count_keys (nsInfo), ==, 1);
      bson_destroy (nsInfo);
   }

   for (size_t i = 0; i < captured.len; i++) {
      bson_t *el = _mongoc_array_index (&captured, bson_t *, i);
      bson_destroy (el);
   }
   _mongoc_array_destroy (&captured);

   mongoc_bulkwrite_destroy (bw);
   mongoc_client_destroy (client);
}


void
test_bulkwrite_install (TestSuite *suite)
{
   TestSuite_AddFull (suite,
                      "/bulkwrite/insert",
                      test_bulkwrite_insert,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      test_framework_skip_if_max_wire_version_less_than_25 // require server 8.0
   );

   TestSuite_AddFull (suite,
                      "/bulkwrite/writeError",
                      test_bulkwrite_writeError,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      test_framework_skip_if_max_wire_version_less_than_25 // require server 8.0
   );

   TestSuite_AddFull (suite,
                      "/bulkwrite/unacknowledged",
                      test_bulkwrite_unacknowledged,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      test_framework_skip_if_max_wire_version_less_than_25 // require server 8.0
   );

   TestSuite_AddFull (suite,
                      "/bulkwrite/session_with_unacknowledged",
                      test_bulkwrite_session_with_unacknowledged,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      test_framework_skip_if_max_wire_version_less_than_25 // require server 8.0
   );

   TestSuite_AddFull (suite,
                      "/bulkwrite/double_execute",
                      test_bulkwrite_double_execute,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      test_framework_skip_if_max_wire_version_less_than_25 // require server 8.0
   );

   TestSuite_AddFull (suite,
                      "/bulkwrite/server_id",
                      test_bulkwrite_serverid,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      test_framework_skip_if_max_wire_version_less_than_25 // require server 8.0
   );

   TestSuite_AddFull (suite,
                      "/bulkwrite/extra",
                      test_bulkwrite_extra,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      test_framework_skip_if_max_wire_version_less_than_25 // require server 8.0
   );

   TestSuite_AddFull (suite,
                      "/bulkwrite/no_verbose_results",
                      test_bulkwrite_no_verbose_results,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      test_framework_skip_if_max_wire_version_less_than_25 // require server 8.0
   );

   TestSuite_AddFull (suite,
                      "/bulkwrite/splits_nsinfo",
                      test_bulkwrite_splits_nsinfo,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      test_framework_skip_if_max_wire_version_less_than_25 // require server 8.0
   );

   TestSuite_AddFull (suite,
                      "/bulkwrite/many_namespaces",
                      test_bulkwrite_many_namespaces,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      test_framework_skip_if_max_wire_version_less_than_25, // require server 8.0
                      skip_because_slow);
}
