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

#include "bulkWrite-impl.c"

static void
test_bulkWrite_insert (void *unused)
{
   BSON_UNUSED (unused);
   mongoc_client_t *client = test_framework_new_default_client ();

   // Drop prior data.
   {
      mongoc_collection_t *coll =
         mongoc_client_get_collection (client, "db", "coll");
      mongoc_collection_drop (coll, NULL); // Ignore return.
      mongoc_collection_destroy (coll);
   }

   // Create list of insert models.
   mongoc_listof_bulkwritemodel_t *lb;
   {
      lb = mongoc_listof_bulkwritemodel_new ();
      bson_error_t error;
      ASSERT_OR_PRINT (
         mongoc_listof_bulkwritemodel_append_insertone (
            lb, "db.coll", -1, tmp_bson ("{'_id': 123 }"), &error),
         error);
      ASSERT_OR_PRINT (
         mongoc_listof_bulkwritemodel_append_insertone (
            lb, "db.coll", -1, tmp_bson ("{'_id': 456 }"), &error),
         error);
   }

   // Do the bulk write.
   mongoc_bulkwritereturn_t br =
      mongoc_client_bulkwrite (client, lb, NULL /* opts */);

   // Ensure no error.
   {
      if (br.exc) {
         const bson_t *error_document;
         bson_error_t error;
         if (mongoc_bulkwriteexception_has_error (br.exc)) {
            mongoc_bulkwriteexception_get_error (
               br.exc, &error, &error_document);
            test_error ("Expected no exception, got: %s\n%s\n",
                        error.message,
                        tmp_json (error_document));
         }
         test_error ("Expected no exception, got one with no top-level error");
      }
   }

   // Ensure results report IDs inserted.
   {
      ASSERT (br.res);

      ASSERT (mongoc_bulkwriteresult_has_insertedcount (br.res));
      int64_t count = mongoc_bulkwriteresult_get_insertedcount (br.res);
      ASSERT_CMPINT64 (count, ==, 2);

      // Check index 0.
      {
         const mongoc_insertoneresult_t *ir =
            mongoc_bulkwriteresult_get_insertoneresult (br.res, 0);
         ASSERT (ir);
         const bson_value_t *id = mongoc_insertoneresult_get_insertedid (ir);
         bson_value_t expected = {.value_type = BSON_TYPE_INT32,
                                  .value = {.v_int32 = 123}};
         ASSERT_BSONVALUE_EQ (id, &expected);
      }

      // Check index 1.
      {
         const mongoc_insertoneresult_t *ir =
            mongoc_bulkwriteresult_get_insertoneresult (br.res, 1);
         ASSERT (ir);
         const bson_value_t *id = mongoc_insertoneresult_get_insertedid (ir);
         bson_value_t expected = {.value_type = BSON_TYPE_INT32,
                                  .value = {.v_int32 = 456}};
         ASSERT_BSONVALUE_EQ (id, &expected);
      }

      // Check no index 2.
      ASSERT (!mongoc_bulkwriteresult_get_insertoneresult (br.res, 2));
   }

   mongoc_bulkwritereturn_cleanup (&br);
   mongoc_listof_bulkwritemodel_destroy (lb);
   mongoc_client_destroy (client);
}

static void
test_bulkWrite_insert_with_writeError (void *unused)
{
   BSON_UNUSED (unused);
   mongoc_client_t *client = test_framework_new_default_client ();

   // Drop prior data.
   {
      mongoc_collection_t *coll =
         mongoc_client_get_collection (client, "db", "coll");
      mongoc_collection_drop (coll, NULL); // Ignore return.
      mongoc_collection_destroy (coll);
   }

   // Create list of insert models.
   mongoc_listof_bulkwritemodel_t *lb;
   {
      lb = mongoc_listof_bulkwritemodel_new ();
      bson_error_t error;
      ASSERT_OR_PRINT (
         mongoc_listof_bulkwritemodel_append_insertone (
            lb, "db.coll", -1, tmp_bson ("{'_id': 123 }"), &error),
         error);
      ASSERT_OR_PRINT (
         mongoc_listof_bulkwritemodel_append_insertone (
            lb, "db.coll", -1, tmp_bson ("{'_id': 123 }"), &error),
         error);
   }

   // Do the bulk write.
   mongoc_bulkwritereturn_t br =
      mongoc_client_bulkwrite (client, lb, NULL /* opts */);

   // Expect an error due to duplicate keys.
   {
      ASSERT (br.exc);
      const mongoc_write_error_t *we =
         mongoc_bulkwriteexception_get_writeerror (br.exc, 1);
      ASSERT (we);
      ASSERT_CONTAINS (mongoc_write_error_get_message (we),
                       "E11000 duplicate key error collection: db.coll index: "
                       "_id_ dup key: { _id: 123 }");
   }

   // Ensure only reported ID of successful insert is reported.
   {
      ASSERT (br.res);

      ASSERT (mongoc_bulkwriteresult_has_insertedcount (br.res));
      int64_t count = mongoc_bulkwriteresult_get_insertedcount (br.res);
      ASSERT_CMPINT64 (count, ==, 1);

      // Check index 0.
      {
         const mongoc_insertoneresult_t *ir =
            mongoc_bulkwriteresult_get_insertoneresult (br.res, 0);
         ASSERT (ir);
         const bson_value_t *id = mongoc_insertoneresult_get_insertedid (ir);
         bson_value_t expected = {.value_type = BSON_TYPE_INT32,
                                  .value = {.v_int32 = 123}};
         ASSERT_BSONVALUE_EQ (id, &expected);
      }

      // Expect failed insert not to be reported.
      {
         const mongoc_insertoneresult_t *ir =
            mongoc_bulkwriteresult_get_insertoneresult (br.res, 1);
         ASSERT (!ir);
      }
   }

   mongoc_bulkwritereturn_cleanup (&br);
   mongoc_listof_bulkwritemodel_destroy (lb);
   mongoc_client_destroy (client);
}

static void
test_bulkWrite_insert_with_writeError_with_details (void *unused)
{
   BSON_UNUSED (unused);
   mongoc_client_t *client = test_framework_new_default_client ();

   // Drop prior data.
   {
      mongoc_collection_t *coll =
         mongoc_client_get_collection (client, "db", "coll");
      mongoc_collection_drop (coll, NULL); // Ignore return.
      mongoc_collection_destroy (coll);
   }

   // Create collection with a validator.
   {
      mongoc_database_t *db = mongoc_client_get_database (client, "db");
      bson_error_t error;
      mongoc_collection_t *coll = mongoc_database_create_collection (
         db,
         "coll",
         tmp_bson (BSON_STR (
            {"validator" : {"$jsonSchema" : {"required" : ["foo"]}}})),
         &error);
      ASSERT_OR_PRINT (coll, error);
      mongoc_collection_destroy (coll);
      mongoc_database_destroy (db);
   }

   // Create list of insert models.
   mongoc_listof_bulkwritemodel_t *lb;
   {
      lb = mongoc_listof_bulkwritemodel_new ();
      bson_error_t error;
      ASSERT_OR_PRINT (
         mongoc_listof_bulkwritemodel_append_insertone (
            lb, "db.coll", -1, tmp_bson ("{'_id': 123 }"), &error),
         error);
   }

   // Do the bulk write.
   mongoc_bulkwritereturn_t br =
      mongoc_client_bulkwrite (client, lb, NULL /* opts */);

   // Expect an error due to document validation failure
   {
      ASSERT (br.exc);
      const mongoc_write_error_t *we =
         mongoc_bulkwriteexception_get_writeerror (br.exc, 0);
      ASSERT (we);
      ASSERT_CONTAINS (mongoc_write_error_get_message (we),
                       "Document failed validation");
      ASSERT_MATCH (mongoc_write_error_get_details (we),
                    BSON_STR ({"failingDocumentId" : 123}));
   }

   mongoc_bulkwritereturn_cleanup (&br);
   mongoc_listof_bulkwritemodel_destroy (lb);
   mongoc_client_destroy (client);
}

static void
test_bulkWrite_insert_multiBatch_maxWriteBatchSize (void *unused)
{
   BSON_UNUSED (unused);
   mongoc_client_t *client = test_framework_new_default_client ();

   // Mock the maxWriteBatchSize.
   _mock_maxWriteBatchSize = 1;

   // Drop prior data.
   {
      mongoc_collection_t *coll =
         mongoc_client_get_collection (client, "db", "coll");
      mongoc_collection_drop (coll, NULL); // Ignore return.
      mongoc_collection_destroy (coll);
   }

   // Create list of insert models.
   mongoc_listof_bulkwritemodel_t *lb;
   {
      lb = mongoc_listof_bulkwritemodel_new ();
      bson_error_t error;
      ASSERT_OR_PRINT (
         mongoc_listof_bulkwritemodel_append_insertone (
            lb, "db.coll", -1, tmp_bson ("{'_id': 123 }"), &error),
         error);
      ASSERT_OR_PRINT (
         mongoc_listof_bulkwritemodel_append_insertone (
            lb, "db.coll", -1, tmp_bson ("{'_id': 456 }"), &error),
         error);
   }

   // Do the bulk write.
   mongoc_bulkwritereturn_t br =
      mongoc_client_bulkwrite (client, lb, NULL /* opts */);

   // Ensure no error.
   {
      if (br.exc) {
         const bson_t *error_document;
         bson_error_t error;
         if (mongoc_bulkwriteexception_has_error (br.exc)) {
            mongoc_bulkwriteexception_get_error (
               br.exc, &error, &error_document);
            test_error ("Expected no exception, got: %s\n%s\n",
                        error.message,
                        tmp_json (error_document));
         }
         test_error ("Expected no exception, got one with no top-level error");
      }
   }

   // Ensure results report IDs inserted.
   {
      ASSERT (br.res);

      ASSERT (mongoc_bulkwriteresult_has_insertedcount (br.res));
      int64_t count = mongoc_bulkwriteresult_get_insertedcount (br.res);
      ASSERT_CMPINT64 (count, ==, 2);

      // Check index 0.
      {
         const mongoc_insertoneresult_t *ir =
            mongoc_bulkwriteresult_get_insertoneresult (br.res, 0);
         ASSERT (ir);
         const bson_value_t *id = mongoc_insertoneresult_get_insertedid (ir);
         bson_value_t expected = {.value_type = BSON_TYPE_INT32,
                                  .value = {.v_int32 = 123}};
         ASSERT_BSONVALUE_EQ (id, &expected);
      }

      // Check index 1.
      {
         const mongoc_insertoneresult_t *ir =
            mongoc_bulkwriteresult_get_insertoneresult (br.res, 1);
         ASSERT (ir);
         const bson_value_t *id = mongoc_insertoneresult_get_insertedid (ir);
         bson_value_t expected = {.value_type = BSON_TYPE_INT32,
                                  .value = {.v_int32 = 456}};
         ASSERT_BSONVALUE_EQ (id, &expected);
      }

      // Check no index 2.
      ASSERT (!mongoc_bulkwriteresult_get_insertoneresult (br.res, 2));
   }

   // Restore.
   _mock_maxWriteBatchSize = 0;

   mongoc_bulkwritereturn_cleanup (&br);
   mongoc_listof_bulkwritemodel_destroy (lb);
   mongoc_client_destroy (client);
}

static void
command_started (const mongoc_apm_command_started_t *event)
{
   int *nBulkwrite = mongoc_apm_command_started_get_context (event);
   const char *cmd_name = mongoc_apm_command_started_get_command_name (event);
   if (0 == strcmp (cmd_name, "bulkWrite")) {
      *nBulkwrite += 1;
   }
}

static int *
capture_bulkWrite_count (mongoc_client_t *client)
{
   int *nBulkwrite = bson_malloc0 (sizeof (int));
   mongoc_apm_callbacks_t *cbs = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_started_cb (cbs, command_started);
   ASSERT (mongoc_client_set_apm_callbacks (client, cbs, nBulkwrite));
   mongoc_apm_callbacks_destroy (cbs);
   return nBulkwrite;
}

static void
test_bulkWrite_insert_multiBatch_maxMessageSizeBytes (void *unused)
{
   BSON_UNUSED (unused);
   mongoc_client_t *client = test_framework_new_default_client ();

   // Capture the number of `bulkWrite` commands.
   int *nBulkWrite = capture_bulkWrite_count (client);

   // Mock the maxWriteBatchSize.
   _mock_maxMessageSizeBytes = 300; // Enough for one, but not two.

   // Drop prior data.
   {
      mongoc_collection_t *coll =
         mongoc_client_get_collection (client, "db", "coll");
      mongoc_collection_drop (coll, NULL); // Ignore return.
      mongoc_collection_destroy (coll);
   }

   // Create list of insert models.
   mongoc_listof_bulkwritemodel_t *lb;
   {
      lb = mongoc_listof_bulkwritemodel_new ();
      bson_error_t error;
      ASSERT_OR_PRINT (
         mongoc_listof_bulkwritemodel_append_insertone (
            lb, "db.coll", -1, tmp_bson ("{'_id': 123 }"), &error),
         error);
      ASSERT_OR_PRINT (
         mongoc_listof_bulkwritemodel_append_insertone (
            lb, "db.coll", -1, tmp_bson ("{'_id': 456 }"), &error),
         error);
   }

   // Do the bulk write.
   mongoc_bulkwritereturn_t br =
      mongoc_client_bulkwrite (client, lb, NULL /* opts */);

   ASSERT_CMPINT (*nBulkWrite, ==, 2);

   // Ensure no error.
   {
      if (br.exc) {
         const bson_t *error_document;
         bson_error_t error;
         if (mongoc_bulkwriteexception_has_error (br.exc)) {
            mongoc_bulkwriteexception_get_error (
               br.exc, &error, &error_document);
            test_error ("Expected no exception, got: %s\n%s\n",
                        error.message,
                        tmp_json (error_document));
         }
         test_error ("Expected no exception, got one with no top-level error");
      }
   }

   // Ensure results report IDs inserted.
   {
      ASSERT (br.res);

      ASSERT (mongoc_bulkwriteresult_has_insertedcount (br.res));
      int64_t count = mongoc_bulkwriteresult_get_insertedcount (br.res);
      ASSERT_CMPINT64 (count, ==, 2);

      // Check index 0.
      {
         const mongoc_insertoneresult_t *ir =
            mongoc_bulkwriteresult_get_insertoneresult (br.res, 0);
         ASSERT (ir);
         const bson_value_t *id = mongoc_insertoneresult_get_insertedid (ir);
         bson_value_t expected = {.value_type = BSON_TYPE_INT32,
                                  .value = {.v_int32 = 123}};
         ASSERT_BSONVALUE_EQ (id, &expected);
      }

      // Check index 1.
      {
         const mongoc_insertoneresult_t *ir =
            mongoc_bulkwriteresult_get_insertoneresult (br.res, 1);
         ASSERT (ir);
         const bson_value_t *id = mongoc_insertoneresult_get_insertedid (ir);
         bson_value_t expected = {.value_type = BSON_TYPE_INT32,
                                  .value = {.v_int32 = 456}};
         ASSERT_BSONVALUE_EQ (id, &expected);
      }

      // Check no index 2.
      ASSERT (!mongoc_bulkwriteresult_get_insertoneresult (br.res, 2));
   }

   // Restore.
   _mock_maxMessageSizeBytes = 0;

   bson_free (nBulkWrite);
   mongoc_bulkwritereturn_cleanup (&br);
   mongoc_listof_bulkwritemodel_destroy (lb);
   mongoc_client_destroy (client);
}


void
test_bulkWrite_install (TestSuite *suite)
{
   TestSuite_AddFull (
      suite,
      "/bulkWrite/insert",
      test_bulkWrite_insert,
      NULL /* dtor */,
      NULL /* ctx */,
      test_framework_skip_if_max_wire_version_less_than_25 // require server 8.0
   );

   TestSuite_AddFull (
      suite,
      "/bulkWrite/insert/writeError",
      test_bulkWrite_insert_with_writeError,
      NULL /* dtor */,
      NULL /* ctx */,
      test_framework_skip_if_max_wire_version_less_than_25 // require server 8.0
   );

   TestSuite_AddFull (
      suite,
      "/bulkWrite/insert/writeError/details",
      test_bulkWrite_insert_with_writeError_with_details,
      NULL /* dtor */,
      NULL /* ctx */,
      test_framework_skip_if_max_wire_version_less_than_25 // require server 8.0
   );

   TestSuite_AddFull (
      suite,
      "/bulkWrite/insert/multiBatch/maxWriteBatchSize",
      test_bulkWrite_insert_multiBatch_maxWriteBatchSize,
      NULL /* dtor */,
      NULL /* ctx */,
      test_framework_skip_if_max_wire_version_less_than_25 // require server 8.0
   );

   TestSuite_AddFull (
      suite,
      "/bulkWrite/insert/multiBatch/maxMessageSizeBytes",
      test_bulkWrite_insert_multiBatch_maxMessageSizeBytes,
      NULL /* dtor */,
      NULL /* ctx */,
      test_framework_skip_if_max_wire_version_less_than_25 // require server 8.0
   );
}
