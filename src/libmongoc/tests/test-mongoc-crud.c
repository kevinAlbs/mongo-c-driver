#include <mongoc/mongoc.h>

#include "json-test.h"
#include "json-test-operations.h"
#include "test-libmongoc.h"
#include "mongoc-bulkwrite.h"

static bool
crud_test_operation_cb (json_test_ctx_t *ctx, const bson_t *test, const bson_t *operation)
{
   bson_t reply;
   bool res;

   res = json_test_operation (ctx, test, operation, ctx->collection, NULL, &reply);

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
   install_json_test_suite_with_check (
      suite, JSON_DIR, "crud/legacy", &test_crud_cb, test_framework_skip_if_no_crypto, TestSuite_CheckLive);

   /* Read/write concern spec tests use the same format. */
   install_json_test_suite_with_check (
      suite, JSON_DIR, "read_write_concern/operation", &test_crud_cb, TestSuite_CheckLive);
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

   ret = mongoc_client_command_simple (client,
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

   ret = mongoc_collection_insert_one (coll, tmp_bson ("{'x':1}"), NULL /* opts */, &reply, &error);
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
   if (!strcmp (mongoc_apm_command_succeeded_get_command_name (event), "insert")) {
      prose_test_2_apm_ctx_t *ctx = mongoc_apm_command_succeeded_get_context (event);
      ASSERT (!ctx->has_reply);
      ctx->has_reply = true;
      bson_copy_to (mongoc_apm_command_succeeded_get_reply (event), &ctx->reply);
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
      db, mongoc_collection_get_name (coll), tmp_bson ("{'validator': {'x': {'$type': 'string'}}}"), &error);
   ASSERT_OR_PRINT (coll_created, error);
   mongoc_collection_destroy (coll_created);

   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_succeeded_cb (callbacks, prose_test_2_command_succeeded);
   mongoc_client_set_apm_callbacks (client, callbacks, (void *) &apm_ctx);
   mongoc_apm_callbacks_destroy (callbacks);

   ret = mongoc_collection_insert_one (coll, tmp_bson ("{'x':1}"), NULL /* opts */, &reply, &error);
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
   int numGetMore;
   int numKillCursors;
} bulkWrite_ctx;


// `bulkWrite_cb` records the number of `ops` in each sent `bulkWrite` to a BSON
// document of this form:
// { "0": <int64>, "1": <int64> ... }
static void
bulkWrite_cb (const mongoc_apm_command_started_t *event)
{
   bulkWrite_ctx *ctx = mongoc_apm_command_started_get_context (event);
   const char *cmd_name = mongoc_apm_command_started_get_command_name (event);

   if (0 == strcmp (cmd_name, "bulkWrite")) {
      const bson_t *cmd = mongoc_apm_command_started_get_command (event);
      bson_iter_t ops_iter;
      // Count the number of `ops`.
      ASSERT (bson_iter_init_find (&ops_iter, cmd, "ops"));
      bson_t ops;
      bson_iter_bson (&ops_iter, &ops);
      uint32_t ops_count = bson_count_keys (&ops);
      // Record.
      char *key = bson_strdup_printf ("%" PRIu32, bson_count_keys (&ctx->ops_counts));
      BSON_APPEND_INT64 (&ctx->ops_counts, key, ops_count);
      BSON_APPEND_INT64 (&ctx->operation_ids, key, mongoc_apm_command_started_get_operation_id (event));
      bson_free (key);
   }

   if (0 == strcmp (cmd_name, "getMore")) {
      ctx->numGetMore++;
   }

   if (0 == strcmp (cmd_name, "killCursors")) {
      ctx->numKillCursors++;
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
   bulkWrite_ctx cb_ctx = {.ops_counts = BSON_INITIALIZER, .operation_ids = BSON_INITIALIZER};
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

      ok = mongoc_client_command_simple (client, "admin", tmp_bson ("{'hello': 1}"), NULL, &reply, &error);
      ASSERT_OR_PRINT (ok, error);

      maxWriteBatchSize = bson_lookup_int32 (&reply, "maxWriteBatchSize");
      bson_destroy (&reply);
   }

   bson_t *doc = tmp_bson ("{'a': 'b'}");

   // Test maximum in one batch
   {
      mongoc_listof_bulkwritemodel_t *models = mongoc_listof_bulkwritemodel_new ();
      for (int32_t i = 0; i < maxWriteBatchSize; i++) {
         ok = mongoc_listof_bulkwritemodel_append_insertone (
            models, "db.coll", -1, (mongoc_insertone_model_t){.document = doc}, &error);
         ASSERT_OR_PRINT (ok, error);
      }

      mongoc_bulkwritereturn_t ret = mongoc_client_bulkwrite (client, models, NULL /* options */);
      ASSERT (ret.res);
      ASSERT_CMPINT64 (mongoc_bulkwriteresult_insertedCount (ret.res), ==, maxWriteBatchSize);
      mongoc_bulkwritereturn_cleanup (&ret);

      // Assert first `bulkWrite` sends `maxWriteBatchSize` ops.
      bson_t expect = BSON_INITIALIZER;
      BSON_APPEND_INT64 (&expect, "0", maxWriteBatchSize);
      ASSERT_EQUAL_BSON (&expect, &cb_ctx.ops_counts);
      bson_destroy (&expect);

      mongoc_listof_bulkwritemodel_destroy (models);
   }

   // Reset.
   bson_destroy (&cb_ctx.operation_ids);
   bson_init (&cb_ctx.operation_ids);
   bson_destroy (&cb_ctx.ops_counts);
   bson_init (&cb_ctx.ops_counts);

   // Test two batches
   {
      mongoc_listof_bulkwritemodel_t *models = mongoc_listof_bulkwritemodel_new ();
      for (int32_t i = 0; i < maxWriteBatchSize + 1; i++) {
         ok = mongoc_listof_bulkwritemodel_append_insertone (
            models, "db.coll", -1, (mongoc_insertone_model_t){.document = doc}, &error);
         ASSERT_OR_PRINT (ok, error);
      }

      mongoc_bulkwritereturn_t ret = mongoc_client_bulkwrite (client, models, NULL /* options */);
      ASSERT (ret.res);
      ASSERT_CMPINT64 (mongoc_bulkwriteresult_insertedCount (ret.res), ==, maxWriteBatchSize + 1);
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

      mongoc_listof_bulkwritemodel_destroy (models);
   }

   bson_destroy (&cb_ctx.operation_ids);
   bson_destroy (&cb_ctx.ops_counts);
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
   bulkWrite_ctx cb_ctx = {.ops_counts = BSON_INITIALIZER, .operation_ids = BSON_INITIALIZER};
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

      ok = mongoc_client_command_simple (client, "admin", tmp_bson ("{'hello': 1}"), NULL, &reply, &error);
      ASSERT_OR_PRINT (ok, error);

      maxMessageSizeBytes = bson_lookup_int32 (&reply, "maxMessageSizeBytes");
      maxBsonObjectSize = bson_lookup_int32 (&reply, "maxBsonObjectSize");
      bson_destroy (&reply);
   }


   bson_t doc = BSON_INITIALIZER;
   {
      size_t len = maxBsonObjectSize - 500;
      char *large_str = bson_malloc (len + 1);
      memset (large_str, 'b', len);
      large_str[len] = '\0';
      BSON_APPEND_UTF8 (&doc, "a", large_str);
      bson_free (large_str);
   }

   // Test maximum in one batch
   {
      mongoc_listof_bulkwritemodel_t *models = mongoc_listof_bulkwritemodel_new ();
      int32_t numModels = (maxMessageSizeBytes / maxBsonObjectSize);

      for (int32_t i = 0; i < numModels; i++) {
         ok = mongoc_listof_bulkwritemodel_append_insertone (
            models, "db.coll", -1, (mongoc_insertone_model_t){.document = &doc}, &error);
         ASSERT_OR_PRINT (ok, error);
      }

      mongoc_bulkwritereturn_t ret = mongoc_client_bulkwrite (client, models, NULL /* options */);
      if (ret.exc) {
         ASSERT_OR_PRINT (!mongoc_bulkwriteexception_error (ret.exc, &error, NULL), error);
      }
      ASSERT (ret.res);
      ASSERT_CMPINT64 (mongoc_bulkwriteresult_insertedCount (ret.res), ==, numModels);
      mongoc_bulkwritereturn_cleanup (&ret);

      // Assert one `bulkWrite` was sent:
      ASSERT_CMPUINT32 (1, ==, bson_count_keys (&cb_ctx.ops_counts));

      // Assert the count of total models sent:
      int64_t ops_count_0 = bson_lookup_int64 (&cb_ctx.ops_counts, "0");
      ASSERT_CMPINT64 (ops_count_0, ==, numModels);

      mongoc_listof_bulkwritemodel_destroy (models);
   }

   // Reset.
   bson_destroy (&cb_ctx.operation_ids);
   bson_init (&cb_ctx.operation_ids);
   bson_destroy (&cb_ctx.ops_counts);
   bson_init (&cb_ctx.ops_counts);

   // Test two batches
   {
      mongoc_listof_bulkwritemodel_t *models = mongoc_listof_bulkwritemodel_new ();
      int32_t numModels = (maxMessageSizeBytes / maxBsonObjectSize) + 1;

      for (int32_t i = 0; i < numModels; i++) {
         ok = mongoc_listof_bulkwritemodel_append_insertone (
            models, "db.coll", -1, (mongoc_insertone_model_t){.document = &doc}, &error);
         ASSERT_OR_PRINT (ok, error);
      }

      mongoc_bulkwritereturn_t ret = mongoc_client_bulkwrite (client, models, NULL /* options */);
      if (ret.exc) {
         ASSERT_OR_PRINT (!mongoc_bulkwriteexception_error (ret.exc, &error, NULL), error);
      }
      ASSERT (ret.res);
      ASSERT_CMPINT64 (mongoc_bulkwriteresult_insertedCount (ret.res), ==, numModels);
      mongoc_bulkwritereturn_cleanup (&ret);

      // Assert two `bulkWrite`s were sent:
      ASSERT_CMPUINT32 (2, ==, bson_count_keys (&cb_ctx.ops_counts));

      // Assert the sum of the total models sent:
      int64_t ops_count_0 = bson_lookup_int64 (&cb_ctx.ops_counts, "0");
      int64_t ops_count_1 = bson_lookup_int64 (&cb_ctx.ops_counts, "1");
      ASSERT_CMPINT64 (ops_count_0, ==, numModels - 1);
      ASSERT_CMPINT64 (ops_count_1, ==, 1);

      // Assert both have the same `operation_id`.
      int64_t operation_id_0 = bson_lookup_int64 (&cb_ctx.operation_ids, "0");
      int64_t operation_id_1 = bson_lookup_int64 (&cb_ctx.operation_ids, "1");
      ASSERT_CMPINT64 (operation_id_0, ==, operation_id_1);

      mongoc_listof_bulkwritemodel_destroy (models);
   }

   bson_destroy (&cb_ctx.operation_ids);
   bson_destroy (&cb_ctx.ops_counts);
   bson_destroy (&doc);
   mongoc_client_destroy (client);
}

static void
prose_test_5 (void *ctx)
{
   /*
   `MongoClient.bulkWrite` collects `writeConcernErrors` across batches
   */
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

   // Drop collection to clear prior data.
   {
      mongoc_collection_t *coll = mongoc_client_get_collection (client, "db", "coll");
      mongoc_collection_drop (coll, NULL);
      mongoc_collection_destroy (coll);
   }

   // Set callbacks to count the number of bulkWrite commands sent.
   bulkWrite_ctx cb_ctx = {.ops_counts = BSON_INITIALIZER, .operation_ids = BSON_INITIALIZER};
   {
      mongoc_apm_callbacks_t *cbs = mongoc_apm_callbacks_new ();
      mongoc_apm_set_command_started_cb (cbs, bulkWrite_cb);
      mongoc_client_set_apm_callbacks (client, cbs, &cb_ctx);
      mongoc_apm_callbacks_destroy (cbs);
   }

   // Set failpoint
   {
      ok = mongoc_client_command_simple (
         client,
         "admin",
         tmp_bson (BSON_STR ({
            "configureFailPoint" : "failCommand",
            "mode" : {"times" : 2},
            "data" : {
               "failCommands" : ["bulkWrite"],
               "writeConcernError" : {"code" : 11601, "errmsg" : "operation was interrupted"}
            }
         })),
         NULL,
         NULL,
         &error);
      ASSERT_OR_PRINT (ok, error);
   }

   // Construct models.
   mongoc_listof_bulkwritemodel_t *models = mongoc_listof_bulkwritemodel_new ();
   {
      bson_t doc = BSON_INITIALIZER;
      BSON_APPEND_UTF8 (&doc, "foo", "bar");
      for (int32_t i = 0; i < maxWriteBatchSize + 1; i++) {
         ok = mongoc_listof_bulkwritemodel_append_insertone (
            models, "db.coll", -1, (mongoc_insertone_model_t){.document = &doc}, &error);
         ASSERT_OR_PRINT (ok, error);
      }
   }

   mongoc_bulkwritereturn_t ret = mongoc_client_bulkwrite (client, models, NULL);
   ASSERT (ret.exc);

   const bson_t *error_document;
   if (mongoc_bulkwriteexception_error (ret.exc, &error, &error_document)) {
      test_error ("unexpected error: %s\n%s", error.message, tmp_json (error_document));
   }

   // Assert two batches were sent.
   ASSERT_CMPUINT32 (bson_count_keys (&cb_ctx.ops_counts), ==, 2);

   // Count write concern errors.
   {
      const mongoc_listof_writeconcernerror_t *listof_wce = mongoc_bulkwriteexception_writeConcernErrors (ret.exc);
      size_t numWriteConcernErrors = mongoc_listof_writeconcernerror_len (listof_wce);
      ASSERT_CMPSIZE_T (numWriteConcernErrors, ==, 2);
   }

   // Assert partial results.
   {
      ASSERT_CMPINT64 (mongoc_bulkwriteresult_insertedCount (ret.res), ==, maxWriteBatchSize + 1);
   }

   bson_destroy (&cb_ctx.operation_ids);
   bson_destroy (&cb_ctx.ops_counts);
   mongoc_bulkwritereturn_cleanup (&ret);
   mongoc_listof_bulkwritemodel_destroy (models);
   mongoc_client_destroy (client);
}


static void
prose_test_6 (void *ctx)
{
   /*
   `MongoClient.bulkWrite` collects `writeErrors` across batches
   */
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

   // Drop collection to clear prior data.

   mongoc_collection_t *coll = mongoc_client_get_collection (client, "db", "coll");
   mongoc_collection_drop (coll, NULL);


   // Set callbacks to count the number of bulkWrite commands sent.
   bulkWrite_ctx cb_ctx = {.ops_counts = BSON_INITIALIZER, .operation_ids = BSON_INITIALIZER};
   {
      mongoc_apm_callbacks_t *cbs = mongoc_apm_callbacks_new ();
      mongoc_apm_set_command_started_cb (cbs, bulkWrite_cb);
      mongoc_client_set_apm_callbacks (client, cbs, &cb_ctx);
      mongoc_apm_callbacks_destroy (cbs);
   }

   bson_t document = BSON_INITIALIZER;
   BSON_APPEND_INT32 (&document, "_id", 1);
   ok = mongoc_collection_insert_one (coll, &document, NULL, NULL, &error);
   ASSERT_OR_PRINT (ok, error);


   // Test Unordered
   {
      // Construct models.
      size_t numModels = 0;
      mongoc_listof_bulkwritemodel_t *models = mongoc_listof_bulkwritemodel_new ();

      for (int32_t i = 0; i < maxWriteBatchSize + 1; i++) {
         ok = mongoc_listof_bulkwritemodel_append_insertone (
            models, "db.coll", -1, (mongoc_insertone_model_t){.document = &document}, &error);
         ASSERT_OR_PRINT (ok, error);
         numModels++;
      }

      mongoc_bulkwriteoptions_t opts = {.ordered = MONGOC_OPT_BOOL_FALSE, .verboseResults = true};
      mongoc_bulkwritereturn_t ret = mongoc_client_bulkwrite (client, models, &opts);
      ASSERT (ret.exc);

      const bson_t *error_document;
      if (mongoc_bulkwriteexception_error (ret.exc, &error, &error_document)) {
         test_error ("unexpected error: %s\n%s", error.message, tmp_json (error_document));
      }

      // Assert two batches were sent.
      ASSERT_CMPUINT32 (bson_count_keys (&cb_ctx.ops_counts), ==, 2);

      // Count write errors.
      {
         const mongoc_mapof_writeerror_t *mapof_we = mongoc_bulkwriteexception_writeErrors (ret.exc);
         size_t numWriteErrors = 0;
         for (size_t i = 0; i < numModels; i++) {
            if (mongoc_mapof_writeerror_lookup (mapof_we, i)) {
               numWriteErrors += 1;
            }
         }
         ASSERT_CMPSIZE_T (numWriteErrors, ==, maxWriteBatchSize + 1);
      }

      // Assert partial results.
      {
         ASSERT_CMPINT64 (mongoc_bulkwriteresult_insertedCount (ret.res), ==, 0);
      }

      mongoc_bulkwritereturn_cleanup (&ret);
      mongoc_listof_bulkwritemodel_destroy (models);
   }

   {
      bson_destroy (&cb_ctx.operation_ids);
      bson_destroy (&cb_ctx.ops_counts);
      bson_init (&cb_ctx.operation_ids);
      bson_init (&cb_ctx.ops_counts);
   }

   // Test Ordered
   {
      // Construct models.
      size_t numModels = 0;
      mongoc_listof_bulkwritemodel_t *models = mongoc_listof_bulkwritemodel_new ();

      for (int32_t i = 0; i < maxWriteBatchSize + 1; i++) {
         ok = mongoc_listof_bulkwritemodel_append_insertone (
            models, "db.coll", -1, (mongoc_insertone_model_t){.document = &document}, &error);
         ASSERT_OR_PRINT (ok, error);
         numModels++;
      }


      mongoc_bulkwriteoptions_t opts = {.ordered = MONGOC_OPT_BOOL_TRUE, .verboseResults = true};
      mongoc_bulkwritereturn_t ret = mongoc_client_bulkwrite (client, models, &opts);
      ASSERT (ret.exc);

      const bson_t *error_document;
      if (mongoc_bulkwriteexception_error (ret.exc, &error, &error_document)) {
         test_error ("unexpected error: %s\n%s", error.message, tmp_json (error_document));
      }

      // Assert one batch was sent.
      ASSERT_CMPUINT32 (bson_count_keys (&cb_ctx.ops_counts), ==, 1);

      // Count write errors.
      {
         const mongoc_mapof_writeerror_t *mapof_we = mongoc_bulkwriteexception_writeErrors (ret.exc);
         size_t numWriteErrors = 0;
         for (size_t i = 0; i < numModels; i++) {
            if (mongoc_mapof_writeerror_lookup (mapof_we, i)) {
               numWriteErrors += 1;
            }
         }
         ASSERT_CMPSIZE_T (numWriteErrors, ==, 1);
      }

      // Assert partial results.
      {
         ASSERT_CMPINT64 (mongoc_bulkwriteresult_insertedCount (ret.res), ==, 0);
      }

      bson_destroy (&cb_ctx.operation_ids);
      bson_destroy (&cb_ctx.ops_counts);
      mongoc_bulkwritereturn_cleanup (&ret);
      mongoc_listof_bulkwritemodel_destroy (models);
   }

   bson_destroy (&document);
   mongoc_collection_destroy (coll);
   mongoc_client_destroy (client);
}

static void
prose_test_7 (void *ctx)
{
   /*
   `MongoClient.bulkWrite` handles a cursor requiring a `getMore`
   */
   mongoc_client_t *client;
   BSON_UNUSED (ctx);
   bool ok;
   bson_error_t error;

   client = test_framework_new_default_client ();
   // Get `maxBsonObjectSize` from the server.
   int32_t maxBsonObjectSize;
   {
      bson_t reply;

      ok = mongoc_client_command_simple (client, "admin", tmp_bson ("{'hello': 1}"), NULL, &reply, &error);
      ASSERT_OR_PRINT (ok, error);

      maxBsonObjectSize = bson_lookup_int32 (&reply, "maxBsonObjectSize");
      bson_destroy (&reply);
   }

   // Drop collection to clear prior data.
   mongoc_collection_t *coll = mongoc_client_get_collection (client, "db", "coll");
   mongoc_collection_drop (coll, NULL);


   // Set callbacks to count the number of bulkWrite commands sent.
   bulkWrite_ctx cb_ctx = {.ops_counts = BSON_INITIALIZER, .operation_ids = BSON_INITIALIZER};
   {
      mongoc_apm_callbacks_t *cbs = mongoc_apm_callbacks_new ();
      mongoc_apm_set_command_started_cb (cbs, bulkWrite_cb);
      mongoc_client_set_apm_callbacks (client, cbs, &cb_ctx);
      mongoc_apm_callbacks_destroy (cbs);
   }

   bson_t document = BSON_INITIALIZER;
   char *large_str = bson_malloc0 ((maxBsonObjectSize / 2) + 1);
   memset (large_str, 'a', maxBsonObjectSize / 2);
   BSON_APPEND_UTF8 (&document, "_id", large_str);
   ok = mongoc_collection_insert_one (coll, &document, NULL, NULL, &error);
   ASSERT_OR_PRINT (ok, error);

   // Construct models.
   size_t numModels = 0;
   mongoc_listof_bulkwritemodel_t *models = mongoc_listof_bulkwritemodel_new ();

   for (int32_t i = 0; i < 2; i++) {
      ok = mongoc_listof_bulkwritemodel_append_insertone (
         models, "db.coll", -1, (mongoc_insertone_model_t){.document = &document}, &error);
      ASSERT_OR_PRINT (ok, error);
      numModels++;
   }

   mongoc_bulkwriteoptions_t opts = {.ordered = MONGOC_OPT_BOOL_FALSE, .verboseResults = true};
   mongoc_bulkwritereturn_t ret = mongoc_client_bulkwrite (client, models, &opts);
   ASSERT (ret.exc);

   const bson_t *error_document;
   if (mongoc_bulkwriteexception_error (ret.exc, &error, &error_document)) {
      test_error ("unexpected error: %s\n%s", error.message, tmp_json (error_document));
   }

   // Count write errors.
   {
      const mongoc_mapof_writeerror_t *mapof_we = mongoc_bulkwriteexception_writeErrors (ret.exc);
      size_t numWriteErrors = 0;
      for (size_t i = 0; i < numModels; i++) {
         if (mongoc_mapof_writeerror_lookup (mapof_we, i)) {
            numWriteErrors += 1;
         }
      }
      ASSERT_CMPSIZE_T (numWriteErrors, ==, 2);
   }

   ASSERT_CMPINT (cb_ctx.numGetMore, ==, 1);

   bson_free (large_str);
   bson_destroy (&document);
   mongoc_bulkwritereturn_cleanup (&ret);
   mongoc_listof_bulkwritemodel_destroy (models);
   mongoc_collection_destroy (coll);
   bson_destroy (&cb_ctx.operation_ids);
   bson_destroy (&cb_ctx.ops_counts);
   mongoc_client_destroy (client);
}

static void
prose_test_8 (void *ctx)
{
   /*
   `MongoClient.bulkWrite` handles a cursor requiring a `getMore`
   */
   mongoc_client_t *client;
   BSON_UNUSED (ctx);
   bool ok;
   bson_error_t error;

   client = test_framework_new_default_client ();
   // Get `maxBsonObjectSize` from the server.
   int32_t maxBsonObjectSize;
   {
      bson_t reply;

      ok = mongoc_client_command_simple (client, "admin", tmp_bson ("{'hello': 1}"), NULL, &reply, &error);
      ASSERT_OR_PRINT (ok, error);

      maxBsonObjectSize = bson_lookup_int32 (&reply, "maxBsonObjectSize");
      bson_destroy (&reply);
   }

   // Drop collection to clear prior data.
   mongoc_collection_t *coll = mongoc_client_get_collection (client, "db", "coll");
   mongoc_collection_drop (coll, NULL);


   // Set callbacks to count the number of bulkWrite commands sent.
   bulkWrite_ctx cb_ctx = {.ops_counts = BSON_INITIALIZER, .operation_ids = BSON_INITIALIZER};
   {
      mongoc_apm_callbacks_t *cbs = mongoc_apm_callbacks_new ();
      mongoc_apm_set_command_started_cb (cbs, bulkWrite_cb);
      mongoc_client_set_apm_callbacks (client, cbs, &cb_ctx);
      mongoc_apm_callbacks_destroy (cbs);
   }

   // Configure failpoint on `getMore`.
   {
      {
         ok = mongoc_client_command_simple (client,
                                            "admin",
                                            tmp_bson (BSON_STR ({
                                               "configureFailPoint" : "failCommand",
                                               "mode" : {"times" : 1},
                                               "data" : {"failCommands" : ["getMore"], "errorCode" : 8}
                                            })),
                                            NULL,
                                            NULL,
                                            &error);
         ASSERT_OR_PRINT (ok, error);
      }
   }

   bson_t document = BSON_INITIALIZER;
   char *large_str = bson_malloc0 ((maxBsonObjectSize / 2) + 1);
   memset (large_str, 'a', maxBsonObjectSize / 2);
   BSON_APPEND_UTF8 (&document, "_id", large_str);
   ok = mongoc_collection_insert_one (coll, &document, NULL, NULL, &error);
   ASSERT_OR_PRINT (ok, error);

   // Construct models.
   size_t numModels = 0;
   mongoc_listof_bulkwritemodel_t *models = mongoc_listof_bulkwritemodel_new ();

   for (int32_t i = 0; i < 2; i++) {
      ok = mongoc_listof_bulkwritemodel_append_insertone (
         models, "db.coll", -1, (mongoc_insertone_model_t){.document = &document}, &error);
      ASSERT_OR_PRINT (ok, error);
      numModels++;
   }

   mongoc_bulkwriteoptions_t opts = {.ordered = MONGOC_OPT_BOOL_FALSE, .verboseResults = true};
   mongoc_bulkwritereturn_t ret = mongoc_client_bulkwrite (client, models, &opts);
   ASSERT (ret.exc);

   ASSERT (mongoc_bulkwriteexception_error (ret.exc, &error, NULL));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_QUERY, 8, "Failing command via 'failCommand' failpoint");

   // Count write errors.
   {
      const mongoc_mapof_writeerror_t *mapof_we = mongoc_bulkwriteexception_writeErrors (ret.exc);
      size_t numWriteErrors = 0;
      for (size_t i = 0; i < numModels; i++) {
         if (mongoc_mapof_writeerror_lookup (mapof_we, i)) {
            numWriteErrors += 1;
         }
      }
      ASSERT_CMPSIZE_T (numWriteErrors, ==, 1); // Only one error is reported.
   }

   ASSERT_CMPINT (cb_ctx.numGetMore, ==, 1);
   ASSERT_CMPINT (cb_ctx.numKillCursors, ==, 1);

   bson_free (large_str);
   bson_destroy (&document);
   mongoc_bulkwritereturn_cleanup (&ret);
   mongoc_listof_bulkwritemodel_destroy (models);
   mongoc_collection_destroy (coll);
   bson_destroy (&cb_ctx.operation_ids);
   bson_destroy (&cb_ctx.ops_counts);
   mongoc_client_destroy (client);
}


static void
prose_test_9 (void *ctx)
{
   /*
   Test a client-side error is returned if a Clientinsert is attempted for an
   unacknowledged
   */
   mongoc_client_t *client;
   BSON_UNUSED (ctx);
   bool ok;
   bson_error_t error;
   mongoc_write_concern_t *wc;

   client = test_framework_new_default_client ();
   // Get `maxBsonObjectSize` from the server.
   int32_t maxBsonObjectSize;
   {
      bson_t reply;

      ok = mongoc_client_command_simple (client, "admin", tmp_bson ("{'hello': 1}"), NULL, &reply, &error);
      ASSERT_OR_PRINT (ok, error);

      maxBsonObjectSize = bson_lookup_int32 (&reply, "maxBsonObjectSize");
      bson_destroy (&reply);
   }

   bson_t doc = BSON_INITIALIZER;
   {
      size_t len = maxBsonObjectSize;
      char *large_str = bson_malloc (len + 1);
      memset (large_str, 'b', len);
      large_str[len] = '\0';
      BSON_APPEND_UTF8 (&doc, "a", large_str);
      bson_free (large_str);
   }

   wc = mongoc_write_concern_new ();
   mongoc_write_concern_set_w (wc, MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED);
   mongoc_bulkwriteoptions_t opts = {.writeConcern = wc};

   // Test a large insert.
   {
      mongoc_listof_bulkwritemodel_t *models = mongoc_listof_bulkwritemodel_new ();

      ok = mongoc_listof_bulkwritemodel_append_insertone (
         models, "db.coll", -1, (mongoc_insertone_model_t){.document = &doc}, &error);
      ASSERT_OR_PRINT (ok, error);

      mongoc_bulkwritereturn_t ret = mongoc_client_bulkwrite (client, models, &opts);
      ASSERT (ret.exc);
      ASSERT (mongoc_bulkwriteexception_error (ret.exc, &error, NULL));
      ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "of size");

      mongoc_bulkwritereturn_cleanup (&ret);
      mongoc_listof_bulkwritemodel_destroy (models);
   }

   // Test a large replace.
   {
      mongoc_listof_bulkwritemodel_t *models = mongoc_listof_bulkwritemodel_new ();

      ok = mongoc_listof_bulkwritemodel_append_replaceone (
         models, "db.coll", -1, (mongoc_replaceone_model_t){.filter = tmp_bson ("{}"), .replacement = &doc}, &error);
      ASSERT_OR_PRINT (ok, error);

      mongoc_bulkwritereturn_t ret = mongoc_client_bulkwrite (client, models, &opts);
      ASSERT (ret.exc);
      ASSERT (mongoc_bulkwriteexception_error (ret.exc, &error, NULL));
      ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "of size");

      mongoc_bulkwritereturn_cleanup (&ret);
      mongoc_listof_bulkwritemodel_destroy (models);
   }

   bson_destroy (&doc);
   mongoc_write_concern_destroy (wc);
   mongoc_client_destroy (client);
}

static int
skip_if_no_SERVER_88895 (void)
{
   return test_framework_getenv_bool ("HAS_SERVER_888895") ? 1 : 0;
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

   TestSuite_AddFull (suite,
                      "/crud/prose_test_5",
                      prose_test_5,
                      NULL, /* dtor */
                      NULL, /* ctx */
                      test_framework_skip_if_max_wire_version_less_than_25 /* require 8.0+ server */);

   TestSuite_AddFull (suite,
                      "/crud/prose_test_6",
                      prose_test_6,
                      NULL, /* dtor */
                      NULL, /* ctx */
                      test_framework_skip_if_max_wire_version_less_than_25 /* require 8.0+ server */,
                      skip_if_no_SERVER_88895);


   TestSuite_AddFull (suite,
                      "/crud/prose_test_7",
                      prose_test_7,
                      NULL,                                                 /* dtor */
                      NULL,                                                 /* ctx */
                      test_framework_skip_if_max_wire_version_less_than_25, /* require 8.0+
                                                                               server */
                      skip_if_no_SERVER_88895);


   TestSuite_AddFull (suite,
                      "/crud/prose_test_8",
                      prose_test_8,
                      NULL,                                                /* dtor */
                      NULL,                                                /* ctx */
                      test_framework_skip_if_max_wire_version_less_than_25 /* require 8.0+
                                                                              server */
                      ,
                      skip_if_no_SERVER_88895);

   TestSuite_AddFull (suite,
                      "/crud/prose_test_9",
                      prose_test_9,
                      NULL, /* dtor */
                      NULL, /* ctx */
                      test_framework_skip_if_max_wire_version_less_than_25 /* require 8.0+ server */);
}
