/*
 * Copyright 2019-present MongoDB, Inc.
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

#include "json-test.h"
#include "test-libmongoc.h"

static void
_before_test (json_test_ctx_t *ctx, const bson_t *test)
{
   mongoc_client_t *client;
   mongoc_collection_t *keyvault_coll;
   bson_iter_t iter;
   bson_error_t error;
   bool ret;
   mongoc_write_concern_t *wc;
   bson_t insert_opts;

   /* Insert data into the key vault. */
   client = test_framework_client_new ();
   wc = mongoc_write_concern_new ();
   mongoc_write_concern_set_wmajority (wc, 1000);
   bson_init (&insert_opts);
   mongoc_write_concern_append (wc, &insert_opts);

   if (bson_iter_init_find (&iter, ctx->config->scenario, "key_vault_data")) {
      keyvault_coll =
         mongoc_client_get_collection (client, "admin", "datakeys");

      /* Drop and recreate, inserting data. */
      ret = mongoc_collection_drop (keyvault_coll, &error);
      if (!ret) {
         /* Ignore "namespace does not exist" error. */
         ASSERT_OR_PRINT (error.code == 26, error);
      }

      bson_iter_recurse (&iter, &iter);
      while (bson_iter_next (&iter)) {
         bson_t doc;

         bson_iter_bson (&iter, &doc);
         ret = mongoc_collection_insert_one (
            keyvault_coll, &doc, &insert_opts, NULL /* reply */, &error);
         ASSERT_OR_PRINT (ret, error);
      }
      mongoc_collection_destroy (keyvault_coll);
   }

   /* Collmod to include the json schema. Data was already inserted. */
   if (bson_iter_init_find (&iter, ctx->config->scenario, "json_schema")) {
      bson_t *cmd;
      bson_t json_schema;

      bson_iter_bson (&iter, &json_schema);
      cmd = BCON_NEW ("collMod",
                      BCON_UTF8 (mongoc_collection_get_name (ctx->collection)),
                      "validator",
                      "{",
                      "$jsonSchema",
                      BCON_DOCUMENT (&json_schema),
                      "}");
      ret = mongoc_client_command_simple (
         client, mongoc_database_get_name (ctx->db), cmd, NULL, NULL, &error);
      ASSERT_OR_PRINT (ret, error);
      bson_destroy (cmd);
   }

   bson_destroy (&insert_opts);
   mongoc_write_concern_destroy (wc);
   mongoc_client_destroy (client);
}

static bool
_run_operation (json_test_ctx_t *ctx,
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
test_client_side_encryption_cb (bson_t *scenario)
{
   json_test_config_t config = JSON_TEST_CONFIG_INIT;
   config.before_test_cb = _before_test;
   config.run_operation_cb = _run_operation;
   config.scenario = scenario;
   config.command_started_events_only = true;
   config.command_monitoring_allow_subset = false;
   run_json_general_test (&config);
}

#define LOCAL_MASTERKEY                                                       \
   "\x32\x78\x34\x34\x2b\x78\x64\x75\x54\x61\x42\x42\x6b\x59\x31\x36\x45\x72" \
   "\x35\x44\x75\x41\x44\x61\x67\x68\x76\x53\x34\x76\x77\x64\x6b\x67\x38\x74" \
   "\x70\x50\x70\x33\x74\x7a\x36\x67\x56\x30\x31\x41\x31\x43\x77\x62\x44\x39" \
   "\x69\x74\x51\x32\x48\x46\x44\x67\x50\x57\x4f\x70\x38\x65\x4d\x61\x43\x31" \
   "\x4f\x69\x37\x36\x36\x4a\x7a\x58\x5a\x42\x64\x42\x64\x62\x64\x4d\x75\x72" \
   "\x64\x6f\x6e\x4a\x31\x64"

typedef struct {
   int num_inserts;
} limits_apm_ctx_t;

static void
_command_started (const mongoc_apm_command_started_t *event)
{
   limits_apm_ctx_t *ctx;

   ctx = (limits_apm_ctx_t *) mongoc_apm_command_started_get_context (event);
   if (0 ==
       strcmp ("insert", mongoc_apm_command_started_get_command_name (event))) {
      ctx->num_inserts++;
   }
}

/* Prose test: BSON size limits and batch splitting */
static void
test_bson_size_limits_and_batch_splitting (void *unused)
{
   /* Expect an insert of two documents over 2MiB to split into two inserts but
    * still succeed. */
   mongoc_client_t *client;
   mongoc_auto_encryption_opts_t *opts;
   mongoc_uri_t *uri;
   mongoc_collection_t *coll;
   bson_error_t error;
   bson_t *corpus_schema;
   bson_t *datakey;
   bson_t *cmd;
   bson_t *kms_providers;
   bson_t *docs[2];
   char *as;
   limits_apm_ctx_t ctx = {0};
   mongoc_apm_callbacks_t *callbacks;

   /* Do the test setup. */

   /* Drop and create db.coll configured with limits-schema.json */
   uri = test_framework_get_uri ();
   client = mongoc_client_new_from_uri (uri);
   mongoc_client_set_error_api (client, MONGOC_ERROR_API_VERSION_2);
   coll = mongoc_client_get_collection (client, "db", "coll");
   (void) mongoc_collection_drop (coll, NULL);
   corpus_schema = get_bson_from_json_file (
      "./src/libmongoc/tests/client_side_encryption_prose/limits-schema.json");
   cmd = BCON_NEW ("create",
                   "coll",
                   "validator",
                   "{",
                   "$jsonSchema",
                   BCON_DOCUMENT (corpus_schema),
                   "}");
   ASSERT_OR_PRINT (
      mongoc_client_command_simple (
         client, "db", cmd, NULL /* read prefs */, NULL /* reply */, &error),
      error);

   /* Drop and create the key vault collection, admin.datakeys. */
   mongoc_collection_destroy (coll);
   coll = mongoc_client_get_collection (client, "admin", "datakeys");
   (void) mongoc_collection_drop (coll, NULL);
   datakey = get_bson_from_json_file (
      "./src/libmongoc/tests/client_side_encryption_prose/limits-key.json");
   ASSERT_OR_PRINT (
      mongoc_collection_insert_one (
         coll, datakey, NULL /* opts */, NULL /* reply */, &error),
      error);

   mongoc_collection_destroy (coll);
   mongoc_client_destroy (client);
   client = mongoc_client_new_from_uri (uri);
   mongoc_client_set_error_api (client, MONGOC_ERROR_API_VERSION_2);

   kms_providers = BCON_NEW (
      "local", "{", "key", BCON_BIN (0, (uint8_t *) LOCAL_MASTERKEY, 96), "}");
   opts = mongoc_auto_encryption_opts_new ();
   if (test_framework_getenv_bool ("MONGOC_TEST_MONGOCRYPTD_BYPASS_SPAWN")) {
      bson_t *extra;

      extra = BCON_NEW ("mongocryptdBypassSpawn", BCON_BOOL (true));
      mongoc_auto_encryption_opts_set_extra (opts, extra);
      bson_destroy (extra);
   }
   mongoc_auto_encryption_opts_set_keyvault_namespace (
      opts, "admin", "datakeys");
   mongoc_auto_encryption_opts_set_kms_providers (opts, kms_providers);

   ASSERT_OR_PRINT (mongoc_client_enable_auto_encryption (client, opts, &error),
                    error);

   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_started_cb (callbacks, _command_started);
   mongoc_client_set_apm_callbacks (client, callbacks, &ctx);

   coll = mongoc_client_get_collection (client, "db", "coll");

   /* Insert { "_id": "over_2mib_under_16mib", "unencrypted": <the string "a"
    * repeated 2097152 times> } */
   docs[0] = BCON_NEW ("_id", "over_2mib_under_16mib");
   as = bson_malloc (16777216);
   memset (as, 'a', 16777216);
   bson_append_utf8 (docs[0], "unencrypted", -1, as, 2097152);
   ASSERT_OR_PRINT (
      mongoc_collection_insert_one (
         coll, docs[0], NULL /* opts */, NULL /* reply */, &error),
      error);
   bson_destroy (docs[0]);

   /* Insert the document `limits/limits-doc.json <../limits/limits-doc.json>`_
    * concatenated with ``{ "_id": "encryption_exceeds_2mib", "unencrypted": <
    * the string "a" repeated (2097152 - 2000) times > }`` */
   docs[0] = get_bson_from_json_file (
      "./src/libmongoc/tests/client_side_encryption_prose/limits-doc.json");
   bson_append_utf8 (docs[0], "_id", -1, "encryption_exceeds_2mib", -1);
   bson_append_utf8 (docs[0], "unencrypted", -1, as, 2097152 - 2000);
   ASSERT_OR_PRINT (
      mongoc_collection_insert_one (
         coll, docs[0], NULL /* opts */, NULL /* reply */, &error),
      error);
   bson_destroy (docs[0]);

   /* Bulk insert the following:

   - ``{ "_id": "over_2mib_1", "unencrypted": <the string "a" repeated (2097152)
   times> }``

   - ``{ "_id": "over_2mib_2", "unencrypted": <the string "a" repeated (2097152)
   times> }``

   Expect the bulk write to succeed and split after first doc (i.e. two inserts
   occur). This may be verified using `command monitoring
   <https://github.com/mongodb/specifications/tree/master/source/command-monitoring/command-monitoring.rst>`_.
   */
   docs[0] = BCON_NEW ("_id", "over_2mib_1");
   bson_append_utf8 (docs[0], "unencrypted", -1, as, 2097152);
   docs[1] = BCON_NEW ("_id", "over_2mib_2");
   bson_append_utf8 (docs[1], "unencrypted", -1, as, 2097152);
   ctx.num_inserts = 0;
   ASSERT_OR_PRINT (mongoc_collection_insert_many (coll,
                                                   (const bson_t **) docs,
                                                   2,
                                                   NULL /* opts */,
                                                   NULL /* reply */,
                                                   &error),
                    error);
   ASSERT_CMPINT (ctx.num_inserts, ==, 2);
   bson_destroy (docs[0]);
   bson_destroy (docs[1]);

   /* #. Bulk insert the following:

   - The document `limits/limits-doc.json <../limits/limits-doc.json>`_
   concatenated with ``{ "_id": "encryption_exceeds_2mib_1", "unencrypted": <
   the string "a" repeated (2097152 - 2000) times > }``

   - The document `limits/limits-doc.json <../limits/limits-doc.json>`_
   concatenated with ``{ "_id": "encryption_exceeds_2mib_2", "unencrypted": <
   the string "a" repeated (2097152 - 2000) times > }``

   Expect the bulk write to succeed and split after first doc (i.e. two inserts
   occur). This may be verified using `command monitoring
   <https://github.com/mongodb/specifications/tree/master/source/command-monitoring/command-monitoring.rst>`_.
   */

   docs[0] = get_bson_from_json_file (
      "./src/libmongoc/tests/client_side_encryption_prose/limits-doc.json");
   bson_append_utf8 (docs[0], "_id", -1, "encryption_exceeds_2mib_1", -1);
   bson_append_utf8 (docs[0], "unencrypted", -1, as, 2097152 - 2000);
   docs[1] = get_bson_from_json_file (
      "./src/libmongoc/tests/client_side_encryption_prose/limits-doc.json");
   bson_append_utf8 (docs[1], "_id", -1, "encryption_exceeds_2mib_2", -1);
   bson_append_utf8 (docs[1], "unencrypted", -1, as, 2097152 - 2000);
   ctx.num_inserts = 0;
   ASSERT_OR_PRINT (mongoc_collection_insert_many (coll,
                                                   (const bson_t **) docs,
                                                   2,
                                                   NULL /* opts */,
                                                   NULL /* reply */,
                                                   &error),
                    error);
   ASSERT_CMPINT (ctx.num_inserts, ==, 2);
   bson_destroy (docs[0]);
   bson_destroy (docs[1]);

   /* Check that inserting close to, but not exceeding, 16MiB, passes */
   docs[0] = bson_new ();
   bson_append_utf8 (docs[0], "_id", -1, "under_16mib", -1);
   bson_append_utf8 (docs[0], "unencrypted", -1, as, 16777216 - 2000);
   ASSERT_OR_PRINT (
      mongoc_collection_insert_one (
         coll, docs[0], NULL /* opts */, NULL /* reply */, &error),
      error);
   bson_destroy (docs[0]);

   /* but.. */
   docs[0] = get_bson_from_json_file (
      "./src/libmongoc/tests/client_side_encryption_prose/limits-doc.json");
   bson_append_utf8 (docs[0], "_id", -1, "under_16mib", -1);
   bson_append_utf8 (docs[0], "unencrypted", -1, as, 16777216 - 2000);
   BSON_ASSERT (!mongoc_collection_insert_one (
      coll, docs[0], NULL /* opts */, NULL /* reply */, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_SERVER, 2, "too large");
   bson_destroy (docs[0]);

   bson_free (as);
   bson_destroy (kms_providers);
   bson_destroy (corpus_schema);
   bson_destroy (cmd);
   bson_destroy (datakey);
   mongoc_collection_destroy (coll);
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
   mongoc_apm_callbacks_destroy (callbacks);
   mongoc_auto_encryption_opts_destroy (opts);
}


static void
_reset (mongoc_client_pool_t **pool,
        mongoc_client_t **singled_threaded_client,
        mongoc_client_t **multi_threaded_client,
        mongoc_auto_encryption_opts_t **opts,
        bool recreate)
{
   bson_t *kms_providers;
   mongoc_uri_t *uri;

   mongoc_auto_encryption_opts_destroy (*opts);
   *opts = mongoc_auto_encryption_opts_new ();
   if (test_framework_getenv_bool ("MONGOC_TEST_MONGOCRYPTD_BYPASS_SPAWN")) {
      bson_t *extra;

      extra = BCON_NEW ("mongocryptdBypassSpawn", BCON_BOOL (true));
      mongoc_auto_encryption_opts_set_extra (*opts, extra);
      bson_destroy (extra);
   }
   mongoc_auto_encryption_opts_set_keyvault_namespace (*opts, "db", "keyvault");
   kms_providers = BCON_NEW (
      "local", "{", "key", BCON_BIN (0, (uint8_t *) LOCAL_MASTERKEY, 96), "}");
   mongoc_auto_encryption_opts_set_kms_providers (*opts, kms_providers);

   if (*multi_threaded_client) {
      mongoc_client_pool_push (*pool, *multi_threaded_client);
   }

   mongoc_client_destroy (*singled_threaded_client);
   mongoc_client_pool_destroy (*pool);

   if (recreate) {
      uri = test_framework_get_uri ();
      *pool = mongoc_client_pool_new (uri);
      *singled_threaded_client = mongoc_client_new_from_uri (uri);
      *multi_threaded_client = mongoc_client_pool_pop (*pool);
      mongoc_uri_destroy (uri);
   }
   bson_destroy (kms_providers);
}

static void
test_invalid_single_and_pool_mismatches (void *unused)
{
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *single_threaded_client = NULL;
   mongoc_client_t *multi_threaded_client = NULL;
   mongoc_auto_encryption_opts_t *opts = NULL;
   bson_error_t error;
   bool ret;

   _reset (&pool, &single_threaded_client, &multi_threaded_client, &opts, true);

   /* single threaded client, single threaded setter => ok */
   ret = mongoc_client_enable_auto_encryption (
      single_threaded_client, opts, &error);
   ASSERT_OR_PRINT (ret, error);

   /* multi threaded client, single threaded setter => bad */
   ret = mongoc_client_enable_auto_encryption (
      multi_threaded_client, opts, &error);
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_CLIENT,
                          MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                          "Cannot enable auto encryption on a pooled client");

   /* pool - pool setter */
   ret = mongoc_client_pool_enable_auto_encryption (pool, opts, &error);
   ASSERT_OR_PRINT (ret, error);

   /* single threaded client, single threaded key vault client => ok */
   _reset (&pool, &single_threaded_client, &multi_threaded_client, &opts, true);
   mongoc_auto_encryption_opts_set_keyvault_client (opts,
                                                    single_threaded_client);
   ret = mongoc_client_enable_auto_encryption (
      single_threaded_client, opts, &error);
   ASSERT_OR_PRINT (ret, error);

   /* single threaded client, multi threaded key vault client => ok */
   _reset (&pool, &single_threaded_client, &multi_threaded_client, &opts, true);
   mongoc_auto_encryption_opts_set_keyvault_client (opts,
                                                    multi_threaded_client);
   ret = mongoc_client_enable_auto_encryption (
      single_threaded_client, opts, &error);
   ASSERT_OR_PRINT (ret, error);

   /* single threaded client, pool key vault client => bad */
   _reset (&pool, &single_threaded_client, &multi_threaded_client, &opts, true);
   mongoc_auto_encryption_opts_set_keyvault_client_pool (opts, pool);
   ret = mongoc_client_enable_auto_encryption (
      single_threaded_client, opts, &error);
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_CLIENT,
                          MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                          "The key vault client pool only applies to a client "
                          "pool, not a single threaded client");

   /* pool, singled threaded key vault client => bad */
   _reset (&pool, &single_threaded_client, &multi_threaded_client, &opts, true);
   mongoc_auto_encryption_opts_set_keyvault_client (opts,
                                                    single_threaded_client);
   ret = mongoc_client_pool_enable_auto_encryption (pool, opts, &error);
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_CLIENT,
                          MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                          "The key vault client only applies to a single "
                          "threaded client not a single threaded client. Set a "
                          "key vault client pool");

   /* pool, multi threaded key vault client => bad */
   _reset (&pool, &single_threaded_client, &multi_threaded_client, &opts, true);
   mongoc_auto_encryption_opts_set_keyvault_client (opts,
                                                    multi_threaded_client);
   ret = mongoc_client_pool_enable_auto_encryption (pool, opts, &error);
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_CLIENT,
                          MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                          "The key vault client only applies to a single "
                          "threaded client not a single threaded client. Set a "
                          "key vault client pool");

   /* pool, pool key vault client => ok */
   _reset (&pool, &single_threaded_client, &multi_threaded_client, &opts, true);
   mongoc_auto_encryption_opts_set_keyvault_client_pool (opts, pool);
   ret = mongoc_client_pool_enable_auto_encryption (pool, opts, &error);
   ASSERT_OR_PRINT (ret, error);

   /* double enabling */
   _reset (&pool, &single_threaded_client, &multi_threaded_client, &opts, true);
   ret = mongoc_client_enable_auto_encryption (
      single_threaded_client, opts, &error);
   ASSERT_OR_PRINT (ret, error);
   ret = mongoc_client_enable_auto_encryption (
      single_threaded_client, opts, &error);
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_CLIENT,
                          MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                          "Automatic encryption already set");
   ret = mongoc_client_pool_enable_auto_encryption (pool, opts, &error);
   ASSERT_OR_PRINT (ret, error);
   ret = mongoc_client_pool_enable_auto_encryption (pool, opts, &error);
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_CLIENT,
                          MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                          "Automatic encryption already set");

   _reset (
      &pool, &single_threaded_client, &multi_threaded_client, &opts, false);
   mongoc_auto_encryption_opts_destroy (opts);
}

/* Convenience helper for creating auto encryption opts */
/* TODO: replace other manual calls to KMS construction  with this helper */
static bson_t *
_make_kms_providers (bool with_aws, bool with_local)
{
   bson_t *kms_providers = bson_new ();

   if (with_aws) {
      char *aws_secret_access_key;
      char *aws_access_key_id;

      aws_secret_access_key =
         test_framework_getenv ("MONGOC_TEST_AWS_SECRET_ACCESS_KEY");
      aws_access_key_id =
         test_framework_getenv ("MONGOC_TEST_AWS_ACCESS_KEY_ID");

      if (!aws_secret_access_key || !aws_access_key_id) {
         fprintf (stderr,
                  "Set MONGOC_TEST_AWS_SECRET_ACCESS_KEY and "
                  "MONGOC_TEST_AWS_ACCESS_KEY_ID environment "
                  "variables to run Client Side Encryption tests.");
         abort ();
      }

      bson_concat (
         kms_providers,
         tmp_bson ("{ 'aws': { 'secretAccessKey': '%s', 'accessKeyId': '%s' }}",
                   aws_secret_access_key,
                   aws_access_key_id));

      bson_free (aws_secret_access_key);
      bson_free (aws_access_key_id);
   }
   if (with_local) {
      bson_t *local = BCON_NEW ("local",
                                "{",
                                "key",
                                BCON_BIN (0, (uint8_t *) LOCAL_MASTERKEY, 96),
                                "}");
      bson_concat (kms_providers, local);
      bson_destroy (local);
   }

   return kms_providers;
}

/* Convenience helper for maybe bypassing spawning mongocryptd */
/* TODO: replace other manual instances of this with a call to this.  */
static void
_check_bypass (mongoc_auto_encryption_opts_t *opts)
{
   if (test_framework_getenv_bool ("MONGOC_TEST_MONGOCRYPTD_BYPASS_SPAWN")) {
      bson_t *extra;

      extra = BCON_NEW ("mongocryptdBypassSpawn", BCON_BOOL (true));
      mongoc_auto_encryption_opts_set_extra (opts, extra);
      bson_destroy (extra);
   }
}

/* TODO Then implement a multi-threaded test. */
/* Also test with external key vault client pool. */
static void
test_multi_threaded (void *unused)
{
   mongoc_uri_t *uri;
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   mongoc_auto_encryption_opts_t *opts;
   bson_t *datakey;
   mongoc_collection_t *coll;
   bson_t *schema;
   bson_t *schema_map;
   bool ret;
   bson_error_t error;
   bson_t *kms_providers;

   uri = test_framework_get_uri ();
   pool = mongoc_client_pool_new (uri);
   client = mongoc_client_new_from_uri (uri);
   opts = mongoc_auto_encryption_opts_new ();

   coll = mongoc_client_get_collection (client, "db", "keyvault");
   (void) mongoc_collection_drop (coll, NULL);
   datakey = get_bson_from_json_file (
      "./src/libmongoc/tests/client_side_encryption_prose/limits-key.json");
   BSON_ASSERT (datakey);
   ASSERT_OR_PRINT (
      mongoc_collection_insert_one (
         coll, datakey, NULL /* opts */, NULL /* reply */, &error),
      error);

   mongoc_client_destroy (client);

   /* create pool with auto encryption */
   if (test_framework_getenv_bool ("MONGOC_TEST_MONGOCRYPTD_BYPASS_SPAWN")) {
      bson_t *extra;

      extra = BCON_NEW ("mongocryptdBypassSpawn", BCON_BOOL (true));
      mongoc_auto_encryption_opts_set_extra (opts, extra);
      bson_destroy (extra);
   }

   mongoc_auto_encryption_opts_set_keyvault_namespace (opts, "db", "keyvault");
   kms_providers = BCON_NEW (
      "local", "{", "key", BCON_BIN (0, (uint8_t *) LOCAL_MASTERKEY, 96), "}");
   mongoc_auto_encryption_opts_set_kms_providers (opts, kms_providers);

   schema = get_bson_from_json_file (
      "./src/libmongoc/tests/client_side_encryption_prose/schema.json");
   BSON_ASSERT (schema);
   schema_map = BCON_NEW ("db.coll", BCON_DOCUMENT (schema));
   mongoc_auto_encryption_opts_set_schema_map (opts, schema_map);
   ret = mongoc_client_pool_enable_auto_encryption (pool, opts, &error);
   ASSERT_OR_PRINT (ret, error);

   client = mongoc_client_pool_pop (pool);
   mongoc_collection_destroy (coll);
   coll = mongoc_client_get_collection (client, "db", "coll");
   ret = mongoc_collection_insert_one (
      coll, tmp_bson ("{'encrypted_string': 'abc'}"), NULL, NULL, &error);
   ASSERT_OR_PRINT (ret, error);

   mongoc_collection_destroy (coll);
   mongoc_client_pool_push (pool, client);
   mongoc_client_pool_destroy (pool);
   bson_destroy (schema);
   bson_destroy (schema_map);
   bson_destroy (datakey);
   mongoc_auto_encryption_opts_destroy (opts);
   mongoc_uri_destroy (uri);
   bson_destroy (kms_providers);
}

typedef struct {
   bson_t *last_cmd;
} _datakey_and_double_encryption_ctx_t;

static void
_datakey_and_double_encryption_command_started (
   const mongoc_apm_command_started_t *event)
{
   _datakey_and_double_encryption_ctx_t *ctx;

   ctx = (_datakey_and_double_encryption_ctx_t *)
      mongoc_apm_command_started_get_context (event);
   bson_destroy (ctx->last_cmd);
   ctx->last_cmd = bson_copy (mongoc_apm_command_started_get_command (event));
}

static void
test_datakey_and_double_encryption_creating_and_using (
   mongoc_client_encryption_t *client_encryption,
   mongoc_client_t *client,
   mongoc_client_t *client_encrypted,
   const char *kms_provider,
   _datakey_and_double_encryption_ctx_t *test_ctx)
{
   bson_value_t keyid;
   bson_error_t error;
   bool ret;
   mongoc_client_encryption_datakey_opts_t *opts;
   mongoc_client_encryption_encrypt_opts_t *encrypt_opts;
   char *altname;
   mongoc_collection_t *coll;
   mongoc_cursor_t *cursor;
   bson_t filter;
   const bson_t *doc;
   bson_value_t to_encrypt;
   bson_value_t encrypted;
   bson_value_t encrypted_via_altname;
   bson_t to_insert = BSON_INITIALIZER;
   char *hello;

   opts = mongoc_client_encryption_datakey_opts_new ();

   if (0 == strcmp (kms_provider, "aws")) {
      mongoc_client_encryption_datakey_opts_set_masterkey (
         opts,
         tmp_bson ("{ region: 'us-east-1', key: "
                   "'arn:aws:kms:us-east-1:579766882180:key/"
                   "89fcc2c4-08b0-4bd9-9f25-e30687b580d0' }"));
   }

   altname = bson_strdup_printf ("%s_altname", kms_provider);
   mongoc_client_encryption_datakey_opts_set_keyaltnames (opts, &altname, 1);

   ret = mongoc_client_encryption_create_datakey (
      client_encryption, kms_provider, opts, &keyid, &error);
   ASSERT_OR_PRINT (ret, error);

   /* Expect a BSON binary with subtype 4 to be returned */
   BSON_ASSERT (keyid.value_type == BSON_TYPE_BINARY);
   BSON_ASSERT (keyid.value.v_binary.subtype = BSON_SUBTYPE_UUID);

   /* Check that client captured a command_started event for the insert command
    * containing a majority writeConcern. */
   BSON_ASSERT (match_bson (
      test_ctx->last_cmd,
      tmp_bson ("{'insert': 'datakeys', 'writeConcern': { 'w': 'majority' } }"),
      false));

   /* Use client to run a find on admin.datakeys */
   coll = mongoc_client_get_collection (client, "admin", "datakeys");
   bson_init (&filter);
   BSON_APPEND_VALUE (&filter, "_id", &keyid);
   cursor = mongoc_collection_find_with_opts (
      coll, &filter, NULL /* opts */, NULL /* read prefs */);
   mongoc_collection_destroy (coll);

   /* Expect that exactly one document is returned with the "masterKey.provider"
    * equal to <kms_provider> */
   BSON_ASSERT (mongoc_cursor_next (cursor, &doc));
   BSON_ASSERT (
      0 == strcmp (kms_provider, bson_lookup_utf8 (doc, "masterKey.provider")));
   BSON_ASSERT (!mongoc_cursor_next (cursor, &doc));
   ASSERT_OR_PRINT (!mongoc_cursor_error (cursor, &error), error);
   mongoc_cursor_destroy (cursor);

   /* Call client_encryption.encrypt() with the value "hello local" */
   encrypt_opts = mongoc_client_encryption_encrypt_opts_new ();
   mongoc_client_encryption_encrypt_opts_set_algorithm (
      encrypt_opts, "AEAD_AES_256_CBC_HMAC_SHA_512-Deterministic");
   mongoc_client_encryption_encrypt_opts_set_keyid (encrypt_opts, &keyid);

   hello = bson_strdup_printf ("hello %s", kms_provider);

   to_encrypt.value_type = BSON_TYPE_UTF8;
   to_encrypt.value.v_utf8.str = bson_strdup (hello);
   to_encrypt.value.v_utf8.len = strlen (to_encrypt.value.v_utf8.str);

   ret = mongoc_client_encryption_encrypt (
      client_encryption, &to_encrypt, encrypt_opts, &encrypted, &error);
   ASSERT_OR_PRINT (ret, error);
   mongoc_client_encryption_encrypt_opts_destroy (encrypt_opts);

   /* Expect the return value to be a BSON binary subtype 6 */
   BSON_ASSERT (encrypted.value_type == BSON_TYPE_BINARY);
   BSON_ASSERT (encrypted.value.v_binary.subtype = BSON_SUBTYPE_ENCRYPTED);

   /* Use client_encrypted to insert { _id: "<kms provider>", "value":
    * <encrypted> } into db.coll */
   coll = mongoc_client_get_collection (client_encrypted, "db", "coll");
   BSON_APPEND_UTF8 (&to_insert, "_id", kms_provider);
   BSON_APPEND_VALUE (&to_insert, "value", &encrypted);
   ret = mongoc_collection_insert_one (
      coll, &to_insert, NULL /* opts */, NULL /* reply */, &error);
   ASSERT_OR_PRINT (ret, error);

   /* Use client_encrypted to run a find querying with _id of <kms_provider> and
    * expect value to be "hello <kms_provider>". */
   cursor = mongoc_collection_find_with_opts (
      coll,
      tmp_bson ("{ '_id': '%s' }", kms_provider),
      NULL /* opts */,
      NULL /* read prefs */);
   BSON_ASSERT (mongoc_cursor_next (cursor, &doc));
   BSON_ASSERT (0 == strcmp (hello, bson_lookup_utf8 (doc, "value")));
   BSON_ASSERT (!mongoc_cursor_next (cursor, &doc));
   ASSERT_OR_PRINT (!mongoc_cursor_error (cursor, &error), error);
   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (coll);

   /* Call client_encryption.encrypt() with the value "hello <kms provider>",
    * the algorithm AEAD_AES_256_CBC_HMAC_SHA_512-Deterministic, and the
    * key_alt_name of <kms provider>_altname. */
   encrypt_opts = mongoc_client_encryption_encrypt_opts_new ();
   mongoc_client_encryption_encrypt_opts_set_algorithm (
      encrypt_opts, "AEAD_AES_256_CBC_HMAC_SHA_512-Deterministic");
   mongoc_client_encryption_encrypt_opts_set_keyaltname (encrypt_opts, altname);

   ret = mongoc_client_encryption_encrypt (client_encryption,
                                           &to_encrypt,
                                           encrypt_opts,
                                           &encrypted_via_altname,
                                           &error);
   ASSERT_OR_PRINT (ret, error);
   mongoc_client_encryption_encrypt_opts_destroy (encrypt_opts);

   /* Expect the return value to be a BSON binary subtype 6. Expect the value to
    * exactly match the value of encrypted. */
   BSON_ASSERT (encrypted_via_altname.value_type == BSON_TYPE_BINARY);
   BSON_ASSERT (encrypted_via_altname.value.v_binary.subtype =
                   BSON_SUBTYPE_ENCRYPTED);
   BSON_ASSERT (encrypted_via_altname.value.v_binary.data_len ==
                encrypted.value.v_binary.data_len);
   BSON_ASSERT (0 == memcmp (encrypted_via_altname.value.v_binary.data,
                             encrypted.value.v_binary.data,
                             encrypted.value.v_binary.data_len));

   bson_value_destroy (&encrypted);
   bson_value_destroy (&encrypted_via_altname);
   bson_free (hello);
   bson_destroy (&to_insert);
   bson_value_destroy (&to_encrypt);
   bson_value_destroy (&keyid);
   bson_free (altname);
   bson_destroy (&filter);
   mongoc_client_encryption_datakey_opts_destroy (opts);
}

/* Prose test "Data key and double encryption" */
static void
test_datakey_and_double_encryption (void *unused)
{
   mongoc_client_t *client;
   mongoc_client_t *client_encrypted;
   mongoc_client_encryption_t *client_encryption;
   mongoc_apm_callbacks_t *callbacks;
   mongoc_collection_t *coll;
   bson_error_t error;
   bson_t *kms_providers;
   mongoc_auto_encryption_opts_t *auto_encryption_opts;
   mongoc_client_encryption_opts_t *client_encryption_opts;
   bson_t *schema_map;
   bool ret;
   _datakey_and_double_encryption_ctx_t test_ctx = {0};

   /* Test setup */
   /* Create a MongoClient without encryption enabled (referred to as client).
    * Enable command monitoring to listen for command_started events. */
   client = test_framework_client_new ();
   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_started_cb (
      callbacks, _datakey_and_double_encryption_command_started);
   mongoc_client_set_apm_callbacks (client, callbacks, &test_ctx);

   /* Using client, drop the collections admin.datakeys and db.coll. */
   coll = mongoc_client_get_collection (client, "admin", "datakeys");
   (void) mongoc_collection_drop (coll, NULL);
   mongoc_collection_destroy (coll);
   coll = mongoc_client_get_collection (client, "db", "coll");
   (void) mongoc_collection_drop (coll, NULL);
   mongoc_collection_destroy (coll);

   /* Create a MongoClient configured with auto encryption (referred to as
    * client_encrypted) */
   auto_encryption_opts = mongoc_auto_encryption_opts_new ();
   kms_providers = _make_kms_providers (true /* aws */, true /* local */);
   if (test_framework_getenv_bool ("MONGOC_TEST_MONGOCRYPTD_BYPASS_SPAWN")) {
      bson_t *extra;

      extra = BCON_NEW ("mongocryptdBypassSpawn", BCON_BOOL (true));
      mongoc_auto_encryption_opts_set_extra (auto_encryption_opts, extra);
      bson_destroy (extra);
   }
   mongoc_auto_encryption_opts_set_kms_providers (auto_encryption_opts,
                                                  kms_providers);
   mongoc_auto_encryption_opts_set_keyvault_namespace (
      auto_encryption_opts, "admin", "datakeys");
   schema_map = get_bson_from_json_file (
      "./src/libmongoc/tests/client_side_encryption_prose/"
      "datakey-and-double-encryption-schemamap.json");
   mongoc_auto_encryption_opts_set_schema_map (auto_encryption_opts,
                                               schema_map);

   client_encrypted = test_framework_client_new ();
   ret = mongoc_client_enable_auto_encryption (
      client_encrypted, auto_encryption_opts, &error);
   ASSERT_OR_PRINT (ret, error);

   /* Create a ClientEncryption object (referred to as client_encryption) */
   client_encryption_opts = mongoc_client_encryption_opts_new ();
   mongoc_client_encryption_opts_set_kms_providers (client_encryption_opts,
                                                    kms_providers);
   mongoc_client_encryption_opts_set_keyvault_namespace (
      client_encryption_opts, "admin", "datakeys");
   mongoc_client_encryption_opts_set_keyvault_client (client_encryption_opts,
                                                      client);
   client_encryption =
      mongoc_client_encryption_new (client_encryption_opts, &error);
   ASSERT_OR_PRINT (client_encryption, error);

   test_datakey_and_double_encryption_creating_and_using (
      client_encryption, client, client_encrypted, "local", &test_ctx);
   test_datakey_and_double_encryption_creating_and_using (
      client_encryption, client, client_encrypted, "aws", &test_ctx);

   bson_destroy (kms_providers);
   bson_destroy (schema_map);
   mongoc_client_encryption_opts_destroy (client_encryption_opts);
   mongoc_auto_encryption_opts_destroy (auto_encryption_opts);
   mongoc_apm_callbacks_destroy (callbacks);
   mongoc_client_destroy (client);
   mongoc_client_destroy (client_encrypted);
   mongoc_client_encryption_destroy (client_encryption);
   bson_destroy (test_ctx.last_cmd);
}

static void
_test_key_vault (bool with_external_key_vault)
{
   mongoc_client_t *client;
   mongoc_client_t *client_encrypted;
   mongoc_client_t *client_external;
   mongoc_client_encryption_t *client_encryption;
   mongoc_uri_t *external_uri;
   mongoc_collection_t *coll;
   bson_t *datakey;
   bson_t *kms_providers;
   bson_error_t error;
   mongoc_write_concern_t *wc;
   bson_t *schema;
   bson_t *schema_map;
   mongoc_auto_encryption_opts_t *auto_encryption_opts;
   mongoc_client_encryption_opts_t *client_encryption_opts;
   mongoc_client_encryption_encrypt_opts_t *encrypt_opts;
   bool res;
   const bson_value_t *keyid;
   bson_value_t value;
   bson_value_t ciphertext;
   bson_iter_t iter;

   external_uri = test_framework_get_uri ();
   mongoc_uri_set_username (external_uri, "fake-user");
   mongoc_uri_set_password (external_uri, "fake-pwd");
   client_external = mongoc_client_new_from_uri (external_uri);

   /* Using client, drop the collections admin.datakeys and db.coll. */
   client = test_framework_client_new ();
   coll = mongoc_client_get_collection (client, "db", "coll");
   (void) mongoc_collection_drop (coll, NULL);
   mongoc_collection_destroy (coll);
   coll = mongoc_client_get_collection (client, "admin", "datakeys");
   (void) mongoc_collection_drop (coll, NULL);

   /* Insert the document external-key.json into ``admin.datakeys``. */
   wc = mongoc_write_concern_new ();
   mongoc_write_concern_set_wmajority (wc, 1000);
   mongoc_collection_set_write_concern (coll, wc);
   datakey = get_bson_from_json_file ("./src/libmongoc/tests/"
                                      "client_side_encryption_prose/external/"
                                      "external-key.json");
   ASSERT_OR_PRINT (
      mongoc_collection_insert_one (coll, datakey, NULL, NULL, &error), error);
   mongoc_collection_destroy (coll);

   /* Create a MongoClient configured with auto encryption. */
   client_encrypted = test_framework_client_new ();
   mongoc_client_set_error_api (client_encrypted, MONGOC_ERROR_API_VERSION_2);
   auto_encryption_opts = mongoc_auto_encryption_opts_new ();
   _check_bypass (auto_encryption_opts);
   schema = get_bson_from_json_file ("./src/libmongoc/tests/"
                                     "client_side_encryption_prose/external/"
                                     "external-schema.json");
   schema_map = BCON_NEW ("db.coll", BCON_DOCUMENT (schema));
   kms_providers = _make_kms_providers (false /* aws */, true /* local */);
   mongoc_auto_encryption_opts_set_kms_providers (auto_encryption_opts,
                                                  kms_providers);
   mongoc_auto_encryption_opts_set_keyvault_namespace (
      auto_encryption_opts, "admin", "datakeys");
   mongoc_auto_encryption_opts_set_schema_map (auto_encryption_opts,
                                               schema_map);
   if (with_external_key_vault) {
      mongoc_auto_encryption_opts_set_keyvault_client (auto_encryption_opts,
                                                       client_external);
   }
   ASSERT_OR_PRINT (mongoc_client_enable_auto_encryption (
                       client_encrypted, auto_encryption_opts, &error),
                    error);

   /* Create a ClientEncryption object. */
   client_encryption_opts = mongoc_client_encryption_opts_new ();
   mongoc_client_encryption_opts_set_kms_providers (client_encryption_opts,
                                                    kms_providers);
   mongoc_client_encryption_opts_set_keyvault_namespace (
      client_encryption_opts, "admin", "datakeys");
   if (with_external_key_vault) {
      mongoc_client_encryption_opts_set_keyvault_client (client_encryption_opts,
                                                         client_external);
   } else {
      mongoc_client_encryption_opts_set_keyvault_client (client_encryption_opts,
                                                         client);
   }
   client_encryption =
      mongoc_client_encryption_new (client_encryption_opts, &error);
   ASSERT_OR_PRINT (client_encryption, error);

   /* Use client_encrypted to insert the document {"encrypted": "test"} into
    * db.coll. */
   coll = mongoc_client_get_collection (client_encrypted, "db", "coll");
   res = mongoc_collection_insert_one (
      coll, tmp_bson ("{'encrypted': 'test'}"), NULL, NULL, &error);
   if (with_external_key_vault) {
      BSON_ASSERT (!res);
      ASSERT_ERROR_CONTAINS (error,
                             MONGOC_ERROR_CLIENT,
                             MONGOC_ERROR_CLIENT_AUTHENTICATE,
                             "Authentication failed");
   } else {
      ASSERT_OR_PRINT (res, error);
   }

   /* Use client_encryption to explicitly encrypt the string "test" with key ID
    * ``LOCALAAAAAAAAAAAAAAAAA==`` and deterministic algorithm. */
   encrypt_opts = mongoc_client_encryption_encrypt_opts_new ();
   mongoc_client_encryption_encrypt_opts_set_algorithm (
      encrypt_opts, "AEAD_AES_256_CBC_HMAC_SHA_512-Deterministic");
   BSON_ASSERT (bson_iter_init_find (&iter, datakey, "_id"));
   keyid = bson_iter_value (&iter);
   mongoc_client_encryption_encrypt_opts_set_keyid (encrypt_opts, keyid);
   value.value_type = BSON_TYPE_UTF8;
   value.value.v_utf8.str = "test";
   value.value.v_utf8.len = 4;
   res = mongoc_client_encryption_encrypt (
      client_encryption, &value, encrypt_opts, &ciphertext, &error);
   if (with_external_key_vault) {
      BSON_ASSERT (!res);
      ASSERT_ERROR_CONTAINS (error,
                             MONGOC_ERROR_CLIENT,
                             MONGOC_ERROR_CLIENT_AUTHENTICATE,
                             "Authentication failed");
   } else {
      ASSERT_OR_PRINT (res, error);
   }

   bson_destroy (schema);
   bson_destroy (schema_map);
   bson_destroy (datakey);
   bson_value_destroy (&ciphertext);
   mongoc_write_concern_destroy (wc);
   bson_destroy (kms_providers);
   mongoc_collection_destroy (coll);
   mongoc_client_destroy (client);
   mongoc_client_destroy (client_encrypted);
   mongoc_client_destroy (client_external);
   mongoc_uri_destroy (external_uri);
   mongoc_auto_encryption_opts_destroy (auto_encryption_opts);
   mongoc_client_encryption_opts_destroy (client_encryption_opts);
   mongoc_client_encryption_destroy (client_encryption);
   mongoc_client_encryption_encrypt_opts_destroy (encrypt_opts);
}

static void
test_external_key_vault (void *unused)
{
   _test_key_vault (false /* external */);
   _test_key_vault (true /* external */);
}

static void
test_views_are_prohibited (void *unused)
{
   mongoc_client_t *client;
   mongoc_client_t *client_encrypted;
   mongoc_collection_t *coll;
   bool res;
   bson_error_t error;

   mongoc_auto_encryption_opts_t *auto_encryption_opts;
   bson_t *kms_providers;

   client = test_framework_client_new ();
   /* Using client, drop and create a view named db.view with an empty pipeline.
    * E.g. using the command { "create": "view", "viewOn": "coll" }. */
   coll = mongoc_client_get_collection (client, "db", "view");
   (void) mongoc_collection_drop (coll, NULL);
   res = mongoc_client_command_simple (
      client,
      "db",
      tmp_bson ("{'create': 'view', 'viewOn': 'coll'}"),
      NULL,
      NULL,
      &error);
   ASSERT_OR_PRINT (res, error);

   client_encrypted = test_framework_client_new ();
   auto_encryption_opts = mongoc_auto_encryption_opts_new ();
   _check_bypass (auto_encryption_opts);
   kms_providers = _make_kms_providers (false /* aws */, true /* local */);
   mongoc_auto_encryption_opts_set_kms_providers (auto_encryption_opts,
                                                  kms_providers);
   mongoc_auto_encryption_opts_set_keyvault_namespace (
      auto_encryption_opts, "admin", "datakeys");
   ASSERT_OR_PRINT (mongoc_client_enable_auto_encryption (
                       client_encrypted, auto_encryption_opts, &error),
                    error);

   mongoc_collection_destroy (coll);
   coll = mongoc_client_get_collection (client_encrypted, "db", "view");
   res = mongoc_collection_insert_one (
      coll, tmp_bson ("{'x': 1}"), NULL, NULL, &error);
   BSON_ASSERT (!res);
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_CLIENT_SIDE_ENCRYPTION,
                          1,
                          "cannot auto encrypt a view");

   bson_destroy (kms_providers);
   mongoc_collection_destroy (coll);
   mongoc_auto_encryption_opts_destroy (auto_encryption_opts);
   mongoc_client_destroy (client_encrypted);
   mongoc_client_destroy (client);
}

void
test_client_side_encryption_install (TestSuite *suite)
{
   char resolved[PATH_MAX];

   ASSERT (realpath (JSON_DIR "/client_side_encryption", resolved));
   install_json_test_suite_with_check (
      suite,
      resolved,
      test_client_side_encryption_cb,
      test_framework_skip_if_no_client_side_encryption);
   /* Prose tests from the spec. */
   TestSuite_AddFull (suite,
                      "/client_side_encryption/datakey_and_double_encryption",
                      test_datakey_and_double_encryption,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_client_side_encryption,
                      test_framework_skip_if_max_wire_version_less_than_8);
   TestSuite_AddFull (suite,
                      "/client_side_encryption/external_key_vault",
                      test_external_key_vault,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_client_side_encryption,
                      test_framework_skip_if_max_wire_version_less_than_8);
   TestSuite_AddFull (
      suite,
      "/client_side_encryption/bson_size_limits_and_batch_splitting",
      test_bson_size_limits_and_batch_splitting,
      NULL,
      NULL,
      test_framework_skip_if_no_client_side_encryption,
      test_framework_skip_if_max_wire_version_less_than_8);
   TestSuite_AddFull (suite,
                      "/client_side_encryption/views_are_prohibited",
                      test_views_are_prohibited,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_client_side_encryption,
                      test_framework_skip_if_max_wire_version_less_than_8);
   /* Other, C driver specific, tests. */
   TestSuite_AddFull (suite,
                      "/client_side_encryption/single_and_pool_mismatches",
                      test_invalid_single_and_pool_mismatches,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_client_side_encryption,
                      test_framework_skip_if_max_wire_version_less_than_8);
   TestSuite_AddFull (suite,
                      "/client_side_encryption/multi_threaded",
                      test_multi_threaded,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_client_side_encryption,
                      test_framework_skip_if_max_wire_version_less_than_8);
}