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
         "escCollection" : "enxcol_.coll.esc",
         "ecocCollection" : "enxcol_.coll.ecoc",
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

static void
test_bulkWrite_qe (void *unused)
{
   BSON_UNUSED (unused);
   bson_error_t error;
   mongoc_client_t *unencryptedClient = test_framework_new_default_client ();

   // Do test setup.
   mongoc_client_t *encryptedClient;
   {
      bson_t *kms_providers = make_kms_providers ();

      // Drop previous key vault collection (if exists).
      {
         mongoc_collection_t *key_vault_coll = mongoc_client_get_collection (
            unencryptedClient, "keyvault", "datakeys");
         mongoc_collection_drop (key_vault_coll, NULL); // Ignore error.
         mongoc_collection_destroy (key_vault_coll);
      }

      // Create ClientEncryption object.
      mongoc_client_encryption_t *ce;
      {
         mongoc_client_encryption_opts_t *ceo =
            mongoc_client_encryption_opts_new ();
         mongoc_client_encryption_opts_set_keyvault_client (ceo,
                                                            unencryptedClient);
         mongoc_client_encryption_opts_set_kms_providers (ceo, kms_providers);
         mongoc_client_encryption_opts_set_keyvault_namespace (
            ceo, "keyvault", "datakeys");
         ce = mongoc_client_encryption_new (ceo, &error);
         ASSERT_OR_PRINT (ce, error);
         mongoc_client_encryption_opts_destroy (ceo);
      }

      // Create Data Encryption Key (DEK).
      bson_value_t keyid;
      {
         mongoc_client_encryption_datakey_opts_t *dko =
            mongoc_client_encryption_datakey_opts_new ();
         bool ok = mongoc_client_encryption_create_datakey (
            ce, "local", dko, &keyid, &error);
         ASSERT_OR_PRINT (ok, error);
         mongoc_client_encryption_datakey_opts_destroy (dko);
      }


      // Drop previous QE collection (if exists).
      {
         mongoc_collection_t *coll =
            mongoc_client_get_collection (unencryptedClient, "db", "coll");
         bson_t *ef = make_encrypted_fields (&keyid);
         bson_t *dopts = BCON_NEW ("encryptedFields", BCON_DOCUMENT (ef));
         bool ok = mongoc_collection_drop_with_opts (coll, dopts, &error);
         ASSERT_OR_PRINT (ok, error);
         bson_destroy (dopts);
         mongoc_collection_destroy (coll);
      }

      // Create Queryable Encryption (QE) collection.
      {
         mongoc_database_t *db =
            mongoc_client_get_database (unencryptedClient, "db");
         bson_t *ef = make_encrypted_fields (&keyid);
         bson_t *ccopts = BCON_NEW ("encryptedFields", BCON_DOCUMENT (ef));
         mongoc_collection_t *coll =
            mongoc_database_create_collection (db, "coll", ccopts, &error);
         ASSERT_OR_PRINT (coll, error);
         mongoc_collection_destroy (coll);
         bson_destroy (ccopts);
         mongoc_database_destroy (db);
      }

      // Create client with QE enabled.
      {
         mongoc_auto_encryption_opts_t *ao = mongoc_auto_encryption_opts_new ();
         mongoc_auto_encryption_opts_set_kms_providers (ao, kms_providers);
         mongoc_auto_encryption_opts_set_keyvault_namespace (
            ao, "keyvault", "datakeys");
         encryptedClient = test_framework_new_default_client ();

         bool ok =
            mongoc_client_enable_auto_encryption (encryptedClient, ao, &error);
         ASSERT_OR_PRINT (ok, error);
         mongoc_auto_encryption_opts_destroy (ao);
      }

      bson_value_destroy (&keyid);
      mongoc_client_encryption_destroy (ce);
   }

   // Verify setup.
   {
      // Insert document with encrypted client.
      {
         mongoc_collection_t *coll =
            mongoc_client_get_collection (encryptedClient, "db", "coll");
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
         mongoc_collection_t *coll =
            mongoc_client_get_collection (unencryptedClient, "db", "coll");
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
   }

   // Run a `bulkWrite` command.
   {
      bson_t *cmd = tmp_bson (BSON_STR ({
         "bulkWrite" : 1,
         "ops" : [ {"insert" : 0, "document" : {"plainText" : "sample"}} ],
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
   mongoc_client_destroy (unencryptedClient);
}

static void
test_bulkWrite_csfle (void *unused)
{
   // Create a schema map.
   // Run a `bulkWrite` command.
   BSON_UNUSED (unused);
   ASSERT (false);
}

void
test_bulkWrite_install (TestSuite *suite)
{
   TestSuite_AddFull (suite,
                      "/bulkWrite/csfle",
                      test_bulkWrite_csfle,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_client_side_encryption
                      /* TODO: require MongoDB 8.0 */
   );
   TestSuite_AddFull (suite,
                      "/bulkWrite/qe",
                      test_bulkWrite_qe,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_client_side_encryption
                      /* TODO: require MongoDB 8.0 */
   );
}
