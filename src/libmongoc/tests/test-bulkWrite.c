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

#include <mongoc/mongoc.h>
#include <test-libmongoc.h> // test_framework_skip_if_no_client_side_encryption
#include <TestSuite.h>
#include <test-conveniences.h> // tmp_bson
#include <common-b64-private.h>

#define LOCAL_KEK_BASE64                                                       \
   "Mng0NCt4ZHVUYUJCa1kxNkVyNUR1QURhZ2h2UzR2d2RrZzh0cFBwM3R6NmdWMDFBMUN3YkQ5a" \
   "XRRMkhGRGdQV09wOGVNYUMxT2k3NjZKelhaQmRCZGJkTXVyZG9uSjFk"

static bson_t *
make_kms_providers (void)
{
   return tmp_bson (
      BSON_STR ({
         "local" : {"key" : {"$binary" : {"base64" : "%s", "subType" : "00"}}}
      }),
      LOCAL_KEK_BASE64);
}

static bson_t *
make_encrypted_fields (const bson_value_t *keyid)
{
   ASSERT (keyid->value_type == BSON_TYPE_BINARY);
   ASSERT (keyid->value.v_binary.subtype == BSON_SUBTYPE_UUID);

   // Base64 encode `keyid`.
   char keyid_as_base64[64];
   ASSERT (-1 != mcommon_b64_ntop (keyid->value.v_binary.data,
                                   keyid->value.v_binary.data_len,
                                   keyid_as_base64,
                                   sizeof (keyid_as_base64)));

   return tmp_bson (
      BSON_STR ({
         "fields" : [ {
            "keyId" : {"$binary" : {"base64" : "%s", "subType" : "04"}},
            "path" : "encryptedIndexed",
            "bsonType" : "string",
            "queries" :
               {"queryType" : "equality", "contention" : {"$numberLong" : "0"}}
         } ]
      }),
      keyid_as_base64);
}

typedef struct {
   mongoc_client_t *unencryptedSetupClient;
   mongoc_client_t *encryptedSetupClient;
   mongoc_client_encryption_t *ce;
   bson_value_t keyid;
} bulkwrite_test;

static void
bulkwrite_test_recreateCollection (bulkwrite_test *bt);

static bulkwrite_test *
bulkwrite_test_new (void)
{
   bson_error_t error;
   bulkwrite_test *bt = bson_malloc0 (sizeof (bulkwrite_test));

   // Do test setup.
   bt->unencryptedSetupClient = test_framework_new_default_client ();
   {
      bson_t *kms_providers = make_kms_providers ();

      // Drop previous key vault collection (if exists).
      {
         mongoc_collection_t *key_vault_coll = mongoc_client_get_collection (
            bt->unencryptedSetupClient, "keyvault", "datakeys");
         mongoc_collection_drop (key_vault_coll, NULL); // Ignore error.
         mongoc_collection_destroy (key_vault_coll);
      }

      // Create ClientEncryption object.
      {
         mongoc_client_encryption_opts_t *ceo =
            mongoc_client_encryption_opts_new ();
         mongoc_client_encryption_opts_set_keyvault_client (
            ceo, bt->unencryptedSetupClient);
         mongoc_client_encryption_opts_set_kms_providers (ceo, kms_providers);
         mongoc_client_encryption_opts_set_keyvault_namespace (
            ceo, "keyvault", "datakeys");
         bt->ce = mongoc_client_encryption_new (ceo, &error);
         ASSERT_OR_PRINT (bt->ce, error);
         mongoc_client_encryption_opts_destroy (ceo);
      }

      // Create Data Encryption Key (DEK).
      {
         mongoc_client_encryption_datakey_opts_t *dko =
            mongoc_client_encryption_datakey_opts_new ();
         bool ok = mongoc_client_encryption_create_datakey (
            bt->ce, "local", dko, &bt->keyid, &error);
         ASSERT_OR_PRINT (ok, error);
         mongoc_client_encryption_datakey_opts_destroy (dko);
      }


      // Create client with QE enabled.
      bt->encryptedSetupClient = test_framework_new_default_client ();
      {
         mongoc_auto_encryption_opts_t *ao = mongoc_auto_encryption_opts_new ();
         mongoc_auto_encryption_opts_set_kms_providers (ao, kms_providers);
         mongoc_auto_encryption_opts_set_keyvault_namespace (
            ao, "keyvault", "datakeys");

         bool ok = mongoc_client_enable_auto_encryption (
            bt->encryptedSetupClient, ao, &error);
         ASSERT_OR_PRINT (ok, error);
         mongoc_auto_encryption_opts_destroy (ao);
      }
   }

   // Verify setup.
   {
      // Clear data from prior test runs.
      bulkwrite_test_recreateCollection (bt);

      // Insert document with encrypted client.
      {
         mongoc_collection_t *coll = mongoc_client_get_collection (
            bt->encryptedSetupClient, "db", "coll");
         bool ok = mongoc_collection_insert_one (
            coll,
            tmp_bson ("{'encryptedIndexed': 'foo' }"),
            NULL,
            NULL,
            &error);
         ASSERT_OR_PRINT (ok, error);
         mongoc_collection_destroy (coll);
      }


      // Find document with unencrypted client to verify it is encrypted.
      {
         mongoc_collection_t *coll = mongoc_client_get_collection (
            bt->unencryptedSetupClient, "db", "coll");
         const bson_t *doc;
         mongoc_cursor_t *cursor = mongoc_collection_find_with_opts (
            coll, tmp_bson ("{}"), NULL /* opts */, NULL /* read_prefs */);
         bool has_doc = mongoc_cursor_next (cursor, &doc);
         ASSERT (has_doc);
         ASSERT_OR_PRINT (!mongoc_cursor_error (cursor, &error), error);
         bson_iter_t iter;
         ASSERT (bson_iter_init_find (&iter, doc, "encryptedIndexed"));
         ASSERT_MATCH (doc, "{'encryptedIndexed': { '$$type': 'binData' }}");
         mongoc_cursor_destroy (cursor);
         mongoc_collection_destroy (coll);
      }

      // Recreate encrypted collection.
      bulkwrite_test_recreateCollection (bt);
   }

   return bt;
}

static void
bulkwrite_test_recreateCollection (bulkwrite_test *bt)
{
   bson_error_t error;

   // Drop previous QE collection (if exists).
   {
      mongoc_collection_t *coll = mongoc_client_get_collection (
         bt->unencryptedSetupClient, "db", "coll");
      bson_t *ef = make_encrypted_fields (&bt->keyid);
      bson_t *dopts = BCON_NEW ("encryptedFields", BCON_DOCUMENT (ef));
      bool ok = mongoc_collection_drop_with_opts (coll, dopts, &error);
      ASSERT_OR_PRINT (ok, error);
      bson_destroy (dopts);
      mongoc_collection_destroy (coll);
   }

   // Drop unencrypted `coll2`.
   {
      mongoc_collection_t *coll = mongoc_client_get_collection (
         bt->unencryptedSetupClient, "db", "coll2");
      bool ok = mongoc_collection_drop_with_opts (coll, NULL, &error);
      ASSERT_OR_PRINT (ok, error);
      mongoc_collection_destroy (coll);
   }

   // Create Queryable Encryption (QE) collection.
   {
      mongoc_database_t *db =
         mongoc_client_get_database (bt->unencryptedSetupClient, "db");
      bson_t *ef = make_encrypted_fields (&bt->keyid);
      bson_t *ccopts = BCON_NEW ("encryptedFields", BCON_DOCUMENT (ef));
      mongoc_collection_t *coll =
         mongoc_database_create_collection (db, "coll", ccopts, &error);
      ASSERT_OR_PRINT (coll, error);
      mongoc_collection_destroy (coll);
      bson_destroy (ccopts);
      mongoc_database_destroy (db);
   }
}

#define bulkwrite_test_assertOneEncrypted(_bt)                             \
   if (1) {                                                                \
      mongoc_collection_t *coll = mongoc_client_get_collection (           \
         _bt->unencryptedSetupClient, "db", "coll");                       \
      const bson_t *doc;                                                   \
      mongoc_cursor_t *cursor = mongoc_collection_find_with_opts (         \
         coll, tmp_bson ("{}"), NULL /* opts */, NULL /* read_prefs */);   \
      bool has_doc = mongoc_cursor_next (cursor, &doc);                    \
      ASSERT (has_doc);                                                    \
      ASSERT_OR_PRINT (!mongoc_cursor_error (cursor, &error), error);      \
      bson_iter_t iter;                                                    \
      ASSERT (bson_iter_init_find (&iter, doc, "encryptedIndexed"));       \
      ASSERT_MATCH (doc, "{'encryptedIndexed': { '$$type': 'binData' }}"); \
      /* Check exactly one document. */                                    \
      ASSERT (!mongoc_cursor_next (cursor, &doc));                         \
      ASSERT_OR_PRINT (!mongoc_cursor_error (cursor, &error), error);      \
      mongoc_cursor_destroy (cursor);                                      \
      mongoc_collection_destroy (coll);                                    \
   } else                                                                  \
      (void) 0

#define bulkwrite_test_assertOneDecryptsToFoo(_bt)                       \
   if (1) {                                                              \
      mongoc_collection_t *coll = mongoc_client_get_collection (         \
         _bt->encryptedSetupClient, "db", "coll");                       \
      const bson_t *doc;                                                 \
      mongoc_cursor_t *cursor = mongoc_collection_find_with_opts (       \
         coll, tmp_bson ("{}"), NULL /* opts */, NULL /* read_prefs */); \
      bool has_doc = mongoc_cursor_next (cursor, &doc);                  \
      ASSERT (has_doc);                                                  \
      ASSERT_OR_PRINT (!mongoc_cursor_error (cursor, &error), error);    \
      bson_iter_t iter;                                                  \
      ASSERT (bson_iter_init_find (&iter, doc, "encryptedIndexed"));     \
      ASSERT_MATCH (doc, "{'encryptedIndexed': 'foo' }");                \
      /* Check exactly one document. */                                  \
      ASSERT (!mongoc_cursor_next (cursor, &doc));                       \
      ASSERT_OR_PRINT (!mongoc_cursor_error (cursor, &error), error);    \
      mongoc_cursor_destroy (cursor);                                    \
      mongoc_collection_destroy (coll);                                  \
   } else                                                                \
      (void) 0

static void
bulkwrite_test_destroy (bulkwrite_test *bt)
{
   bson_value_destroy (&bt->keyid);
   mongoc_client_encryption_destroy (bt->ce);
   mongoc_client_destroy (bt->encryptedSetupClient);
   mongoc_client_destroy (bt->unencryptedSetupClient);
   bson_free (bt);
}

static void
test_bulkWrite_qe_remoteSchema (void *unused)
{
   BSON_UNUSED (unused);

   bson_error_t error;
   bulkwrite_test *bt = bulkwrite_test_new ();

   // Create client with QE enabled.
   mongoc_client_t *encryptedClient = test_framework_new_default_client ();
   {
      bson_t *kms_providers = make_kms_providers ();
      mongoc_auto_encryption_opts_t *ao = mongoc_auto_encryption_opts_new ();
      mongoc_auto_encryption_opts_set_kms_providers (ao, kms_providers);
      mongoc_auto_encryption_opts_set_keyvault_namespace (
         ao, "keyvault", "datakeys");

      bool ok =
         mongoc_client_enable_auto_encryption (encryptedClient, ao, &error);
      ASSERT_OR_PRINT (ok, error);
      mongoc_auto_encryption_opts_destroy (ao);
   }

   // Run a `bulkWrite` command.
   {
      bson_t *cmd = tmp_bson (BSON_STR ({
         "bulkWrite" : 1,
         "ops" : [ {"insert" : 0, "document" : {"encryptedIndexed" : "foo"}} ],
         "nsInfo" : [ {"ns" : "db.coll"} ]
      }));
      // Use `runCommand` since driver does not yet have a new `bulkWrite`
      // helper.
      bool ok = mongoc_client_command_with_opts (encryptedClient,
                                                 "admin",
                                                 cmd,
                                                 NULL /* read prefs */,
                                                 NULL /* opts */,
                                                 NULL /* reply */,
                                                 &error);
      ASSERT_OR_PRINT (ok, error);
   }

   bulkwrite_test_assertOneEncrypted (bt);
   bulkwrite_test_assertOneDecryptsToFoo (bt);

   mongoc_client_destroy (encryptedClient);
   bulkwrite_test_destroy (bt);
}

static void
test_bulkWrite_qe_remoteSchema_cached (void *unused)
{
   BSON_UNUSED (unused);

   bson_error_t error;
   bulkwrite_test *bt = bulkwrite_test_new ();

   // Create client with QE enabled.
   mongoc_client_t *encryptedClient = test_framework_new_default_client ();
   {
      bson_t *kms_providers = make_kms_providers ();
      mongoc_auto_encryption_opts_t *ao = mongoc_auto_encryption_opts_new ();
      mongoc_auto_encryption_opts_set_kms_providers (ao, kms_providers);
      mongoc_auto_encryption_opts_set_keyvault_namespace (
         ao, "keyvault", "datakeys");

      bool ok =
         mongoc_client_enable_auto_encryption (encryptedClient, ao, &error);
      ASSERT_OR_PRINT (ok, error);
      mongoc_auto_encryption_opts_destroy (ao);
   }

   // Run a `bulkWrite` command.
   {
      bson_t *cmd = tmp_bson (BSON_STR ({
         "bulkWrite" : 1,
         "ops" : [ {"insert" : 0, "document" : {"encryptedIndexed" : "foo"}} ],
         "nsInfo" : [ {"ns" : "db.coll"} ]
      }));
      // Use `runCommand` since driver does not yet have a new `bulkWrite`
      // helper.
      bool ok = mongoc_client_command_with_opts (encryptedClient,
                                                 "admin",
                                                 cmd,
                                                 NULL /* read prefs */,
                                                 NULL /* opts */,
                                                 NULL /* reply */,
                                                 &error);
      ASSERT_OR_PRINT (ok, error);
   }

   bulkwrite_test_assertOneEncrypted (bt);
   bulkwrite_test_assertOneDecryptsToFoo (bt);

   // Do it again (remote schema is expected to be cached).
   {
      bulkwrite_test_recreateCollection (bt);
      // Run a `bulkWrite` command.
      {
         bson_t *cmd = tmp_bson (BSON_STR ({
            "bulkWrite" : 1,
            "ops" :
               [ {"insert" : 0, "document" : {"encryptedIndexed" : "foo"}} ],
            "nsInfo" : [ {"ns" : "db.coll"} ]
         }));
         // Use `runCommand` since driver does not yet have a new `bulkWrite`
         // helper.
         bool ok = mongoc_client_command_with_opts (encryptedClient,
                                                    "admin",
                                                    cmd,
                                                    NULL /* read prefs */,
                                                    NULL /* opts */,
                                                    NULL /* reply */,
                                                    &error);
         ASSERT_OR_PRINT (ok, error);
      }

      bulkwrite_test_assertOneEncrypted (bt);
      bulkwrite_test_assertOneDecryptsToFoo (bt);
   }

   mongoc_client_destroy (encryptedClient);
   bulkwrite_test_destroy (bt);
}

static void
test_bulkWrite_qe_localSchema (void *unused)
{
   BSON_UNUSED (unused);

   bson_error_t error;
   bulkwrite_test *bt = bulkwrite_test_new ();

   // Create client with QE enabled and encrypted field map.
   mongoc_client_t *encryptedClient = test_framework_new_default_client ();
   {
      bson_t *kms_providers = make_kms_providers ();
      mongoc_auto_encryption_opts_t *ao = mongoc_auto_encryption_opts_new ();
      mongoc_auto_encryption_opts_set_kms_providers (ao, kms_providers);
      mongoc_auto_encryption_opts_set_keyvault_namespace (
         ao, "keyvault", "datakeys");

      bson_t *efm = BCON_NEW (
         "db.coll", BCON_DOCUMENT (make_encrypted_fields (&bt->keyid)));
      mongoc_auto_encryption_opts_set_encrypted_fields_map (ao, efm);
      bson_destroy (efm);

      bool ok =
         mongoc_client_enable_auto_encryption (encryptedClient, ao, &error);
      ASSERT_OR_PRINT (ok, error);
      mongoc_auto_encryption_opts_destroy (ao);
   }

   // Run a `bulkWrite` command.
   {
      bson_t *cmd = tmp_bson (BSON_STR ({
         "bulkWrite" : 1,
         "ops" : [ {"insert" : 0, "document" : {"encryptedIndexed" : "foo"}} ],
         "nsInfo" : [ {"ns" : "db.coll"} ]
      }));
      // Use `runCommand` since driver does not yet have a new `bulkWrite`
      // helper.
      bool ok = mongoc_client_command_with_opts (encryptedClient,
                                                 "admin",
                                                 cmd,
                                                 NULL /* read prefs */,
                                                 NULL /* opts */,
                                                 NULL /* reply */,
                                                 &error);
      ASSERT_OR_PRINT (ok, error);
   }

   bulkwrite_test_assertOneEncrypted (bt);
   bulkwrite_test_assertOneDecryptsToFoo (bt);

   mongoc_client_destroy (encryptedClient);
   bulkwrite_test_destroy (bt);
}

static void
test_bulkWrite_qe_bypassQueryAnalysis (void *unused)
{
   BSON_UNUSED (unused);
   bson_error_t error;
   bulkwrite_test *bt = bulkwrite_test_new ();

   // Explicitly encrypt values to insert.
   bson_t to_insert = BSON_INITIALIZER;
   {
      mongoc_client_encryption_encrypt_opts_t *eo =
         mongoc_client_encryption_encrypt_opts_new ();
      mongoc_client_encryption_encrypt_opts_set_contention_factor (eo, 0);
      mongoc_client_encryption_encrypt_opts_set_algorithm (eo, "Indexed");
      mongoc_client_encryption_encrypt_opts_set_keyid (eo, &bt->keyid);

      {
         bson_value_t plaintext = {
            .value_type = BSON_TYPE_UTF8,
            .value = {.v_utf8 = {.len = 3, .str = "foo"}}};
         bson_value_t ciphertext;
         bool ok = mongoc_client_encryption_encrypt (
            bt->ce, &plaintext, eo, &ciphertext, &error);
         ASSERT_OR_PRINT (ok, error);
         BSON_APPEND_VALUE (&to_insert, "encryptedIndexed", &ciphertext);
         bson_value_destroy (&ciphertext);
      }
      mongoc_client_encryption_encrypt_opts_destroy (eo);
   }

   // Explicitly encrypt value to find.
   bson_t to_find = BSON_INITIALIZER;
   {
      mongoc_client_encryption_encrypt_opts_t *eo =
         mongoc_client_encryption_encrypt_opts_new ();
      mongoc_client_encryption_encrypt_opts_set_contention_factor (eo, 0);
      mongoc_client_encryption_encrypt_opts_set_algorithm (eo, "Indexed");
      mongoc_client_encryption_encrypt_opts_set_query_type (eo, "equality");
      mongoc_client_encryption_encrypt_opts_set_keyid (eo, &bt->keyid);

      {
         bson_value_t plaintext = {
            .value_type = BSON_TYPE_UTF8,
            .value = {.v_utf8 = {.len = 3, .str = "foo"}}};
         bson_value_t ciphertext;
         bool ok = mongoc_client_encryption_encrypt (
            bt->ce, &plaintext, eo, &ciphertext, &error);
         ASSERT_OR_PRINT (ok, error);
         BSON_APPEND_VALUE (&to_find, "encryptedIndexed", &ciphertext);
         bson_value_destroy (&ciphertext);
      }

      mongoc_client_encryption_encrypt_opts_destroy (eo);
   }

   // Create client with QE enabled but bypassQueryAnalysis.
   mongoc_client_t *encryptedClient = test_framework_new_default_client ();
   {
      bson_t *kms_providers = make_kms_providers ();
      mongoc_auto_encryption_opts_t *ao = mongoc_auto_encryption_opts_new ();
      mongoc_auto_encryption_opts_set_kms_providers (ao, kms_providers);
      mongoc_auto_encryption_opts_set_keyvault_namespace (
         ao, "keyvault", "datakeys");
      mongoc_auto_encryption_opts_set_bypass_query_analysis (ao, true);
      bool ok =
         mongoc_client_enable_auto_encryption (encryptedClient, ao, &error);
      ASSERT_OR_PRINT (ok, error);
      mongoc_auto_encryption_opts_destroy (ao);
   }

   // Run a `bulkWrite` command.
   {
      // clang-format off
      bson_t *cmd = BCON_NEW (
         "bulkWrite", BCON_INT32(1),
         "ops", "[", "{", "insert", BCON_INT32(0), "document", BCON_DOCUMENT (&to_insert), "}", "]",
         "nsInfo", "[", "{", "ns", "db.coll", "}", "]"
      );
      // clang-format on
      // Use `runCommand` since driver does not yet have a new `bulkWrite`
      // helper.
      bool ok = mongoc_client_command_with_opts (encryptedClient,
                                                 "admin",
                                                 cmd,
                                                 NULL /* read prefs */,
                                                 NULL /* opts */,
                                                 NULL /* reply */,
                                                 &error);
      ASSERT_OR_PRINT (ok, error);
      bson_destroy (cmd);
   }

   bulkwrite_test_assertOneEncrypted (bt);
   bulkwrite_test_assertOneDecryptsToFoo (bt);


   bson_destroy (&to_find);
   bson_destroy (&to_insert);
   mongoc_client_destroy (encryptedClient);
   bulkwrite_test_destroy (bt);
}

static void
test_bulkWrite_qe_differentCollection (void *unused)
{
   BSON_UNUSED (unused);

   bson_error_t error;
   bulkwrite_test *bt = bulkwrite_test_new ();

   // Create client with QE enabled.
   mongoc_client_t *encryptedClient = test_framework_new_default_client ();
   {
      bson_t *kms_providers = make_kms_providers ();
      mongoc_auto_encryption_opts_t *ao = mongoc_auto_encryption_opts_new ();
      mongoc_auto_encryption_opts_set_kms_providers (ao, kms_providers);
      mongoc_auto_encryption_opts_set_keyvault_namespace (
         ao, "keyvault", "datakeys");

      bool ok =
         mongoc_client_enable_auto_encryption (encryptedClient, ao, &error);
      ASSERT_OR_PRINT (ok, error);
      mongoc_auto_encryption_opts_destroy (ao);
   }

   // Run a `bulkWrite` command on a different collection that is not encrypted.
   {
      bson_t *cmd = tmp_bson (BSON_STR ({
         "bulkWrite" : 1,
         "ops" : [ {"insert" : 0, "document" : {"foo" : "bar"}} ],
         "nsInfo" : [ {"ns" : "db.coll2"} ]
      }));
      // Use `runCommand` since driver does not yet have a new `bulkWrite`
      // helper.
      bool ok = mongoc_client_command_with_opts (encryptedClient,
                                                 "admin",
                                                 cmd,
                                                 NULL /* read prefs */,
                                                 NULL /* opts */,
                                                 NULL /* reply */,
                                                 &error);
      ASSERT_OR_PRINT (ok, error);
   }

   // Expect db.coll2 to have an encrypted document.
   {
      mongoc_collection_t *coll = mongoc_client_get_collection (
         bt->unencryptedSetupClient, "db", "coll2");
      const bson_t *doc;
      mongoc_cursor_t *cursor = mongoc_collection_find_with_opts (
         coll, tmp_bson ("{}"), NULL /* opts */, NULL /* read_prefs */);
      bool has_doc = mongoc_cursor_next (cursor, &doc);
      ASSERT (has_doc);
      ASSERT_OR_PRINT (!mongoc_cursor_error (cursor, &error), error);
      bson_iter_t iter;
      ASSERT (bson_iter_init_find (&iter, doc, "foo"));
      ASSERT_MATCH (doc, "{'foo': 'bar' }");
      /* Check exactly one document. */
      ASSERT (!mongoc_cursor_next (cursor, &doc));
      ASSERT_OR_PRINT (!mongoc_cursor_error (cursor, &error), error);
      mongoc_cursor_destroy (cursor);
      mongoc_collection_destroy (coll);
   }

   mongoc_client_destroy (encryptedClient);
   bulkwrite_test_destroy (bt);
}

static void
test_bulkWrite_qe_delete (void *unused)
{
   BSON_UNUSED (unused);

   bson_error_t error;
   bulkwrite_test *bt = bulkwrite_test_new ();

   // Create client with QE enabled.
   mongoc_client_t *encryptedClient = test_framework_new_default_client ();
   {
      bson_t *kms_providers = make_kms_providers ();
      mongoc_auto_encryption_opts_t *ao = mongoc_auto_encryption_opts_new ();
      mongoc_auto_encryption_opts_set_kms_providers (ao, kms_providers);
      mongoc_auto_encryption_opts_set_keyvault_namespace (
         ao, "keyvault", "datakeys");

      bool ok =
         mongoc_client_enable_auto_encryption (encryptedClient, ao, &error);
      ASSERT_OR_PRINT (ok, error);
      mongoc_auto_encryption_opts_destroy (ao);
   }

   // Run a `bulkWrite` command to insert.
   {
      bson_t *cmd = tmp_bson (BSON_STR ({
         "bulkWrite" : 1,
         "ops" : [ {"insert" : 0, "document" : {"encryptedIndexed" : "bar"}} ],
         "nsInfo" : [ {"ns" : "db.coll"} ]
      }));
      // Use `runCommand` since driver does not yet have a new `bulkWrite`
      // helper.
      bool ok = mongoc_client_command_with_opts (encryptedClient,
                                                 "admin",
                                                 cmd,
                                                 NULL /* read prefs */,
                                                 NULL /* opts */,
                                                 NULL /* reply */,
                                                 &error);
      ASSERT_OR_PRINT (ok, error);
   }

   // Run a `bulkWrite` command to delete.
   {
      bson_t *cmd = tmp_bson (BSON_STR ({
         "bulkWrite" : 1,
         "ops" : [ {
            "delete" : 0,
            "filter" : {"encryptedIndexed" : "bar"},
            "multi" : true
         } ],
         "nsInfo" : [ {"ns" : "db.coll"} ]
      }));
      // Use `runCommand` since driver does not yet have a new `bulkWrite`
      // helper.
      bool ok = mongoc_client_command_with_opts (encryptedClient,
                                                 "admin",
                                                 cmd,
                                                 NULL /* read prefs */,
                                                 NULL /* opts */,
                                                 NULL /* reply */,
                                                 &error);
      ASSERT_OR_PRINT (ok, error);
   }

   mongoc_client_destroy (encryptedClient);
   bulkwrite_test_destroy (bt);
}

static void
test_bulkWrite_csfle (void *unused)
{
   BSON_UNUSED (unused);

   bson_error_t error;
   bulkwrite_test *bt = bulkwrite_test_new ();

   // Create client with QE enabled.
   mongoc_client_t *encryptedClient = test_framework_new_default_client ();
   {
      bson_t *kms_providers = make_kms_providers ();
      mongoc_auto_encryption_opts_t *ao = mongoc_auto_encryption_opts_new ();
      mongoc_auto_encryption_opts_set_kms_providers (ao, kms_providers);
      mongoc_auto_encryption_opts_set_keyvault_namespace (
         ao, "keyvault", "datakeys");

      // Set a local JSON schema.
      mongoc_auto_encryption_opts_set_schema_map (
         ao, tmp_bson (BSON_STR ({"db.coll" : {}})));

      bool ok =
         mongoc_client_enable_auto_encryption (encryptedClient, ao, &error);
      ASSERT_OR_PRINT (ok, error);
      mongoc_auto_encryption_opts_destroy (ao);
   }

   // Run a `bulkWrite` command to insert.
   {
      bson_t *cmd = tmp_bson (BSON_STR ({
         "bulkWrite" : 1,
         "ops" : [ {"insert" : 0, "document" : {"encryptedIndexed" : "bar"}} ],
         "nsInfo" : [ {"ns" : "db.coll"} ]
      }));
      // Use `runCommand` since driver does not yet have a new `bulkWrite`
      // helper.
      bool ok = mongoc_client_command_with_opts (encryptedClient,
                                                 "admin",
                                                 cmd,
                                                 NULL /* read prefs */,
                                                 NULL /* opts */,
                                                 NULL /* reply */,
                                                 &error);
      ASSERT (!ok);
      ASSERT_ERROR_CONTAINS (
         error,
         MONGOC_ERROR_QUERY,
         2,
         "The bulkWrite command only supports Queryable Encryption");
   }

   mongoc_client_destroy (encryptedClient);
   bulkwrite_test_destroy (bt);
}

void
test_bulkWrite_install (TestSuite *suite)
{
   TestSuite_AddFull (
      suite,
      "/bulkWrite/csfle",
      test_bulkWrite_csfle,
      NULL,
      NULL,
      test_framework_skip_if_no_client_side_encryption,
      test_framework_skip_if_max_wire_version_less_than_25 /* require server 8.0
                                                            */
   );
   TestSuite_AddFull (
      suite,
      "/bulkWrite/qe/remoteSchema",
      test_bulkWrite_qe_remoteSchema,
      NULL,
      NULL,
      test_framework_skip_if_no_client_side_encryption,
      test_framework_skip_if_max_wire_version_less_than_25 /* require
                                                             server 8.0 */
   );
   TestSuite_AddFull (
      suite,
      "/bulkWrite/qe/remoteSchema/cached",
      test_bulkWrite_qe_remoteSchema_cached,
      NULL,
      NULL,
      test_framework_skip_if_no_client_side_encryption,
      test_framework_skip_if_max_wire_version_less_than_25 /* require server 8.0
                                                            */
   );
   TestSuite_AddFull (
      suite,
      "/bulkWrite/qe/localSchema",
      test_bulkWrite_qe_localSchema,
      NULL,
      NULL,
      test_framework_skip_if_no_client_side_encryption,
      test_framework_skip_if_max_wire_version_less_than_25 /* require server 8.0
                                                            */
   );
   TestSuite_AddFull (
      suite,
      "/bulkWrite/qe/bypassQueryAnalysis",
      test_bulkWrite_qe_bypassQueryAnalysis,
      NULL,
      NULL,
      test_framework_skip_if_no_client_side_encryption,
      test_framework_skip_if_max_wire_version_less_than_25 /* require server 8.0
                                                            */
   );
   TestSuite_AddFull (
      suite,
      "/bulkWrite/qe/differentCollection",
      test_bulkWrite_qe_differentCollection,
      NULL,
      NULL,
      test_framework_skip_if_no_client_side_encryption,
      test_framework_skip_if_max_wire_version_less_than_25 /* require server 8.0
                                                            */
   );
   TestSuite_AddFull (
      suite,
      "/bulkWrite/qe/delete",
      test_bulkWrite_qe_delete,
      NULL,
      NULL,
      test_framework_skip_if_no_client_side_encryption,
      test_framework_skip_if_max_wire_version_less_than_25 /* require server 8.0
                                                            */
   );
}
