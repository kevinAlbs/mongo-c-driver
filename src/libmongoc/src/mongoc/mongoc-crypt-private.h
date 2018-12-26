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

#include "mongoc/mongoc-prelude.h"

#ifndef MONGO_C_DRIVER_MONGOC_CRYPT_PRIVATE_H
#define MONGO_C_DRIVER_MONGOC_CRYPT_PRIVATE_H

#include "bson/bson.h"
#include "mongoc/mongoc-client.h"
#include "mongoc/mongoc-error.h"

/* TODO: use a new error code. */
#define SET_CRYPT_ERR(...) \
   bson_set_error (        \
      error, MONGOC_ERROR_CLIENT, MONGOC_ERROR_CLIENT_NOT_READY, __VA_ARGS__)

typedef struct _mongoc_crypt_t {
   mongoc_client_t *mongocrypt_client;
} mongoc_crypt_t;

/* It's annoying passing around multiple values for bson binary values. */
typedef struct {
   const uint8_t *data;
   bson_subtype_t subtype;
   uint32_t len;
} mongoc_crypt_binary_t;

void
mongoc_crypt_bson_iter_binary (bson_iter_t *iter, mongoc_crypt_binary_t *out);
void
mongoc_crypt_bson_append_binary (bson_t *bson,
                                 const char *key,
                                 uint32_t key_len,
                                 mongoc_crypt_binary_t *in);

typedef struct {
   const bson_value_t *v;
   mongoc_crypt_binary_t iv;
   /* one of the following is zeroed, and the other is set. */
   mongoc_crypt_binary_t key_id;
   const char *key_alt_name;
} mongoc_crypt_marking_t;

/* consider renaming to encrypted_w_metadata? */
typedef struct {
   mongoc_crypt_binary_t e;
   mongoc_crypt_binary_t iv;
   mongoc_crypt_binary_t key_id;
} mongoc_crypt_encrypted_t;

typedef struct {
   mongoc_crypt_binary_t id;
   mongoc_crypt_binary_t key_material;
} mongoc_crypt_key_t;

bool
_mongoc_crypt_marking_parse (const bson_t *bson,
                             mongoc_crypt_marking_t *out,
                             bson_error_t *error);
bool
_mongoc_crypt_encrypted_parse (const bson_t *bson,
                               mongoc_crypt_encrypted_t *out,
                               bson_error_t *error);
bool
_mongoc_crypt_key_parse (const bson_t *bson,
                         mongoc_crypt_key_t *out,
                         bson_error_t *error);

bool
mongoc_client_crypt_init (mongoc_client_t *client, bson_error_t *err);
/* TODO: change to take a handle + schema */
bool
mongoc_crypt_encrypt (mongoc_collection_t *coll,
                      const bson_t *data,
                      bson_t *out,
                      bson_error_t *error);

bool
mongoc_crypt_decrypt (mongoc_client_t *client,
                      const bson_t *data,
                      bson_t *out,
                      bson_error_t *error);
/*
bool mongoc_crypt_encrypt(mongoc_client_t* client, const bson_t* data, bson_t*
out, bson_error_t* error);
bool mongoc_crypt_cleanup(mongoc_client_t* client);
 */

mongoc_client_t *
mongoc_client_new_with_opts (mongoc_uri_t *uri,
                             bson_t *opts,
                             bson_error_t *error);
bool
_mongoc_client_get_schema (mongoc_client_t *client,
                           const char *ns,
                           bson_t *bson);

#endif // MONGO_C_DRIVER_MONGOC_CRYPT_PRIVATE_H
