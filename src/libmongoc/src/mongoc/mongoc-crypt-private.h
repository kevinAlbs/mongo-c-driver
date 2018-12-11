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

bool
mongoc_client_crypt_init (mongoc_client_t *client, bson_error_t *err);
bool
mongoc_crypt_encrypt (mongoc_collection_t* coll,
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
