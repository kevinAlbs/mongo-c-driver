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

#include "mongoc/mongoc.h"
#include "mongoc/mongoc-crypt-private.h"
#include "mongoc/mongoc-error.h"
#include "mongoc/mongoc-client-private.h"
#include "mongoc/mongoc-collection-private.h"
#include "mongoc/mongoc-opts-private.h"

#include <openssl/evp.h>

typedef struct _mongoc_crypt_t {
   mongoc_client_t *mongocrypt_client;
} mongoc_crypt_t;


/* TODO: use a new error code. */
#define SET_CRYPT_ERR(...) \
   bson_set_error (        \
      error, MONGOC_ERROR_CLIENT, MONGOC_ERROR_CLIENT_NOT_READY, __VA_ARGS__)


static void
_spawn_mongocryptd (void)
{
/* oddly, starting mongocryptd in libmongoc starts multiple instances. */
#ifdef SPAWN_BUG_FIXED
   pid_t pid = fork ();
   printf ("initializing mongocryptd\n");
   if (pid == 0) {
      int ret;
      /* child */
      printf ("child starting mongocryptd\n");
      ret = execlp ("mongocryptd", "mongocryptd", (char *) NULL);
      if (ret == -1) {
         MONGOC_ERROR ("child process unable to exec mongocryptd");
         abort ();
      }
   }
#endif
}


bool
_mongoc_client_crypt_init (mongoc_client_t *client, bson_error_t *error)
{
   /* store AWS credentials, init structures in client, store schema
    * somewhere. */
   mongoc_client_t *mongocrypt_client;
   _spawn_mongocryptd ();
   client->encryption = bson_malloc0 (sizeof (mongoc_crypt_t));
   mongocrypt_client =
      mongoc_client_new ("mongodb://%2Ftmp%2Fmongocryptd.sock");
   if (!mongocrypt_client) {
      SET_CRYPT_ERR ("Unable to create client to mongocryptd");
      return false;
   }
   client->encryption->mongocrypt_client = mongocrypt_client;
   return true;
}

//#ifdef MONGOC_ENABLE_SSL_OPENSSL
static bool
_openssl_encrypt (const uint8_t *iv,
                  const uint8_t *key,
                  const uint8_t *data,
                  uint32_t data_len,
                  uint8_t **out,
                  uint32_t *out_len,
                  bson_error_t *error)
{
   const EVP_CIPHER *cipher;
   EVP_CIPHER_CTX ctx;
   bool ret = false;
   int r;
   uint8_t *encrypted = NULL;
   int block_size, bytes_written, encrypted_len = 0;

   EVP_CIPHER_CTX_init (&ctx);
   cipher = EVP_aes_256_cbc_hmac_sha256 ();
   block_size = EVP_CIPHER_block_size (cipher);
   BSON_ASSERT (EVP_CIPHER_iv_length (cipher) == 16);
   BSON_ASSERT (block_size == 16);
   BSON_ASSERT (EVP_CIPHER_key_length (cipher) == 32);
   r = EVP_EncryptInit_ex (&ctx, cipher, NULL /* engine */, key, iv);
   if (!r) {
      /* TODO: use ERR_get_error or similar to get OpenSSL error message? */
      SET_CRYPT_ERR ("failed to initialize cipher");
      goto cleanup;
   }

   /* From `man EVP_EncryptInit`: "as a result the amount of data written may be
    * anything from zero bytes to (inl + cipher_block_size - 1)" and for
    * finalize: "should have sufficient space for one block */
   encrypted = bson_malloc0 (data_len + (block_size - 1) + block_size);
   r = EVP_EncryptUpdate (&ctx, encrypted, &bytes_written, data, data_len);
   if (!r) {
      SET_CRYPT_ERR ("failed to encrypt");
      goto cleanup;
   }

   encrypted_len += bytes_written;
   r = EVP_EncryptFinal_ex (&ctx, encrypted + bytes_written, &bytes_written);
   if (!r) {
      SET_CRYPT_ERR ("failed to finalize\n");
      goto cleanup;
   }

   encrypted_len += bytes_written;
   *out = encrypted;
   *out_len = (uint32_t) encrypted_len;
   encrypted = NULL;
   ret = true;
cleanup:
   EVP_CIPHER_CTX_cleanup (&ctx);
   bson_free (encrypted);
   return ret;
}

static bool
_openssl_decrypt (const uint8_t *iv,
                  const uint8_t *key,
                  const uint8_t *data,
                  uint32_t data_len,
                  uint8_t **out,
                  uint32_t *out_len,
                  bson_error_t *error)
{
   const EVP_CIPHER *cipher;
   EVP_CIPHER_CTX ctx;
   bool ret = false;
   int r;
   uint8_t *decrypted = NULL;
   int block_size, bytes_written, decrypted_len = 0;

   EVP_CIPHER_CTX_init (&ctx);
   cipher = EVP_aes_256_cbc_hmac_sha256 ();
   block_size = EVP_CIPHER_block_size (cipher);
   BSON_ASSERT (EVP_CIPHER_iv_length (cipher) == 16);
   BSON_ASSERT (block_size == 16);
   BSON_ASSERT (EVP_CIPHER_key_length (cipher) == 32);
   r = EVP_DecryptInit_ex (&ctx, cipher, NULL /* engine */, key, iv);
   if (!r) {
      /* TODO: use ERR_get_error or similar to get OpenSSL error message? */
      SET_CRYPT_ERR ("failed to initialize cipher");
      goto cleanup;
   }

   /* " EVP_DecryptUpdate() should have sufficient room for (inl +
     * cipher_block_size) bytes" */
   /* decrypted length <= decrypted_len. */
   decrypted = bson_malloc0 (data_len + block_size);
   r = EVP_DecryptUpdate (&ctx, decrypted, &bytes_written, data, data_len);
   if (!r) {
      SET_CRYPT_ERR ("failed to decrypt");
      goto cleanup;
   }

   decrypted_len += bytes_written;
   r = EVP_DecryptFinal_ex (&ctx, decrypted + bytes_written, &bytes_written);
   if (!r) {
      SET_CRYPT_ERR ("failed to finalize\n");
      goto cleanup;
   }

   decrypted_len += bytes_written;
   *out = decrypted;
   *out_len = (uint32_t) decrypted_len;
   decrypted = NULL;
   ret = true;
cleanup:
   EVP_CIPHER_CTX_cleanup (&ctx);
   bson_free (decrypted);
   return ret;
}
//#endif

uint8_t *
get_key (const char *key_id)
{
   uint8_t *phony = bson_malloc0 (32);
   int i;

   /* TODO: implement. verify length */
   for (i = 0; i < 32; i++)
      phony[i] = (uint8_t) i;
   return phony;
}

static bool
_append_encrypted (const uint8_t *data,
                   uint32_t data_len,
                   bson_t *out,
                   const char *field,
                   uint32_t field_len,
                   bson_error_t *error)
{
   bson_t *marking;
   bson_iter_t marking_iter;
   bool ret = false;
   /* will hold { 'k': <key id>, 'iv': <iv>, 'e': <encrypted data> } */
   bson_t encrypted_w_metadata = BSON_INITIALIZER;
   /* will hold { 'e': <encrypted data> } */
   bson_t to_encrypt = BSON_INITIALIZER;
   uint8_t *encrypted;
   uint32_t encrypted_len;
   const char *key_id;
   uint32_t key_id_len;
   uint8_t *key = NULL;
   const uint8_t *iv = NULL;
   uint32_t iv_len;

   marking = bson_new_from_data (data, data_len);
   printf ("marking=%s\n", bson_as_json (marking, NULL));
   if (!bson_iter_init_find (&marking_iter, marking, "k")) {
      SET_CRYPT_ERR ("invalid marking, no 'k'");
      goto cleanup;
   } else if (!BSON_ITER_HOLDS_UTF8 (&marking_iter)) {
      SET_CRYPT_ERR ("invalid marking, no 'k' is not utf8");
      goto cleanup;
   }
   key_id = bson_iter_utf8 (&marking_iter, &key_id_len);
   key = get_key (key_id);


   if (!bson_iter_init_find (&marking_iter, marking, "iv")) {
      SET_CRYPT_ERR ("invalid marking, no 'iv'");
      goto cleanup;
   } else if (!BSON_ITER_HOLDS_BINARY (&marking_iter)) {
      SET_CRYPT_ERR ("invalid marking, 'iv' is not binary");
      goto cleanup;
   }
   bson_iter_binary (&marking_iter, NULL, &iv_len, &iv);
   if (iv_len != 16) {
      SET_CRYPT_ERR ("iv must be 16 bytes");
      goto cleanup;
   }

   if (!bson_iter_init_find (&marking_iter, marking, "v")) {
      SET_CRYPT_ERR ("invalid marking, no 'v'");
      goto cleanup;
   } else {
      bson_append_value (&to_encrypt, "v", 1, bson_iter_value (&marking_iter));
   }

   if (!_openssl_encrypt (iv,
                          key,
                          bson_get_data (&to_encrypt),
                          to_encrypt.len,
                          &encrypted,
                          &encrypted_len,
                          error)) {
      goto cleanup;
   }

   /* append { 'k': <key id>, 'iv': <iv>, 'e': <encrypted { v: <val> } > } */
   bson_append_utf8 (&encrypted_w_metadata, "k", 1, key_id, key_id_len);
   bson_append_binary (
      &encrypted_w_metadata, "iv", 2, BSON_SUBTYPE_BINARY, iv, iv_len);
   bson_append_binary (&encrypted_w_metadata,
                       "e",
                       1,
                       BSON_SUBTYPE_BINARY,
                       encrypted,
                       encrypted_len);
   bson_append_binary (out,
                       field,
                       field_len,
                       BSON_SUBTYPE_ENCRYPTED,
                       bson_get_data (&encrypted_w_metadata),
                       encrypted_w_metadata.len);

   ret = true;

cleanup:
   bson_destroy (marking);
   bson_free (key);
   bson_destroy (&to_encrypt);
   bson_free (encrypted);
   bson_destroy (&encrypted_w_metadata);
   return ret;
}

static bool
_append_decrypted (const uint8_t *data,
                   uint32_t data_len,
                   bson_t *out,
                   const char *field,
                   uint32_t field_len,
                   bson_error_t *error)
{
   bson_t *encrypted_w_metadata;
   bson_iter_t iter;
   const char *key_id;
   uint32_t key_id_len;
   uint8_t *key = NULL;
   const uint8_t *iv = NULL;
   uint32_t iv_len;
   const uint8_t *encrypted;
   uint32_t encrypted_len;
   uint8_t *decrypted = NULL;
   uint32_t decrypted_len;
   bson_subtype_t subtype;
   bool ret = false;

   encrypted_w_metadata = bson_new_from_data (data, data_len);

   if (!bson_iter_init_find (&iter, encrypted_w_metadata, "k")) {
      SET_CRYPT_ERR ("invalid encrypted data, no 'k'");
      goto cleanup;
   } else if (!BSON_ITER_HOLDS_UTF8 (&iter)) {
      SET_CRYPT_ERR ("invalid encrypted data, no 'k' is not utf8");
      goto cleanup;
   }
   key_id = bson_iter_utf8 (&iter, &key_id_len);
   key = get_key (key_id);

   if (!bson_iter_init_find (&iter, encrypted_w_metadata, "iv")) {
      SET_CRYPT_ERR ("invalid encrypted data, no 'iv'");
      goto cleanup;
   } else if (!BSON_ITER_HOLDS_BINARY (&iter)) {
      SET_CRYPT_ERR ("invalid encrypted data, 'iv' is not binary");
      goto cleanup;
   }
   bson_iter_binary (&iter, NULL, &iv_len, &iv);

   if (!bson_iter_init_find (&iter, encrypted_w_metadata, "e")) {
      SET_CRYPT_ERR ("invalid encrypted data, no 'e'");
      goto cleanup;
   } else if (!BSON_ITER_HOLDS_BINARY (&iter)) {
      SET_CRYPT_ERR ("invalid encrypted data, 'e' does not contain binary");
      goto cleanup;
   }
   bson_iter_binary (&iter, &subtype, &encrypted_len, &encrypted);
   if (subtype != BSON_SUBTYPE_ENCRYPTED) {
      SET_CRYPT_ERR (
         "invalid encrypted data, 'e' does not contain encrypted binary");
      goto cleanup;
   }

   if (!_openssl_decrypt (iv,
                          key,
                          encrypted,
                          encrypted_len,
                          &decrypted,
                          &decrypted_len,
                          error)) {
      goto cleanup;
   } else {
      bson_t *wrapped; /* { 'v': <the value> } */
      bson_iter_t wrapped_iter;
      wrapped = bson_new_from_data (decrypted, decrypted_len);
      if (!bson_iter_init_find (&wrapped_iter, wrapped, "v")) {
         SET_CRYPT_ERR ("invalid encrypted data, missing 'v' field");
         goto cleanup;
      }
      bson_append_value (
         out, field, field_len, bson_iter_value (&wrapped_iter));
   }

   ret = true;

cleanup:
   bson_destroy (encrypted_w_metadata);
   bson_free (decrypted);
   return ret;
}

typedef enum { MARKING_TO_ENCRYPTED, ENCRYPTED_TO_PLAIN } transform_t;

/* TODO: document. */
static bool
_copy_and_transform (bson_iter_t iter,
                     bson_t *out,
                     bson_error_t *error,
                     transform_t transform)
{
   while (bson_iter_next (&iter)) {
      if (BSON_ITER_HOLDS_BINARY (&iter)) {
         bson_subtype_t subtype;
         uint32_t data_len;
         const uint8_t *data;

         bson_iter_binary (&iter, &subtype, &data_len, &data);
         if (subtype == BSON_SUBTYPE_ENCRYPTED) {
            if (transform == MARKING_TO_ENCRYPTED) {
               _append_encrypted (data,
                                  data_len,
                                  out,
                                  bson_iter_key (&iter),
                                  bson_iter_key_len (&iter),
                                  error);
            } else {
               _append_decrypted (data,
                                  data_len,
                                  out,
                                  bson_iter_key (&iter),
                                  bson_iter_key_len (&iter),
                                  error);
            }
            continue;
         }
         /* otherwise, fall through. copy over like a normal value. */
      }

      if (BSON_ITER_HOLDS_ARRAY (&iter)) {
         bson_iter_t child_iter;
         bson_t child_out;
         bool ret;

         bson_iter_recurse (&iter, &child_iter);
         bson_append_array_begin (
            out, bson_iter_key (&iter), bson_iter_key_len (&iter), &child_out);
         ret = _copy_and_transform (child_iter, &child_out, error, transform);
         bson_append_array_end (out, &child_out);
         if (!ret) {
            return false;
         }
      } else if (BSON_ITER_HOLDS_DOCUMENT (&iter)) {
         bson_iter_t child_iter;
         bson_t child_out;
         bool ret;

         bson_iter_recurse (&iter, &child_iter);
         bson_append_document_begin (
            out, bson_iter_key (&iter), bson_iter_key_len (&iter), &child_out);
         ret = _copy_and_transform (child_iter, &child_out, error, transform);
         bson_append_document_end (out, &child_out);
         if (!ret) {
            return false;
         }
      } else {
         bson_append_value (out,
                            bson_iter_key (&iter),
                            bson_iter_key_len (&iter),
                            bson_iter_value (&iter));
      }
   }
   return true;
}


static bool
_replace_markings (const bson_t *reply, bson_t *out, bson_error_t *error)
{
   bson_iter_t iter;

   BSON_ASSERT (bson_iter_init_find (&iter, reply, "ok"));
   if (!bson_iter_as_bool (&iter)) {
      SET_CRYPT_ERR ("markFields returned ok:0");
      return false;
   }

   if (!bson_iter_init_find (&iter, reply, "data")) {
      SET_CRYPT_ERR ("markFields returned ok:0");
      return false;
   }
   /* recurse into array. */
   bson_iter_recurse (&iter, &iter);
   bson_iter_next (&iter);
   /* recurse into first document. */
   bson_iter_recurse (&iter, &iter);
   _copy_and_transform (iter, out, error, MARKING_TO_ENCRYPTED);
   return true;
}

static void
_make_marking_cmd (const bson_t *data, const bson_t* schema, bson_t *cmd)
{
   bson_t child;

   bson_init (cmd);
   BSON_APPEND_INT64 (cmd, "markFields", 1);
   BSON_APPEND_ARRAY_BEGIN (cmd, "data", &child);
   BSON_APPEND_DOCUMENT (&child, "0", data);
   bson_append_array_end (cmd, &child);
   BSON_APPEND_DOCUMENT (cmd, "schema", schema);
}

bool
mongoc_crypt_encrypt (mongoc_collection_t *coll,
                      const bson_t *data,
                      bson_t *out,
                      bson_error_t *error)
{
   mongoc_client_t* client;
   bson_t cmd, schema, reply;
   bool ret;

   ret = false;
   client = coll->client;
   bson_init (out);

   /* TODO: maybe this function shouldn't check if encryption is necessary? */
   if (!_mongoc_client_get_schema (client, coll->ns, &schema)) {
      /* collection does not have encrypted fields. */
      goto cleanup;
   }

   _make_marking_cmd (data, &schema, &cmd);
   bson_destroy (&schema);
   if (!mongoc_client_command_simple (client->encryption->mongocrypt_client,
                                      "admin",
                                      &cmd,
                                      NULL /* read prefs */,
                                      &reply,
                                      error)) {
      goto cleanup;
   }

   printf ("sent %s, got %s\n",
           bson_as_json (&cmd, NULL),
           bson_as_json (&reply, NULL));

   if (!_replace_markings (&reply, out, error)) {
      goto cleanup;
   }

   ret = true;
cleanup:
   bson_destroy (&cmd);
   bson_destroy (&reply);
   return ret;
}

bool
mongoc_crypt_decrypt (mongoc_client_t *client,
                      const bson_t *data,
                      bson_t *out,
                      bson_error_t *error)
{
   bson_iter_t iter;
   bson_iter_init (&iter, data);

   if (!client->encryption) {
      return true;
   }
   bson_init (out);
   if (!_copy_and_transform (iter, out, error, ENCRYPTED_TO_PLAIN)) {
      return false;
   }
   return true;
}


/*
 * Returns false if the collection has no known encrypted fields.
 * Initializes schema regardless.
 */
bool
_mongoc_client_get_schema (mongoc_client_t *client,
                           const char *ns,
                           bson_t *schema)
{
   /* TODO: do remote fetching and use JSONSchema cache. */
   bson_iter_t array_iter;
   bson_iter_init (&array_iter, &client->encryption_opts.schemas);
   const uint8_t *data;
   uint32_t len;

   while (bson_iter_next (&array_iter)) {
      bson_iter_t doc_iter;
      bson_iter_recurse (&array_iter, &doc_iter);
      if (!bson_iter_find (&doc_iter, "ns")) {
         continue;
      }

      if (0 != strcmp (bson_iter_utf8 (&doc_iter, NULL), ns)) {
         continue;
      }

      bson_iter_recurse (&array_iter, &doc_iter);
      if (!bson_iter_find (&doc_iter, "schema")) {
         continue;
      }

      bson_iter_document (&doc_iter, &len, &data);
      bson_init_static (schema, data, len);
      return true;
   }

   bson_init (schema);
   return false;
}


mongoc_client_t *
mongoc_client_new_with_opts (mongoc_uri_t *uri,
                             bson_t *opts,
                             bson_error_t *error)
{
   mongoc_client_t *client;
   bson_iter_t iter;

   client = mongoc_client_new_from_uri (uri);
   if (!client)
      return NULL;


   /* TODO: generate-opts.py only supports top-level options. I can't nest
    * a Struct within a Struct in generate-opts.py and have it validate
    * recursively. Consider changing. */
   if (opts && bson_iter_init_find (&iter, opts, "clientSideEncryption")) {
      const uint8_t* data;
      uint32_t len;
      bson_t nested_opts;

      if (!BSON_ITER_HOLDS_DOCUMENT (&iter)) {
         bson_set_error (error,
                         MONGOC_ERROR_BSON,
                         MONGOC_ERROR_BSON_INVALID,
                         "clientSideEncryption must be a document.");
      }
      if (!_mongoc_client_crypt_init (client, error)) {
         mongoc_client_destroy (client);
         return NULL;
      }

      bson_iter_document (&iter, &len, &data);
      bson_init_static(&nested_opts, data, len);

      if (!_mongoc_client_side_encryption_opts_parse (NULL, &nested_opts, &client->encryption_opts, error)) {
         _mongoc_client_side_encryption_opts_cleanup (&client->encryption_opts);
         return NULL;
      }
   }

   return client;
}