#include <mongoc/mongoc.h>

#include "json-test.h"
#include "json-test-operations.h"
#include "test-libmongoc.h"
#include "mongoc-bulkwrite.h"

static bool
crud_test_operation_cb (json_test_ctx_t *ctx,
                        const bson_t *test,
                        const bson_t *operation)
{
   bson_t reply;
   bool res;

   res =
      json_test_operation (ctx, test, operation, ctx->collection, NULL, &reply);

   bson_destroy (&reply);

   return res;
}

static void
test_crud_cb (bson_t *scenario)
{
   json_test_config_t config = JSON_TEST_CONFIG_INIT;
   config.run_operation_cb = crud_test_operation_cb;
   config.command_started_events_only = true;
   config.scenario = scenario;
   run_json_general_test (&config);
}

static void
test_all_spec_tests (TestSuite *suite)
{
   install_json_test_suite_with_check (suite,
                                       JSON_DIR,
                                       "crud/legacy",
                                       &test_crud_cb,
                                       test_framework_skip_if_no_crypto,
                                       TestSuite_CheckLive);

   /* Read/write concern spec tests use the same format. */
   install_json_test_suite_with_check (suite,
                                       JSON_DIR,
                                       "read_write_concern/operation",
                                       &test_crud_cb,
                                       TestSuite_CheckLive);
}

static void
prose_test_1 (void *ctx)
{
   mongoc_client_t *client;
   mongoc_collection_t *coll;
   bool ret;
   bson_t reply;
   bson_error_t error;

   BSON_UNUSED (ctx);

   client = test_framework_new_default_client ();
   coll = get_test_collection (client, "coll");

   ret = mongoc_client_command_simple (
      client,
      "admin",
      tmp_bson ("{'configureFailPoint': 'failCommand', 'mode': {'times': 1}, "
                " 'data': {'failCommands': ['insert'], 'writeConcernError': {"
                "   'code': 100, 'codeName': 'UnsatisfiableWriteConcern', "
                "   'errmsg': 'Not enough data-bearing nodes', "
                "   'errInfo': {'writeConcern': {'w': 2, 'wtimeout': 0, "
                "               'provenance': 'clientSupplied'}}}}}"),
      NULL,
      NULL,
      &error);
   ASSERT_OR_PRINT (ret, error);

   ret = mongoc_collection_insert_one (
      coll, tmp_bson ("{'x':1}"), NULL /* opts */, &reply, &error);
   ASSERT (!ret);

   /* libmongoc does not model WriteConcernError, so we only assert that the
    * "errInfo" field set in configureFailPoint matches that in the result */
   ASSERT_MATCH (&reply,
                 "{'writeConcernErrors': [{'errInfo': {'writeConcern': {"
                 "'w': 2, 'wtimeout': 0, 'provenance': 'clientSupplied'}}}]}");

   bson_destroy (&reply);
   mongoc_collection_destroy (coll);
   mongoc_client_destroy (client);
}

typedef struct {
   bool has_reply;
   bson_t reply;
} prose_test_2_apm_ctx_t;

static void
prose_test_2_command_succeeded (const mongoc_apm_command_succeeded_t *event)
{
   if (!strcmp (mongoc_apm_command_succeeded_get_command_name (event),
                "insert")) {
      prose_test_2_apm_ctx_t *ctx =
         mongoc_apm_command_succeeded_get_context (event);
      ASSERT (!ctx->has_reply);
      ctx->has_reply = true;
      bson_copy_to (mongoc_apm_command_succeeded_get_reply (event),
                    &ctx->reply);
   }
}

static void
prose_test_2 (void *ctx)
{
   mongoc_client_t *client;
   mongoc_database_t *db;
   mongoc_collection_t *coll, *coll_created;
   mongoc_apm_callbacks_t *callbacks;
   prose_test_2_apm_ctx_t apm_ctx = {0};
   bool ret;
   bson_t reply, reply_errInfo, observed_errInfo;
   bson_error_t error = {0};

   BSON_UNUSED (ctx);

   client = test_framework_new_default_client ();
   db = get_test_database (client);
   coll = get_test_collection (client, "coll");

   /* don't care if ns not found. */
   (void) mongoc_collection_drop (coll, NULL);

   coll_created = mongoc_database_create_collection (
      db,
      mongoc_collection_get_name (coll),
      tmp_bson ("{'validator': {'x': {'$type': 'string'}}}"),
      &error);
   ASSERT_OR_PRINT (coll_created, error);
   mongoc_collection_destroy (coll_created);

   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_succeeded_cb (callbacks,
                                        prose_test_2_command_succeeded);
   mongoc_client_set_apm_callbacks (client, callbacks, (void *) &apm_ctx);
   mongoc_apm_callbacks_destroy (callbacks);

   ret = mongoc_collection_insert_one (
      coll, tmp_bson ("{'x':1}"), NULL /* opts */, &reply, &error);
   ASSERT (!ret);

   /* Assert that the WriteError's code is DocumentValidationFailure */
   ASSERT_MATCH (&reply, "{'writeErrors': [{'code': 121}]}");

   /* libmongoc does not model WriteError, so we only assert that the observed
    * "errInfo" field matches that in the result */
   ASSERT (apm_ctx.has_reply);
   bson_lookup_doc (&apm_ctx.reply, "writeErrors.0.errInfo", &observed_errInfo);
   bson_lookup_doc (&reply, "writeErrors.0.errInfo", &reply_errInfo);
   ASSERT (bson_compare (&reply_errInfo, &observed_errInfo) == 0);

   bson_destroy (&apm_ctx.reply);
   bson_destroy (&reply);
   mongoc_collection_destroy (coll);
   mongoc_database_destroy (db);
   mongoc_client_destroy (client);
}

typedef struct {
   // `ops_counts` is a BSON document of this form:
   // { "0": <int64>, "1": <int64> ... }
   bson_t ops_counts;
   // `operation_ids` is a BSON document of this form:
   // { "0": , "1":  ... }
   bson_t operation_ids;
} bulkWrite_ctx;


// `bulkWrite_cb` records the number of `ops` in each sent `bulkWrite` to a BSON
// document of this form:
// { "0": <int64>, "1": <int64> ... }
static void
bulkWrite_cb (const mongoc_apm_command_started_t *event)
{
   bulkWrite_ctx *ctx = mongoc_apm_command_started_get_context (event);

   if (0 == strcmp (mongoc_apm_command_started_get_command_name (event),
                    "bulkWrite")) {
      const bson_t *cmd = mongoc_apm_command_started_get_command (event);
      bson_iter_t ops_iter;
      // Count the number of `ops`.
      ASSERT (bson_iter_init_find (&ops_iter, cmd, "ops"));
      bson_t ops;
      bson_iter_bson (&ops_iter, &ops);
      uint32_t ops_count = bson_count_keys (&ops);
      // Record.
      char *key =
         bson_strdup_printf ("%" PRIu32, bson_count_keys (&ctx->ops_counts));
      BSON_APPEND_INT64 (&ctx->ops_counts, key, ops_count);
      BSON_APPEND_INT64 (&ctx->operation_ids,
                         key,
                         mongoc_apm_command_started_get_operation_id (event));
      bson_free (key);
   }
}

static void
prose_test_3 (void *ctx)
{
   /*
   From Spec:
   """
   Test that `MongoClient.bulkWrite` properly handles `writeModels` inputs
   containing a number of writes greater than `maxWriteBatchSize`.
   """
   */
   mongoc_client_t *client;
   BSON_UNUSED (ctx);
   bool ok;
   bson_error_t error;

   client = test_framework_new_default_client ();
   // Set callbacks to count the number of bulkWrite commands sent.
   bulkWrite_ctx cb_ctx = {.ops_counts = BSON_INITIALIZER,
                           .operation_ids = BSON_INITIALIZER};
   {
      mongoc_apm_callbacks_t *cbs = mongoc_apm_callbacks_new ();
      mongoc_apm_set_command_started_cb (cbs, bulkWrite_cb);
      mongoc_client_set_apm_callbacks (client, cbs, &cb_ctx);
      mongoc_apm_callbacks_destroy (cbs);
   }

   // Get `maxWriteBatchSize` from the server.
   int32_t maxWriteBatchSize;
   {
      bson_t reply;

      ok = mongoc_client_command_simple (
         client, "admin", tmp_bson ("{'hello': 1}"), NULL, &reply, &error);
      ASSERT_OR_PRINT (ok, error);

      maxWriteBatchSize = bson_lookup_int32 (&reply, "maxWriteBatchSize");
      bson_destroy (&reply);
   }

   mongoc_listof_bulkwritemodel_t *models = mongoc_listof_bulkwritemodel_new ();
   bson_t *doc = tmp_bson ("{'a': 'b'}");

   for (int32_t i = 0; i < maxWriteBatchSize + 1; i++) {
      ok = mongoc_listof_bulkwritemodel_append_insertone (
         models,
         "db.coll",
         -1,
         (mongoc_insertone_model_t){.document = doc},
         &error);
      ASSERT_OR_PRINT (ok, error);
   }

   mongoc_bulkwritereturn_t ret =
      mongoc_client_bulkwrite (client, models, NULL /* options */);
   ASSERT (ret.res);
   ASSERT_CMPINT64 (mongoc_bulkwriteresult_insertedCount (ret.res),
                    ==,
                    maxWriteBatchSize + 1);
   mongoc_bulkwritereturn_cleanup (&ret);

   // Assert first `bulkWrite` sends `maxWriteBatchSize` ops.
   // Assert second `bulkWrite` sends 1 op.
   bson_t expect = BSON_INITIALIZER;
   BSON_APPEND_INT64 (&expect, "0", maxWriteBatchSize);
   BSON_APPEND_INT64 (&expect, "1", 1);
   ASSERT_EQUAL_BSON (&expect, &cb_ctx.ops_counts);
   bson_destroy (&expect);

   // Assert both have the same `operation_id`.
   int64_t operation_id_0 = bson_lookup_int64 (&cb_ctx.operation_ids, "0");
   int64_t operation_id_1 = bson_lookup_int64 (&cb_ctx.operation_ids, "1");
   ASSERT_CMPINT64 (operation_id_0, ==, operation_id_1);

   bson_destroy (&cb_ctx.operation_ids);
   bson_destroy (&cb_ctx.ops_counts);
   mongoc_listof_bulkwritemodel_destroy (models);
   mongoc_client_destroy (client);
}

static void
prose_test_4 (void *ctx)
{
   /*
   From Spec:
   """
   Test that `MongoClient.bulkWrite` properly handles a `writeModels` input
   which constructs an `ops` array larger than `maxMessageSizeBytes`.
   """
   */
   mongoc_client_t *client;
   BSON_UNUSED (ctx);
   bool ok;
   bson_error_t error;

   client = test_framework_new_default_client ();
   // Set callbacks to count the number of bulkWrite commands sent.
   bulkWrite_ctx cb_ctx = {.ops_counts = BSON_INITIALIZER,
                           .operation_ids = BSON_INITIALIZER};
   {
      mongoc_apm_callbacks_t *cbs = mongoc_apm_callbacks_new ();
      mongoc_apm_set_command_started_cb (cbs, bulkWrite_cb);
      mongoc_client_set_apm_callbacks (client, cbs, &cb_ctx);
      mongoc_apm_callbacks_destroy (cbs);
   }

   // Get `maxWriteBatchSize` and `maxBsonObjectSize` from the server.
   int32_t maxMessageSizeBytes;
   int32_t maxBsonObjectSize;
   {
      bson_t reply;

      ok = mongoc_client_command_simple (
         client, "admin", tmp_bson ("{'hello': 1}"), NULL, &reply, &error);
      ASSERT_OR_PRINT (ok, error);

      maxMessageSizeBytes = bson_lookup_int32 (&reply, "maxMessageSizeBytes");
      maxBsonObjectSize = bson_lookup_int32 (&reply, "maxBsonObjectSize");
      bson_destroy (&reply);
   }

   mongoc_listof_bulkwritemodel_t *models = mongoc_listof_bulkwritemodel_new ();

   bson_t doc = BSON_INITIALIZER;
   {
      size_t len = maxBsonObjectSize - 500;
      char *large_str = bson_malloc (len + 1);
      memset (large_str, 'b', len);
      large_str[len] = '\0';
      BSON_APPEND_UTF8 (&doc, "a", large_str);
      bson_free (large_str);
   }

   int32_t numModels = (maxMessageSizeBytes / maxBsonObjectSize) + 1;


   for (int32_t i = 0; i < numModels; i++) {
      ok = mongoc_listof_bulkwritemodel_append_insertone (
         models,
         "db.coll",
         -1,
         (mongoc_insertone_model_t){.document = &doc},
         &error);
      ASSERT_OR_PRINT (ok, error);
   }

   mongoc_bulkwritereturn_t ret =
      mongoc_client_bulkwrite (client, models, NULL /* options */);
   if (ret.exc) {
      ASSERT_OR_PRINT (!mongoc_bulkwriteexception_error (ret.exc, &error, NULL),
                       error);
   }
   ASSERT (ret.res);
   ASSERT_CMPINT64 (
      mongoc_bulkwriteresult_insertedCount (ret.res), ==, numModels);
   mongoc_bulkwritereturn_cleanup (&ret);

   // Assert two `bulkWrite`s were sent:
   ASSERT_CMPUINT32 (2, ==, bson_count_keys (&cb_ctx.ops_counts));

   // Assert the sum of the total models sent:
   int64_t ops_count_0 = bson_lookup_int64 (&cb_ctx.ops_counts, "0");
   int64_t ops_count_1 = bson_lookup_int64 (&cb_ctx.ops_counts, "1");
   ASSERT_CMPINT64 (ops_count_0 + ops_count_1, ==, numModels);

   // Assert both have the same `operation_id`.
   int64_t operation_id_0 = bson_lookup_int64 (&cb_ctx.operation_ids, "0");
   int64_t operation_id_1 = bson_lookup_int64 (&cb_ctx.operation_ids, "1");
   ASSERT_CMPINT64 (operation_id_0, ==, operation_id_1);

   bson_destroy (&cb_ctx.operation_ids);
   bson_destroy (&cb_ctx.ops_counts);
   bson_destroy (&doc);
   mongoc_listof_bulkwritemodel_destroy (models);
   mongoc_client_destroy (client);
}


void
test_crud_install (TestSuite *suite)
{
   test_all_spec_tests (suite);

   TestSuite_AddFull (suite,
                      "/crud/prose_test_1",
                      prose_test_1,
                      NULL, /* dtor */
                      NULL, /* ctx */
                      test_framework_skip_if_no_failpoint,
                      test_framework_skip_if_max_wire_version_less_than_7);

   TestSuite_AddFull (suite,
                      "/crud/prose_test_2",
                      prose_test_2,
                      NULL, /* dtor */
                      NULL, /* ctx */
                      test_framework_skip_if_max_wire_version_less_than_13);

   TestSuite_AddFull (suite,
                      "/crud/prose_test_3",
                      prose_test_3,
                      NULL, /* dtor */
                      NULL, /* ctx */
                      test_framework_skip_if_max_wire_version_less_than_25 /* require 8.0+ server */);

   TestSuite_AddFull (suite,
                      "/crud/prose_test_4",
                      prose_test_4,
                      NULL, /* dtor */
                      NULL, /* ctx */
                      test_framework_skip_if_max_wire_version_less_than_25 /* require 8.0+ server */);
}
