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
test_clientbulkwrite_insert (void *unused)
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
      mongoc_insertone_model_t m1 = {.document = tmp_bson ("{'_id': 123}")};
      mongoc_insertone_model_t m2 = {.document = tmp_bson ("{'_id': 456}")};
      bool ok;
      ok = mongoc_listof_bulkwritemodel_append_insertone (
         lb, "db.coll", -1, m1, &error);
      ASSERT_OR_PRINT (ok, error);
      ok = mongoc_listof_bulkwritemodel_append_insertone (
         lb, "db.coll", -1, m2, &error);
      ASSERT_OR_PRINT (ok, error);
   }

   // Do the bulk write.
   mongoc_bulkwritereturn_t br =
      mongoc_client_bulkwrite (client, lb, NULL /* opts */);

   // Ensure no error.
   {
      if (br.exc) {
         bson_error_t error;
         const bson_t *error_document;
         if (mongoc_bulkwriteexception_error (
                br.exc, &error, &error_document)) {
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
      ASSERT_CMPINT64 (mongoc_bulkwriteresult_insertedCount (br.res), ==, 2);
      const mongoc_mapof_insertoneresult_t *mir =
         mongoc_bulkwriteresult_insertResults (br.res);

      // Check index 0.
      {
         const mongoc_insertoneresult_t *ir =
            mongoc_mapof_insertoneresult_lookup (mir, 0);
         ASSERT (ir);
         bson_value_t expected = {.value_type = BSON_TYPE_INT32,
                                  .value = {.v_int32 = 123}};
         ASSERT_BSONVALUE_EQ (mongoc_insertoneresult_inserted_id (ir),
                              &expected);
      }

      // Check index 1.
      {
         const mongoc_insertoneresult_t *ir =
            mongoc_mapof_insertoneresult_lookup (mir, 1);
         ASSERT (ir);
         bson_value_t expected = {.value_type = BSON_TYPE_INT32,
                                  .value = {.v_int32 = 456}};
         ASSERT_BSONVALUE_EQ (mongoc_insertoneresult_inserted_id (ir),
                              &expected);
      }

      // Check no index 2.
      ASSERT (!mongoc_mapof_insertoneresult_lookup (mir, 2));
   }

   mongoc_bulkwritereturn_cleanup (&br);
   mongoc_listof_bulkwritemodel_destroy (lb);
   mongoc_client_destroy (client);
}

void
test_clientbulkwrite_install (TestSuite *suite)
{
   TestSuite_AddFull (
      suite,
      "/clientbulkwrite/insert",
      test_clientbulkwrite_insert,
      NULL /* dtor */,
      NULL /* ctx */,
      test_framework_skip_if_max_wire_version_less_than_25 // require server 8.0
   );
}
