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

   // Ensure no error.
   if (bwr.exc) {
      const char *msg = "(none)";
      if (mongoc_bulkwriteexception_error (bwr.exc, &error)) {
         msg = error.message;
      }
      const bson_t *doc = mongoc_bulkwriteexception_error_document (bwr.exc);
      test_error ("Expected no bulk write exception, but got:\n"
                  "  Error     : %s\n"
                  "  Document  : %s",
                  msg,
                  tmp_json (doc));
   }

   // Ensure results report IDs inserted.
   {
      ASSERT (bwr.res);
      const bson_t *vr = mongoc_bulkwriteresult_verboseresults (bwr.res);
      ASSERT (vr);
      ASSERT_MATCH (vr, BSON_STR ({"insertResults" : {"0" : {"insertedId" : 123}, "1" : {"insertedId" : 456}}}));
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
   const bson_t *ed = mongoc_bulkwriteexception_error_document (bwr.exc);
   ASSERT_MATCH (
      ed, BSON_STR ({
         "errorLabels" : [],
         "writeErrors" : {
            "1" : {
               "code" : 11000,
               "message" : "E11000 duplicate key error collection: db.coll index: _id_ dup key: { _id: 123 }",
               "details" : {}
            }
         },
         "writeConcernErrors" : [],
         "errorReply" : {}
      }));

   // Ensure results report only one ID inserted.
   {
      ASSERT (bwr.res);
      const bson_t *vr = mongoc_bulkwriteresult_verboseresults (bwr.res);
      ASSERT (vr);
      ASSERT_MATCH (vr, BSON_STR ({"insertResults" : {"0" : {"insertedId" : 123}}}));
   }

   mongoc_bulkwriteexception_destroy (bwr.exc);
   mongoc_bulkwriteresult_destroy (bwr.res);
   mongoc_bulkwrite_destroy (bw);
   mongoc_bulkwriteoptions_destroy (opts);
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
}
