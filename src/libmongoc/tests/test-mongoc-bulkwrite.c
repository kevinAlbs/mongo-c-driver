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
   mongoc_bulkwriteoptions_t *opts = mongoc_bulkwriteoptions_new ();
   mongoc_bulkwriteoptions_set_verboseresults (opts, true);
   mongoc_bulkwritereturn_t bwr = mongoc_bulkwrite_execute (bw, opts);

   ASSERT_NO_BULKWRITEEXCEPTION (bwr);

   // Ensure results report IDs inserted.
   {
      ASSERT (bwr.res);
      const bson_t *insertResults = mongoc_bulkwriteresult_insertresults (bwr.res);
      ASSERT_MATCH (insertResults, BSON_STR ({"0" : {"insertedId" : 123}, "1" : {"insertedId" : 456}}));
   }

   mongoc_bulkwriteexception_destroy (bwr.exc);
   mongoc_bulkwriteresult_destroy (bwr.res);
   mongoc_bulkwrite_destroy (bw);
   mongoc_bulkwriteoptions_destroy (opts);
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
   mongoc_bulkwriteoptions_t *opts = mongoc_bulkwriteoptions_new ();
   mongoc_bulkwriteoptions_set_verboseresults (opts, true);
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
   ASSERT_MATCH (insertResults, BSON_STR ({"0" : {"insertedId" : 123}}));

   mongoc_bulkwriteexception_destroy (bwr.exc);
   mongoc_bulkwriteresult_destroy (bwr.res);
   mongoc_bulkwrite_destroy (bw);
   mongoc_bulkwriteoptions_destroy (opts);
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
   mongoc_bulkwriteoptions_t *opts = mongoc_bulkwriteoptions_new ();
   mongoc_bulkwriteoptions_set_writeconcern (opts, wc);

   ok = mongoc_bulkwrite_append_insertone (bw, "db.coll", -1, tmp_bson ("{}"), NULL, &error);
   ASSERT_OR_PRINT (ok, error);
   mongoc_bulkwritereturn_t ret = mongoc_bulkwrite_execute (bw, opts);
   // Expect no result.
   ASSERT (!ret.res);
   ASSERT_NO_BULKWRITEEXCEPTION (ret);
   mongoc_bulkwriteresult_destroy (ret.res);
   mongoc_bulkwriteexception_destroy (ret.exc);
   mongoc_bulkwriteoptions_destroy (opts);
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
   mongoc_bulkwriteoptions_t *opts = mongoc_bulkwriteoptions_new ();
   mongoc_bulkwriteoptions_set_writeconcern (opts, wc);
   mongoc_bulkwriteoptions_set_session (opts, session);

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
   mongoc_bulkwriteoptions_destroy (opts);
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
   mongoc_bulkwriteoptions_t *bwo = mongoc_bulkwriteoptions_new ();
   mongoc_bulkwriteoptions_set_serverid (bwo, selected_serverid);

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

   mongoc_bulkwriteoptions_destroy (bwo);
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

   mongoc_bulkwriteoptions_t *bwo = mongoc_bulkwriteoptions_new ();
   // Create bulk write options with extra options.
   {
      bson_t *extra = tmp_bson ("{'comment': 'foo'}");
      mongoc_bulkwriteoptions_set_extra (bwo, extra);
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

   mongoc_bulkwriteoptions_destroy (bwo);
   mongoc_bulkwrite_destroy (bw);
   bson_destroy (&last_captured);
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
}
