/*
 * Copyright 2018-present MongoDB, Inc.
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

/* test encrypting a document. */

#include "TestSuite.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"
#include "mongoc/mongoc-crypt-private.h"
#include "mongoc/mongoc-collection-private.h"

#include "openssl/evp.h"

static void
_append_encryption_opts (bson_t *client_opts)
{
   bson_json_reader_t *reader;
   bson_t schemas = BSON_INITIALIZER;
   bson_t encryption_opts;
   bson_error_t error;
   int status;


   reader = bson_json_reader_new_from_file ("./build/example.schemas", &error);
   ASSERT_OR_PRINT (reader, error);

   status = bson_json_reader_read (reader, &schemas, &error);
   ASSERT_OR_PRINT (status == 1, error);

   BSON_APPEND_DOCUMENT_BEGIN (
      client_opts, "clientSideEncryption", &encryption_opts);
   BSON_APPEND_DOCUMENT (&encryption_opts, "schemas", &schemas);
   BSON_APPEND_UTF8 (&encryption_opts,
                     "awsAccessKeyId",
                     test_framework_getenv ("AWS_ACCESS_KEY_ID"));
   BSON_APPEND_UTF8 (&encryption_opts,
                     "awsSecretAccessKey",
                     test_framework_getenv ("AWS_SECRET_ACCESS_KEY"));
   BSON_APPEND_UTF8 (
      &encryption_opts, "awsRegion", test_framework_getenv ("AWS_REGION"));
   bson_append_document_end (client_opts, &encryption_opts);
   printf ("opts are: %s\n", tmp_json (&encryption_opts));
   bson_destroy (&schemas);
   bson_json_reader_destroy (reader);
}

void
test_encryption_with_schema (void)
{
   bson_error_t error;
   bson_t client_opts = BSON_INITIALIZER;
   bson_t schema;
   mongoc_uri_t *uri;
   mongoc_client_t *client;
   mongoc_collection_t *coll;
   bson_t encrypted, decrypted;
   bool ret;

   uri = mongoc_uri_new_with_error ("mongodb://localhost:27017/", &error);
   ASSERT_OR_PRINT (uri, error);

   _append_encryption_opts (&client_opts);

   client = mongoc_client_new_with_opts (uri, &client_opts, &error);
   ASSERT_OR_PRINT (client, error);

   coll = mongoc_client_get_collection (client, "test", "crypt");
   BSON_ASSERT (_mongoc_client_get_schema (client, coll->ns, &schema));

   ret = mongoc_crypt_encrypt (
      client->crypt,
      &schema,
      tmp_bson ("{'name': 'Todd Davis', 'ssn': '457-55-5642'}"),
      &encrypted,
      &error);

   ASSERT_OR_PRINT (ret, error);

   printf ("encrypted data=%s\n", bson_as_json (&encrypted, NULL));
   printf ("error=%s\n", error.message);

   /* And now decrypt it back. */
   ret = mongoc_crypt_decrypt (client->crypt, &encrypted, &decrypted, &error);
   printf ("decrypted data=%s\n", bson_as_json (&decrypted, NULL));

   ASSERT_OR_PRINT (ret, error);

   bson_destroy (&schema);

   bson_destroy (&client_opts);
   mongoc_collection_destroy (coll);
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
}

static bson_t *
_find_one (mongoc_collection_t *coll, bson_t *filter)
{
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bson_t *returning;

   cursor = mongoc_collection_find_with_opts (coll, filter, NULL, NULL);
   ASSERT (mongoc_cursor_next (cursor, &doc));
   returning = bson_copy (doc);
   mongoc_cursor_destroy (cursor);
   return returning;
}

#define FIND_ONE(coll) _find_one (coll, tmp_bson ("{}"))

static void
_assert_encrypted (mongoc_collection_t *coll_w_enc, char *field)
{
   /* creates an unencrypted client. */
   mongoc_client_t *client;
   mongoc_collection_t *coll;
   mongoc_cursor_t *cursor;
   const bson_t *doc;

   client = mongoc_client_new_from_uri (coll_w_enc->client->uri);
   coll = mongoc_client_get_collection (
      client, coll_w_enc->db, coll_w_enc->collection);
   cursor =
      mongoc_collection_find_with_opts (coll, tmp_bson ("{}"), NULL, NULL);

   while (mongoc_cursor_next (cursor, &doc)) {
      bson_iter_t iter;
      mongoc_crypt_binary_t binary;

      ASSERT (bson_iter_init_find (&iter, doc, field));
      ASSERT (BSON_ITER_HOLDS_BINARY (&iter));
      mongoc_crypt_binary_from_iter_unowned (&iter, &binary);
      ASSERT (binary.subtype == BSON_SUBTYPE_ENCRYPTED);
   }
}


void
test_encryption_round_trip (void)
{
   bson_error_t error;
   bson_t client_opts = BSON_INITIALIZER;
   bson_t schema;
   mongoc_uri_t *uri;
   mongoc_client_t *client;
   mongoc_collection_t *coll;
   bool ret;
   bson_t *returned;
   const char *json = "{ 'name': 'Todd Davis', 'ssn': '457-55-5642' }";

   uri = mongoc_uri_new_with_error ("mongodb://localhost:27017/", &error);
   ASSERT_OR_PRINT (uri, error);

   _append_encryption_opts (&client_opts);

   client = mongoc_client_new_with_opts (uri, &client_opts, &error);
   ASSERT_OR_PRINT (client, error);

   coll = mongoc_client_get_collection (client, "test", "crypt");
   BSON_ASSERT (_mongoc_client_get_schema (client, coll->ns, &schema));

   (void) mongoc_collection_drop (coll,
                                  &error); /* no worries if ns not found. */

   ret =
      mongoc_collection_insert_one (coll, tmp_bson (json), NULL, NULL, &error);
   ASSERT_OR_PRINT (ret, error);

   returned = FIND_ONE (coll);
   ASSERT_MATCH (returned, json);

   _assert_encrypted (coll, "ssn");

   bson_destroy (&schema);
   bson_destroy (&client_opts);
   mongoc_collection_destroy (coll);
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
}


void
test_openssl (void)
{
   uint8_t ex_data[100] = {0};
   uint8_t *encrypted = NULL, *decrypted = NULL;
   uint8_t key[32]; /* 256 bits */
   uint8_t iv[16];  /* provided by schema - apparently it's 16 bytes? */
   int i, ret, block_size, bytes_written, encrypted_len = 0, decrypted_len = 0;
   const EVP_CIPHER *cipher;
   EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new ();
   EVP_CIPHER_CTX_init (ctx);

   for (i = 0; i < sizeof (key); i++) {
      key[i] = (uint8_t) i;
   }

   for (i = 0; i < sizeof (iv); i++) {
      iv[i] = (uint8_t) i;
   }

   for (i = 0; i < sizeof (ex_data); i++) {
      ex_data[i] = (uint8_t) i;
   }

   cipher = EVP_aes_256_cbc_hmac_sha256 ();
   block_size = EVP_CIPHER_block_size (cipher);
   ASSERT_CMPINT (EVP_CIPHER_iv_length (cipher), ==, 16);
   ASSERT_CMPINT (EVP_CIPHER_block_size (cipher), ==, 16);
   ASSERT_CMPINT (EVP_CIPHER_key_length (cipher), ==, 32);

   ret = EVP_EncryptInit_ex (ctx, cipher, NULL /* engine */, key, iv);
   if (!ret) {
      ASSERT_WITH_MSG (false, "failed to initialize cipher\n");
   }

   // From man EVP_EncryptInit:
   // "as a result the amount of data written may be anything from zero bytes to
   // (inl + cipher_block_size - 1)"
   // and for finalize: "should have sufficient space for one block */
   encrypted = bson_malloc0 (sizeof (ex_data) + (block_size - 1) + block_size);
   ret = EVP_EncryptUpdate (
      ctx, encrypted, &bytes_written, ex_data, sizeof (ex_data));
   if (!ret) {
      ASSERT_WITH_MSG (false, "failed to encrypt\n");
   }
   encrypted_len += bytes_written;

   ret = EVP_EncryptFinal_ex (ctx, encrypted + bytes_written, &bytes_written);
   if (!ret) {
      ASSERT_WITH_MSG (false, "failed to finalize\n");
   }
   encrypted_len += bytes_written;

   printf ("encrypted data: \n");
   for (i = 0; i < encrypted_len; i++) {
      printf ("%x ", encrypted[i]);
   }
   printf ("\n");

   /* Now - let's decrypt :) */
   ret = EVP_DecryptInit_ex (ctx, cipher, NULL /* engine. */, key, iv);
   if (!ret) {
      ASSERT_WITH_MSG (false, "failed to init decryption\n");
   }

   /* " EVP_DecryptUpdate() should have sufficient room for (inl +
    * cipher_block_size) bytes" */
   /* decrypted length <= encrypted_len. */
   decrypted = bson_malloc0 ((size_t) encrypted_len + block_size);
   EVP_DecryptUpdate (ctx, decrypted, &bytes_written, encrypted, encrypted_len);
   decrypted_len += bytes_written;
   EVP_DecryptFinal_ex (ctx, decrypted + bytes_written, &decrypted_len);
   decrypted_len += bytes_written;

   printf ("decrypted: %d bytes\n", decrypted_len);
   for (i = 0; i < decrypted_len; i++) {
      printf ("%x ", decrypted[i]);
   }
   EVP_CIPHER_CTX_free (ctx);
}

void
test_crypt_install (TestSuite *suite)
{
   TestSuite_AddLive (suite, "/openssl", test_openssl);
   TestSuite_AddLive (suite, "/crypt", test_encryption_with_schema);
   TestSuite_AddLive (suite, "/crypt/round_trip", test_encryption_round_trip);
}