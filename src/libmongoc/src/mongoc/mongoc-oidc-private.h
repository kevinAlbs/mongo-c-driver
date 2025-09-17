/*
 * Copyright 2024-present MongoDB, Inc.
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

#ifndef MONGOC_OIDC_PRIVATE
#define MONGOC_OIDC_PRIVATE

#include <mongoc/mongoc-oidc-callback.h>
#include <mongoc/mongoc-sleep.h>

// mongoc_oidc_t stores the OIDC callback, cache, and lock.
// Expected to be shared among all clients in a pool.
typedef struct mongoc_oidc_t mongoc_oidc_t;

mongoc_oidc_t *
mongoc_oidc_new (void);

// mongoc_oidc_set_callback is not thread safe. Call before any authentication can occur.
void
mongoc_oidc_set_callback (mongoc_oidc_t *oidc, const mongoc_oidc_callback_t *cb);

const mongoc_oidc_callback_t *
mongoc_oidc_get_callback (mongoc_oidc_t *oidc);

void
mongoc_oidc_set_usleep_fn (mongoc_oidc_t *oidc, mongoc_usleep_func_t usleep_fn, void *usleep_data);

char *
mongoc_oidc_get_token (mongoc_oidc_t *oidc, bool *is_cache, bson_error_t *error);

char *
mongoc_oidc_get_cached_token (mongoc_oidc_t *oidc);

void
mongoc_oidc_invalidate_cached_token (mongoc_oidc_t *oidc, const char *token);

void
mongoc_oidc_destroy (mongoc_oidc_t *);

#endif // MONGOC_OIDC_PRIVATE
