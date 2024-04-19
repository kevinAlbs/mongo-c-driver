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
   `MongoClient.bulkWrite` batch splits a `writeModels` input with greater than `maxWriteBatchSize` operations
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
   mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);
   for (int32_t i = 0; i < maxWriteBatchSize + 1; i++) {
      ok = mongoc_bulkwrite_append_insertone (bw, "db.coll", -1, doc, NULL, &error);
      ASSERT_OR_PRINT (ok, error);
   }

   mongoc_bulkwritereturn_t ret = mongoc_bulkwrite_execute (bw, NULL /* options */);
   ASSERT (ret.res);
   ASSERT_CMPINT64 (mongoc_bulkwriteresult_insertedcount (ret.res), ==, maxWriteBatchSize + 1);
   mongoc_bulkwriteexception_destroy (ret.exc);
   mongoc_bulkwriteresult_destroy (ret.res);


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

   mongoc_bulkwrite_destroy (bw);
   bson_destroy (&cb_ctx.operation_ids);
   bson_destroy (&cb_ctx.ops_counts);
   mongoc_client_destroy (client);
}

static void
prose_test_4 (void *ctx)
{
   /*
   `MongoClient.bulkWrite` batch splits when an `ops` payload exceeds `maxMessageSizeBytes`
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

   mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);
   int32_t numModels = (maxMessageSizeBytes / maxBsonObjectSize) + 1;

   for (int32_t i = 0; i < numModels; i++) {
      ok = mongoc_bulkwrite_append_insertone (bw, "db.coll", -1, &doc, NULL, &error);
      ASSERT_OR_PRINT (ok, error);
   }

   mongoc_bulkwritereturn_t ret = mongoc_bulkwrite_execute (bw, NULL /* options */);
   ASSERT_NO_BULKWRITEEXCEPTION (ret);
   ASSERT (ret.res);
   ASSERT_CMPINT64 (mongoc_bulkwriteresult_insertedcount (ret.res), ==, numModels);
   mongoc_bulkwriteexception_destroy (ret.exc);
   mongoc_bulkwriteresult_destroy (ret.res);


   // Assert two `bulkWrite`s were sent:
   ASSERT_CMPUINT32 (2, ==, bson_count_keys (&cb_ctx.ops_counts));

   // Assert first `bulkWrite` sends `numModels - 1` ops.
   // Assert second `bulkWrite` sends 1 op.
   int64_t ops_count_0 = bson_lookup_int64 (&cb_ctx.ops_counts, "0");
   int64_t ops_count_1 = bson_lookup_int64 (&cb_ctx.ops_counts, "1");
   ASSERT_CMPINT64 (ops_count_0, ==, numModels - 1);
   ASSERT_CMPINT64 (ops_count_1, ==, 1);

   // Assert both have the same `operation_id`.
   int64_t operation_id_0 = bson_lookup_int64 (&cb_ctx.operation_ids, "0");
   int64_t operation_id_1 = bson_lookup_int64 (&cb_ctx.operation_ids, "1");
   ASSERT_CMPINT64 (operation_id_0, ==, operation_id_1);

   mongoc_bulkwrite_destroy (bw);

   bson_destroy (&cb_ctx.operation_ids);
   bson_destroy (&cb_ctx.ops_counts);
   bson_destroy (&doc);
   mongoc_client_destroy (client);
}

static void
prose_test_5 (void *ctx)
{
   /*
   `MongoClient.bulkWrite` collects `WriteConcernError`s across batches
   */
   mongoc_client_t *client;
   BSON_UNUSED (ctx);
   bool ok;
   bson_error_t error;

   {
      mongoc_uri_t *uri = test_framework_get_uri ();
      mongoc_uri_set_option_as_bool (uri, MONGOC_URI_RETRYWRITES, false);
      client = mongoc_client_new_from_uri (uri);
      test_framework_set_ssl_opts (client);
      mongoc_uri_destroy (uri);
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
               "writeConcernError" : {"code" : 91, "errmsg" : "Replication is being shut down"}
            }
         })),
         NULL,
         NULL,
         &error);
      ASSERT_OR_PRINT (ok, error);
   }

   // Construct models.
   mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);
   {
      bson_t doc = BSON_INITIALIZER;
      BSON_APPEND_UTF8 (&doc, "a", "b");
      for (int32_t i = 0; i < maxWriteBatchSize + 1; i++) {
         ok = mongoc_bulkwrite_append_insertone (bw, "db.coll", -1, &doc, NULL, &error);
         ASSERT_OR_PRINT (ok, error);
      }
   }

   mongoc_bulkwritereturn_t ret = mongoc_bulkwrite_execute (bw, NULL /* options */);
   ASSERT (ret.exc);

   // Expect no top-level error.
   if (mongoc_bulkwriteexception_error (ret.exc, &error)) {
      test_error ("Expected no top-level error but got:\n%s", test_bulkwriteexception_str (ret.exc));
   }

   // Assert two batches were sent.
   ASSERT_CMPUINT32 (bson_count_keys (&cb_ctx.ops_counts), ==, 2);

   // Count write concern errors.
   {
      const bson_t *writeConcernErrors = mongoc_bulkwriteexception_writeconcernerrors (ret.exc);
      ASSERT_CMPUINT32 (bson_count_keys (writeConcernErrors), ==, 2);
   }

   // Assert partial results.
   ASSERT_CMPINT64 (mongoc_bulkwriteresult_insertedcount (ret.res), ==, maxWriteBatchSize + 1);

   bson_destroy (&cb_ctx.operation_ids);
   bson_destroy (&cb_ctx.ops_counts);
   mongoc_bulkwriteexception_destroy (ret.exc);
   mongoc_bulkwriteresult_destroy (ret.res);
   mongoc_bulkwrite_destroy (bw);
   mongoc_client_destroy (client);
}


static void
prose_test_6 (void *ctx)
{
   /*
   `MongoClient.bulkWrite` handles individual `WriteError`s across batches
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
      mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);

      for (int32_t i = 0; i < maxWriteBatchSize + 1; i++) {
         ok = mongoc_bulkwrite_append_insertone (bw, "db.coll", -1, &document, NULL, &error);
         ASSERT_OR_PRINT (ok, error);
      }

      mongoc_bulkwriteopts_t *opts = mongoc_bulkwriteopts_new ();
      mongoc_bulkwriteopts_set_ordered (opts, false);
      mongoc_bulkwriteopts_set_verboseresults (opts, true);
      mongoc_bulkwritereturn_t ret = mongoc_bulkwrite_execute (bw, opts);
      ASSERT (ret.exc);

      if (mongoc_bulkwriteexception_error (ret.exc, &error)) {
         test_error ("Expected no top-level error but got:\n%s", test_bulkwriteexception_str (ret.exc));
      }

      // Assert two batches were sent.
      ASSERT_CMPUINT32 (bson_count_keys (&cb_ctx.ops_counts), ==, 2);

      // Count write errors.
      {
         const bson_t *writeErrors = mongoc_bulkwriteexception_writeerrors (ret.exc);
         ASSERT (bson_in_range_uint32_t_signed (maxWriteBatchSize + 1));
         ASSERT_CMPUINT32 (bson_count_keys (writeErrors), ==, (uint32_t) maxWriteBatchSize + 1);
      }

      // Assert partial results.
      ASSERT_CMPINT64 (mongoc_bulkwriteresult_insertedcount (ret.res), ==, 0);

      mongoc_bulkwriteexception_destroy (ret.exc);
      mongoc_bulkwriteresult_destroy (ret.res);
      mongoc_bulkwriteopts_destroy (opts);
      mongoc_bulkwrite_destroy (bw);
   }

   // Reset state.
   {
      bson_destroy (&cb_ctx.operation_ids);
      bson_destroy (&cb_ctx.ops_counts);
      bson_init (&cb_ctx.operation_ids);
      bson_init (&cb_ctx.ops_counts);
   }

   // Test Ordered
   {
      // Construct models.
      mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);

      for (int32_t i = 0; i < maxWriteBatchSize + 1; i++) {
         ok = mongoc_bulkwrite_append_insertone (bw, "db.coll", -1, &document, NULL, &error);
         ASSERT_OR_PRINT (ok, error);
      }


      mongoc_bulkwriteopts_t *opts = mongoc_bulkwriteopts_new ();
      mongoc_bulkwriteopts_set_ordered (opts, true);
      mongoc_bulkwriteopts_set_verboseresults (opts, true);
      mongoc_bulkwritereturn_t ret = mongoc_bulkwrite_execute (bw, opts);
      ASSERT (ret.exc);

      if (mongoc_bulkwriteexception_error (ret.exc, &error)) {
         test_error ("Expected no top-level error but got:\n%s", test_bulkwriteexception_str (ret.exc));
      }

      // Assert one batch was sent.
      ASSERT_CMPUINT32 (bson_count_keys (&cb_ctx.ops_counts), ==, 1);

      // Count write errors.
      {
         const bson_t *writeErrors = mongoc_bulkwriteexception_writeerrors (ret.exc);
         ASSERT_CMPUINT32 (bson_count_keys (writeErrors), ==, 1);
      }

      // Assert partial results.
      {
         ASSERT_CMPINT64 (mongoc_bulkwriteresult_insertedcount (ret.res), ==, 0);
      }

      bson_destroy (&cb_ctx.operation_ids);
      bson_destroy (&cb_ctx.ops_counts);
      mongoc_bulkwriteexception_destroy (ret.exc);
      mongoc_bulkwriteresult_destroy (ret.res);
      mongoc_bulkwriteopts_destroy (opts);
      mongoc_bulkwrite_destroy (bw);
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

   // Construct models.
   mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);
   size_t numModels = 0;

   mongoc_updateoneopts_t *uo = mongoc_updateoneopts_new ();
   mongoc_updateoneopts_set_upsert (uo, true);
   bson_t *update = BCON_NEW ("$set", "{", "x", BCON_INT32 (1), "}");
   bson_t d1 = BSON_INITIALIZER;
   {
      char *large_str = bson_malloc0 ((maxBsonObjectSize / 2) + 1);
      memset (large_str, 'a', maxBsonObjectSize / 2);
      BSON_APPEND_UTF8 (&d1, "_id", large_str);
      bson_free (large_str);
   }

   ok = mongoc_bulkwrite_append_updateone (bw, "db.coll", -1, &d1, update, uo, &error);
   ASSERT_OR_PRINT (ok, error);
   numModels++;

   bson_t d2 = BSON_INITIALIZER;
   {
      char *large_str = bson_malloc0 ((maxBsonObjectSize / 2) + 1);
      memset (large_str, 'b', maxBsonObjectSize / 2);
      BSON_APPEND_UTF8 (&d2, "_id", large_str);
      bson_free (large_str);
   }

   ok = mongoc_bulkwrite_append_updateone (bw, "db.coll", -1, &d2, update, uo, &error);
   ASSERT_OR_PRINT (ok, error);
   numModels++;

   mongoc_bulkwriteopts_t *opts = mongoc_bulkwriteopts_new ();
   mongoc_bulkwriteopts_set_ordered (opts, false);
   mongoc_bulkwriteopts_set_verboseresults (opts, true);
   mongoc_bulkwritereturn_t ret = mongoc_bulkwrite_execute (bw, opts);

   ASSERT_NO_BULKWRITEEXCEPTION (ret);

   ASSERT_CMPINT64 (mongoc_bulkwriteresult_upsertedcount (ret.res), ==, 2);

   // Check length of update results.
   {
      const bson_t *updateResults = mongoc_bulkwriteresult_updateresults (ret.res);
      ASSERT_CMPSIZE_T ((size_t) bson_count_keys (updateResults), ==, numModels);
   }

   ASSERT_CMPINT (cb_ctx.numGetMore, ==, 1);

   mongoc_bulkwriteopts_destroy (opts);
   bson_destroy (&d2);
   bson_destroy (&d1);
   bson_destroy (update);
   mongoc_bulkwriteexception_destroy (ret.exc);
   mongoc_bulkwriteresult_destroy (ret.res);
   mongoc_updateoneopts_destroy (uo);
   mongoc_bulkwrite_destroy (bw);
   mongoc_collection_destroy (coll);
   bson_destroy (&cb_ctx.operation_ids);
   bson_destroy (&cb_ctx.ops_counts);
   mongoc_client_destroy (client);
}

static void
prose_test_8 (void *ctx)
{
   /*
   `MongoClient.bulkWrite` handles a cursor requiring `getMore` within a transaction
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

   // Construct models.
   mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);
   size_t numModels = 0;

   mongoc_updateoneopts_t *uo = mongoc_updateoneopts_new ();
   mongoc_updateoneopts_set_upsert (uo, true);

   bson_t *update = BCON_NEW ("$set", "{", "x", BCON_INT32 (1), "}");
   mongoc_client_session_t *sess = mongoc_client_start_session (client, NULL, &error);
   ASSERT_OR_PRINT (sess, error);
   ASSERT_OR_PRINT (mongoc_client_session_start_transaction (sess, NULL, &error), error);

   bson_t d1 = BSON_INITIALIZER;
   {
      char *large_str = bson_malloc0 ((maxBsonObjectSize / 2) + 1);
      memset (large_str, 'a', maxBsonObjectSize / 2);
      BSON_APPEND_UTF8 (&d1, "_id", large_str);
      bson_free (large_str);
   }

   ok = mongoc_bulkwrite_append_updateone (bw, "db.coll", -1, &d1, update, uo, &error);
   ASSERT_OR_PRINT (ok, error);
   numModels++;

   bson_t d2 = BSON_INITIALIZER;
   {
      char *large_str = bson_malloc0 ((maxBsonObjectSize / 2) + 1);
      memset (large_str, 'b', maxBsonObjectSize / 2);
      BSON_APPEND_UTF8 (&d2, "_id", large_str);
      bson_free (large_str);
   }

   ok = mongoc_bulkwrite_append_updateone (bw, "db.coll", -1, &d2, update, uo, &error);
   ASSERT_OR_PRINT (ok, error);
   numModels++;

   mongoc_bulkwriteopts_t *opts = mongoc_bulkwriteopts_new ();
   mongoc_bulkwriteopts_set_ordered (opts, false);
   mongoc_bulkwriteopts_set_verboseresults (opts, true);
   mongoc_bulkwritereturn_t ret = mongoc_bulkwrite_execute (bw, opts);

   ASSERT_NO_BULKWRITEEXCEPTION (ret);

   ASSERT_CMPINT64 (mongoc_bulkwriteresult_upsertedcount (ret.res), ==, 2);

   ASSERT_CMPINT (cb_ctx.numGetMore, ==, 1);

   // Check length of update results.
   {
      const bson_t *updateResults = mongoc_bulkwriteresult_updateresults (ret.res);
      ASSERT_CMPSIZE_T ((size_t) bson_count_keys (updateResults), ==, numModels);
   }

   mongoc_updateoneopts_destroy (uo);
   mongoc_bulkwriteopts_destroy (opts);
   bson_destroy (&d2);
   bson_destroy (&d1);
   bson_destroy (update);
   mongoc_client_session_destroy (sess);
   mongoc_bulkwriteexception_destroy (ret.exc);
   mongoc_bulkwriteresult_destroy (ret.res);

   mongoc_bulkwrite_destroy (bw);

   mongoc_collection_destroy (coll);
   bson_destroy (&cb_ctx.operation_ids);
   bson_destroy (&cb_ctx.ops_counts);
   mongoc_client_destroy (client);
}

static void
prose_test_9 (void *ctx)
{
   /*
   `MongoClient.bulkWrite` handles a `getMore` error
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

   bson_t *update = BCON_NEW ("$set", "{", "x", BCON_INT32 (1), "}");

   // Construct models.
   mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);
   size_t numModels = 0;

   mongoc_updateoneopts_t *uo = mongoc_updateoneopts_new ();
   mongoc_updateoneopts_set_upsert (uo, true);

   bson_t d1 = BSON_INITIALIZER;
   {
      char *large_str = bson_malloc0 ((maxBsonObjectSize / 2) + 1);
      memset (large_str, 'a', maxBsonObjectSize / 2);
      BSON_APPEND_UTF8 (&d1, "_id", large_str);
      bson_free (large_str);
   }

   ok = mongoc_bulkwrite_append_updateone (bw, "db.coll", -1, &d1, update, uo, &error);
   ASSERT_OR_PRINT (ok, error);
   numModels++;

   bson_t d2 = BSON_INITIALIZER;
   {
      char *large_str = bson_malloc0 ((maxBsonObjectSize / 2) + 1);
      memset (large_str, 'b', maxBsonObjectSize / 2);
      BSON_APPEND_UTF8 (&d2, "_id", large_str);
      bson_free (large_str);
   }

   ok = mongoc_bulkwrite_append_updateone (bw, "db.coll", -1, &d2, update, uo, &error);
   ASSERT_OR_PRINT (ok, error);
   numModels++;

   mongoc_bulkwriteopts_t *opts = mongoc_bulkwriteopts_new ();
   mongoc_bulkwriteopts_set_ordered (opts, false);
   mongoc_bulkwriteopts_set_verboseresults (opts, true);
   mongoc_bulkwritereturn_t ret = mongoc_bulkwrite_execute (bw, opts);
   ASSERT (ret.exc);

   if (!mongoc_bulkwriteexception_error (ret.exc, &error)) {
      test_error ("Expected top-level error but got:\n%s", test_bulkwriteexception_str (ret.exc));
   }
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_QUERY, 8, "Failing command via 'failCommand' failpoint");

   ASSERT (ret.res);
   ASSERT_CMPSIZE_T ((size_t) mongoc_bulkwriteresult_upsertedcount (ret.res), ==, numModels);

   // Check length of update results.
   {
      const bson_t *updateResults = mongoc_bulkwriteresult_updateresults (ret.res);
      ASSERT_CMPSIZE_T ((size_t) bson_count_keys (updateResults), ==, 1);
   }
   ASSERT_CMPINT (cb_ctx.numGetMore, ==, 1);
   ASSERT_CMPINT (cb_ctx.numKillCursors, ==, 1);

   mongoc_updateoneopts_destroy (uo);
   mongoc_bulkwriteopts_destroy (opts);
   bson_destroy (&d2);
   bson_destroy (&d1);
   bson_destroy (update);
   mongoc_bulkwriteexception_destroy (ret.exc);
   mongoc_bulkwriteresult_destroy (ret.res);

   mongoc_bulkwrite_destroy (bw);
   mongoc_collection_destroy (coll);
   bson_destroy (&cb_ctx.operation_ids);
   bson_destroy (&cb_ctx.ops_counts);
   mongoc_client_destroy (client);
}


static void
prose_test_10 (void *ctx)
{
   /*
   `MongoClient.bulkWrite` returns error for unacknowledged too-large insert
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
   mongoc_bulkwriteopts_t *opts = mongoc_bulkwriteopts_new ();
   mongoc_bulkwriteopts_set_writeconcern (opts, wc);

   // Test a large insert.
   {
      mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);
      ok = mongoc_bulkwrite_append_insertone (bw, "db.coll", -1, &doc, NULL, &error);
      ASSERT_OR_PRINT (ok, error);

      mongoc_bulkwritereturn_t ret = mongoc_bulkwrite_execute (bw, opts);
      ASSERT (ret.exc);
      if (!mongoc_bulkwriteexception_error (ret.exc, &error)) {
         test_error ("Expected top-level error but got:\n%s", test_bulkwriteexception_str (ret.exc));
      }
      ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "of size");

      mongoc_bulkwriteexception_destroy (ret.exc);
      mongoc_bulkwriteresult_destroy (ret.res);
      mongoc_bulkwrite_destroy (bw);
   }

   // Test a large replace.
   {
      mongoc_bulkwrite_t *bw = mongoc_client_bulkwrite_new (client);
      ok = mongoc_bulkwrite_append_replaceone (bw, "db.coll", -1, tmp_bson ("{}"), &doc, NULL, &error);
      ASSERT_OR_PRINT (ok, error);

      mongoc_bulkwritereturn_t ret = mongoc_bulkwrite_execute (bw, opts);
      ASSERT (ret.exc);
      if (!mongoc_bulkwriteexception_error (ret.exc, &error)) {
         test_error ("Expected top-level error but got:\n%s", test_bulkwriteexception_str (ret.exc));
      }
      ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "of size");

      mongoc_bulkwriteexception_destroy (ret.exc);
      mongoc_bulkwriteresult_destroy (ret.res);
      mongoc_bulkwrite_destroy (bw);
   }

   mongoc_bulkwriteopts_destroy (opts);
   bson_destroy (&doc);
   mongoc_write_concern_destroy (wc);
   mongoc_client_destroy (client);
}

static int
skip_if_no_SERVER_89464 (void)
{
   bool is_mongos = test_framework_is_mongos ();
   bool has_SERVER_89464 = test_framework_getenv_bool ("HAS_SERVER_89464");
   if (is_mongos && !has_SERVER_89464) {
      printf ("Skipping test. Detected mongos without changes of SERVER-89464");
      return 0;
   }

   return 1;
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
                      skip_if_no_SERVER_89464);


   TestSuite_AddFull (suite,
                      "/crud/prose_test_7",
                      prose_test_7,
                      NULL,                                                 /* dtor */
                      NULL,                                                 /* ctx */
                      test_framework_skip_if_max_wire_version_less_than_25, /* require 8.0+
                                                                               server */
                      skip_if_no_SERVER_89464);


   TestSuite_AddFull (suite,
                      "/crud/prose_test_8",
                      prose_test_8,
                      NULL,                                                /* dtor */
                      NULL,                                                /* ctx */
                      test_framework_skip_if_max_wire_version_less_than_25 /* require 8.0+
                                                                              server */
                      ,
                      test_framework_skip_if_single,
                      skip_if_no_SERVER_89464);

   TestSuite_AddFull (suite,
                      "/crud/prose_test_9",
                      prose_test_9,
                      NULL, /* dtor */
                      NULL, /* ctx */
                      test_framework_skip_if_max_wire_version_less_than_25 /* require 8.0+ server */,
                      skip_if_no_SERVER_89464);

   TestSuite_AddFull (suite,
                      "/crud/prose_test_10",
                      prose_test_10,
                      NULL, /* dtor */
                      NULL, /* ctx */
                      test_framework_skip_if_max_wire_version_less_than_25 /* require 8.0+ server */);
}
