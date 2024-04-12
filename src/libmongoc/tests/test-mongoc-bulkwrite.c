#include <mongoc/mongoc.h>

#include "test-libmongoc.h"
#include "mongoc-bulkwrite.h"
#include "TestSuite.h"
#include "test-conveniences.h"

static void
test_bulkwrite_insert_errors (void *ctx)
{
   mongoc_client_t *client;
   BSON_UNUSED (ctx);
   bool ok;
   bson_error_t error;

   client = test_framework_new_default_client ();

   // Drop prior test data.
   {
      mongoc_collection_t *coll = mongoc_client_get_collection (client, "db", "coll");
      mongoc_collection_drop (coll, NULL);
      mongoc_collection_destroy (coll);
   }

   mongoc_listof_bulkwritemodel_t *models = mongoc_listof_bulkwritemodel_new ();
   size_t numModels = 0;

   ok = mongoc_listof_bulkwritemodel_append_insertone (
      models, "db.coll", -1, (mongoc_insertone_model_t){.document = tmp_bson ("{'_id': 1 }")}, &error);
   ASSERT_OR_PRINT (ok, error);
   numModels++;
   ok = mongoc_listof_bulkwritemodel_append_insertone (
      models, "db.coll", -1, (mongoc_insertone_model_t){.document = tmp_bson ("{'_id': 1 }")}, &error);
   ASSERT_OR_PRINT (ok, error);
   numModels++;
   ok = mongoc_listof_bulkwritemodel_append_insertone (
      models, "db.coll", -1, (mongoc_insertone_model_t){.document = tmp_bson ("{'_id': 1 }")}, &error);
   ASSERT_OR_PRINT (ok, error);
   numModels++;

   mongoc_bulkwriteoptions_t opts = {.verboseResults = true};
   mongoc_bulkwritereturn_t ret = mongoc_client_bulkwrite (client, models, &opts);
   ASSERT (ret.exc);

   ASSERT (ret.res);
   const mongoc_mapof_insertoneresult_t *mapof_ir = mongoc_bulkwriteresult_insertResults (ret.res);
   size_t numInsertResults = 0;
   for (size_t i = 0; i < numModels; i++) {
      if (mongoc_mapof_insertoneresult_lookup (mapof_ir, i)) {
         numInsertResults++;
      }
   }
   // Expect only one insert reported.
   ASSERT_CMPSIZE_T (numInsertResults, ==, 1);
   mongoc_bulkwritereturn_cleanup (&ret);
   mongoc_listof_bulkwritemodel_destroy (models);
   mongoc_client_destroy (client);
}

static void
test_unacknowledged (void *ctx)
{
   mongoc_client_t *client;
   BSON_UNUSED (ctx);
   bool ok;
   bson_error_t error;
   mongoc_write_concern_t *wc = mongoc_write_concern_new ();

   mongoc_write_concern_set_w (wc, MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED);

   client = test_framework_new_default_client ();

   mongoc_listof_bulkwritemodel_t *models = mongoc_listof_bulkwritemodel_new ();
   ok = mongoc_listof_bulkwritemodel_append_insertone (
      models, "db.coll", -1, (mongoc_insertone_model_t){.document = tmp_bson ("{'a': 1 }")}, &error);
   ASSERT_OR_PRINT (ok, error);
   mongoc_bulkwriteoptions_t opts = {.writeConcern = wc};
   mongoc_bulkwritereturn_t ret = mongoc_client_bulkwrite (client, models, &opts);
   // Expect no result.
   ASSERT (!ret.res);
   mongoc_bulkwritereturn_cleanup (&ret);
   mongoc_listof_bulkwritemodel_destroy (models);
   mongoc_client_destroy (client);
   mongoc_write_concern_destroy (wc);
}

static void
test_session_with_unacknowledged (void *ctx)
{
   mongoc_client_t *client;
   BSON_UNUSED (ctx);
   bool ok;
   bson_error_t error;
   mongoc_write_concern_t *wc = mongoc_write_concern_new ();

   mongoc_write_concern_set_w (wc, MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED);

   client = test_framework_new_default_client ();
   mongoc_client_session_t *session = mongoc_client_start_session (client, NULL, &error);
   ASSERT_OR_PRINT (session, error);

   mongoc_listof_bulkwritemodel_t *models = mongoc_listof_bulkwritemodel_new ();
   ok = mongoc_listof_bulkwritemodel_append_insertone (
      models, "db.coll", -1, (mongoc_insertone_model_t){.document = tmp_bson ("{'a': 1 }")}, &error);
   ASSERT_OR_PRINT (ok, error);
   mongoc_bulkwriteoptions_t opts = {.session = session, .writeConcern = wc};
   mongoc_bulkwritereturn_t ret = mongoc_client_bulkwrite (client, models, &opts);
   ASSERT (ret.exc);
   ASSERT (mongoc_bulkwriteexception_error (ret.exc, &error, NULL));
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_COMMAND,
                          MONGOC_ERROR_COMMAND_INVALID_ARG,
                          "Cannot use client session with unacknowledged command");
   mongoc_bulkwritereturn_cleanup (&ret);
   mongoc_listof_bulkwritemodel_destroy (models);
   mongoc_client_session_destroy (session);
   mongoc_client_destroy (client);
   mongoc_write_concern_destroy (wc);
}

static void
test_verbose (void *ctx)
{
   mongoc_client_t *client;
   BSON_UNUSED (ctx);
   bool ok;
   bson_error_t error;

   client = test_framework_new_default_client ();

   mongoc_listof_bulkwritemodel_t *models = mongoc_listof_bulkwritemodel_new ();
   ok = mongoc_listof_bulkwritemodel_append_insertone (
      models, "db.coll", -1, (mongoc_insertone_model_t){.document = tmp_bson ("{'a': 1 }")}, &error);
   ASSERT_OR_PRINT (ok, error);
   mongoc_bulkwriteoptions_t opts = {.verboseResults = true};
   mongoc_bulkwritereturn_t ret = mongoc_client_bulkwrite (client, models, &opts);
   ASSERT (ret.res);
   ASSERT (mongoc_bulkwriteresult_hasVerboseResults (ret.res));
   mongoc_bulkwritereturn_cleanup (&ret);
   mongoc_listof_bulkwritemodel_destroy (models);
   mongoc_client_destroy (client);
}

void
test_bulkwrite_install (TestSuite *suite)
{
   TestSuite_AddFull (suite,
                      "/bulkwrite/insert_errors",
                      test_bulkwrite_insert_errors,
                      NULL, /* dtor */
                      NULL, /* ctx */
                      test_framework_skip_if_max_wire_version_less_than_25);
   TestSuite_AddFull (suite,
                      "/bulkwrite/unacknowledged",
                      test_unacknowledged,
                      NULL, /* dtor */
                      NULL, /* ctx */
                      test_framework_skip_if_max_wire_version_less_than_25);
   TestSuite_AddFull (suite,
                      "/bulkwrite/session_with_unacknowledged",
                      test_session_with_unacknowledged,
                      NULL, /* dtor */
                      NULL, /* ctx */
                      test_framework_skip_if_max_wire_version_less_than_25);
   TestSuite_AddFull (suite,
                      "/bulkwrite/verbose",
                      test_verbose,
                      NULL, /* dtor */
                      NULL, /* ctx */
                      test_framework_skip_if_max_wire_version_less_than_25);
}
