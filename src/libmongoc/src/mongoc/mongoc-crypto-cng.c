/* Copyright 2016 MongoDB, Inc.
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

#include "mongoc-config.h"

#ifdef MONGOC_ENABLE_CRYPTO_CNG
#include "mongoc-crypto-private.h"
#include "mongoc-crypto-cng-private.h"
#include "mongoc-log.h"
#include "mongoc-thread-private.h"

#include <windows.h>
#include <stdio.h>
#include <bcrypt.h>

#define NT_SUCCESS(Status) (((NTSTATUS) (Status)) >= 0)
#define STATUS_UNSUCCESSFUL ((NTSTATUS) 0xC0000001L)

typedef struct {
   BCRYPT_ALG_HANDLE algo;
   mongoc_mutex_t mutex;
} _bcrypt_algo_t;

static _bcrypt_algo_t _sha1_hash;
static _bcrypt_algo_t _sha1_hmac;
static _bcrypt_algo_t _sha256_hash;
static _bcrypt_algo_t _sha256_hmac;

void
mongoc_crypto_cng_init (void)
{
   NTSTATUS status = STATUS_UNSUCCESSFUL;
   memset (&_sha1_hash, 0, sizeof (_sha1_hash));
   mongoc_mutex_init (&_sha1_hash.mutex);
   status = BCryptOpenAlgorithmProvider (
      &_sha1_hash.algo, BCRYPT_SHA1_ALGORITHM, NULL, 0);
   if (!NT_SUCCESS (status)) {
      MONGOC_ERROR ("BCryptOpenAlgorithmProvider(SHA1): %x", status);
   }

   memset (&_sha1_hmac, 0, sizeof (_sha1_hmac));
   mongoc_mutex_init (&_sha1_hmac.mutex);

   status = BCryptOpenAlgorithmProvider (&_sha1_hmac.algo,
                                         BCRYPT_SHA1_ALGORITHM,
                                         NULL,
                                         BCRYPT_ALG_HANDLE_HMAC_FLAG);
   if (!NT_SUCCESS (status)) {
      MONGOC_ERROR ("BCryptOpenAlgorithmProvider(SHA1 HMAC): %x", status);
   }

   memset (&_sha256_hash, 0, sizeof (_sha256_hash));
   mongoc_mutex_init (&_sha256_hash.mutex);
   status = BCryptOpenAlgorithmProvider (
      &_sha256_hash.algo, BCRYPT_SHA256_ALGORITHM, NULL, 0);
   if (!NT_SUCCESS (status)) {
      MONGOC_ERROR ("BCryptOpenAlgorithmProvider(SHA256): %x", status);
   }

   memset (&_sha256_hmac, 0, sizeof (_sha256_hmac));
   mongoc_mutex_init (&_sha256_hmac.mutex);
   status = BCryptOpenAlgorithmProvider (&_sha256_hmac.algo,
                                         BCRYPT_SHA256_ALGORITHM,
                                         NULL,
                                         BCRYPT_ALG_HANDLE_HMAC_FLAG);
   if (!NT_SUCCESS (status)) {
      MONGOC_ERROR ("BCryptOpenAlgorithmProvider(SHA256 HMAC): %x", status);
   }
}

void
mongoc_crypto_cng_cleanup (void)
{
   if (_sha1_hash.algo) {
      BCryptCloseAlgorithmProvider (&_sha1_hash.algo, 0);
   }
   if (_sha1_hmac.algo) {
      BCryptCloseAlgorithmProvider (&_sha1_hmac.algo, 0);
   }
   if (_sha256_hash.algo) {
      BCryptCloseAlgorithmProvider (&_sha256_hash.algo, 0);
   }
   if (_sha256_hash.algo) {
      BCryptCloseAlgorithmProvider (&_sha256_hash.algo, 0);
   }
}

bool
_mongoc_crypto_cng_hmac_or_hash (BCRYPT_ALG_HANDLE algorithm,
                                 void *key,
                                 size_t key_length,
                                 void *data,
                                 size_t data_length,
                                 void *output)
{
   char *hash_object_buffer = 0;
   ULONG hash_object_length = 0;
   BCRYPT_HASH_HANDLE hash = 0;
   ULONG mac_length = 0;
   NTSTATUS status = STATUS_UNSUCCESSFUL;
   bool retval = false;
   ULONG noop = 0;

   status = BCryptGetProperty (algorithm,
                               BCRYPT_OBJECT_LENGTH,
                               (char *) &hash_object_length,
                               sizeof hash_object_length,
                               &noop,
                               0);

   if (!NT_SUCCESS (status)) {
      MONGOC_ERROR ("BCryptGetProperty(): OBJECT_LENGTH %x", status);
      return false;
   }

   status = BCryptGetProperty (algorithm,
                               BCRYPT_HASH_LENGTH,
                               (char *) &mac_length,
                               sizeof mac_length,
                               &noop,
                               0);

   if (!NT_SUCCESS (status)) {
      MONGOC_ERROR ("BCryptGetProperty(): HASH_LENGTH %x", status);
      return false;
   }

   hash_object_buffer = bson_malloc (hash_object_length);

   status = BCryptCreateHash (algorithm,
                              &hash,
                              hash_object_buffer,
                              hash_object_length,
                              key,
                              (ULONG) key_length,
                              0);

   if (!NT_SUCCESS (status)) {
      MONGOC_ERROR ("BCryptCreateHash(): %x", status);
      goto cleanup;
   }

   status = BCryptHashData (hash, data, (ULONG) data_length, 0);
   if (!NT_SUCCESS (status)) {
      MONGOC_ERROR ("BCryptHashData(): %x", status);
      goto cleanup;
   }

   status = BCryptFinishHash (hash, output, mac_length, 0);
   if (!NT_SUCCESS (status)) {
      MONGOC_ERROR ("BCryptFinishHash(): %x", status);
      goto cleanup;
   }

   retval = true;

cleanup:
   if (hash) {
      (void) BCryptDestroyHash (hash);
   }

   bson_free (hash_object_buffer);
   return retval;
}

void
mongoc_crypto_cng_hmac_sha1 (mongoc_crypto_t *crypto,
                             const void *key,
                             int key_len,
                             const unsigned char *data,
                             int data_len,
                             unsigned char *hmac_out)
{
   NTSTATUS status = STATUS_UNSUCCESSFUL;

   mongoc_mutex_lock (&_sha1_hmac.mutex);

   if (!_sha1_hmac.algo) {
      mongoc_mutex_unlock (&_sha1_hmac.mutex);
      return;
   }

   _mongoc_crypto_cng_hmac_or_hash (
      _sha1_hmac.algo, key, key_len, data, data_len, hmac_out);
   mongoc_mutex_unlock (&_sha1_hmac.mutex);
}

bool
mongoc_crypto_cng_sha1 (mongoc_crypto_t *crypto,
                        const unsigned char *input,
                        const size_t input_len,
                        unsigned char *hash_out)
{
   NTSTATUS status = STATUS_UNSUCCESSFUL;
   bool res;

   mongoc_mutex_lock (&_sha1_hash.mutex);
   if (!_sha1_hash.algo) {
      mongoc_mutex_unlock (&_sha1_hash.mutex);
      return false;
   }

   res = _mongoc_crypto_cng_hmac_or_hash (
      _sha1_hash.algo, NULL, 0, input, input_len, hash_out);
   mongoc_mutex_unlock (&_sha1_hash.mutex);
   return res;
}

void
mongoc_crypto_cng_hmac_sha256 (mongoc_crypto_t *crypto,
                               const void *key,
                               int key_len,
                               const unsigned char *data,
                               int data_len,
                               unsigned char *hmac_out)
{
   NTSTATUS status = STATUS_UNSUCCESSFUL;

   mongoc_mutex_lock (&_sha256_hmac.mutex);

   if (!_sha256_hmac.algo) {
      mongoc_mutex_unlock (&_sha256_hmac.mutex);
      return;
   }

   _mongoc_crypto_cng_hmac_or_hash (
      _sha256_hmac.algo, key, key_len, data, data_len, hmac_out);
   mongoc_mutex_unlock (&_sha256_hmac.mutex);
}

bool
mongoc_crypto_cng_sha256 (mongoc_crypto_t *crypto,
                          const unsigned char *input,
                          const size_t input_len,
                          unsigned char *hash_out)
{
   NTSTATUS status = STATUS_UNSUCCESSFUL;
   bool res;

   mongoc_mutex_lock (&_sha256_hash.mutex);
   if (!_sha256_hash.algo) {
      mongoc_mutex_unlock (&_sha256_hash.mutex);
      return false;
   }

   res = _mongoc_crypto_cng_hmac_or_hash (
      _sha256_hash.algo, NULL, 0, input, input_len, hash_out);
   return res;
}
#endif
