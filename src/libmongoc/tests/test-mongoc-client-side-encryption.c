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
   mongoc_collection_t *key_vault_coll;
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
      key_vault_coll =
         mongoc_client_get_collection (client, "admin", "datakeys");

      /* Drop and recreate, inserting data. */
      ret = mongoc_collection_drop (key_vault_coll, &error);
      if (!ret) {
         /* Ignore "namespace does not exist" error. */
         ASSERT_OR_PRINT (error.code == 26, error);
      }

      bson_iter_recurse (&iter, &iter);
      while (bson_iter_next (&iter)) {
         bson_t doc;

         bson_iter_bson (&iter, &doc);
         ret = mongoc_collection_insert_one (
            key_vault_coll, &doc, &insert_opts, NULL /* reply */, &error);
         ASSERT_OR_PRINT (ret, error);
      }
      mongoc_collection_destroy (key_vault_coll);
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

   mongoc_client_destroy (client);
   client = mongoc_client_new_from_uri (uri);
   mongoc_client_set_error_api (client, MONGOC_ERROR_API_VERSION_2);

   kms_providers = BCON_NEW (
      "local", "{", "key", BCON_BIN (0, (uint8_t *) LOCAL_MASTERKEY, 96), "}");
   opts = mongoc_auto_encryption_opts_new ();
   mongoc_auto_encryption_opts_set_key_vault_namespace (
      opts, "admin", "datakeys");
   mongoc_auto_encryption_opts_set_kms_providers (opts, kms_providers);

   ASSERT_OR_PRINT (mongoc_client_enable_auto_encryption (client, opts, &error),
                    error);

   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_started_cb (callbacks, _command_started);
   mongoc_client_set_apm_callbacks (client, callbacks, &ctx);

   mongoc_collection_destroy (coll);
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
   TestSuite_AddFull (
      suite,
      "/client_side_encryption/bson_size_limits_and_batch_splitting",
      test_bson_size_limits_and_batch_splitting,
      NULL,
      NULL,
      test_framework_skip_if_no_client_side_encryption,
      test_framework_skip_if_max_wire_version_less_than_8);
}