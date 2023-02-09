/*
 * Copyright 2020-present MongoDB, Inc.
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

#include "mongoc-prelude.h"

#ifndef MONGOC_CLUSTER_AWS_PRIVATE_H
#define MONGOC_CLUSTER_AWS_PRIVATE_H

#include "bson/bson.h"
#include "mongoc/mongoc-cluster-private.h"
#include "common-thread-private.h" // bson_mutex_t

bool
_mongoc_cluster_auth_node_aws (mongoc_cluster_t *cluster,
                               mongoc_stream_t *stream,
                               mongoc_server_description_t *sd,
                               bson_error_t *error);

/* The following are declared in the private header for testing. It is only used
 * in test-mongoc-aws.c and mongoc-cluster.aws.c */
typedef struct {
   char *access_key_id;
   char *secret_access_key;
   char *session_token;
   // expiration is the time in milliseconds since the Epoch when these
   // credentials expire. If expiration is 0, the credentials do not have a
   // known expiration.
   uint64_t expiration_ms;
} _mongoc_aws_credentials_t;

#define MONGOC_AWS_CREDENTIALS_EXPIRATION_WINDOW_MS 60 * 5 * 1000

// _mongoc_aws_credentials_cache_t is a thread-safe cache of AWS credentials.
typedef struct _mongoc_aws_credentials_cache_t {
   struct {
      _mongoc_aws_credentials_t value;
      bool set;
   } cached;
   bson_mutex_t mutex; // guards cached.
} _mongoc_aws_credentials_cache_t;


// _mongoc_aws_credentials_cache_new creates a new cache.
_mongoc_aws_credentials_cache_t *
_mongoc_aws_credentials_cache_new (void);

// _mongoc_aws_credentials_cache_put adds credentials into `cache`.
void
_mongoc_aws_credentials_cache_put (_mongoc_aws_credentials_cache_t *cache,
                                   const _mongoc_aws_credentials_t *creds);

// _mongoc_aws_credentials_cache_get returns true if cached credentials were
// retrieved. Retrieved credentials are copied to `creds`. Returns false if
// there are no valid cached credentials.
bool
_mongoc_aws_credentials_cache_get (_mongoc_aws_credentials_cache_t *cache,
                                   _mongoc_aws_credentials_t *creds);

// _mongoc_aws_credentials_cache_clear clears credentials in `cache`.
void
_mongoc_aws_credentials_cache_clear (_mongoc_aws_credentials_cache_t *cache);

// _mongoc_aws_credentials_cache_destroy frees data for `cache`.
void
_mongoc_aws_credentials_cache_destroy (_mongoc_aws_credentials_cache_t *cache);


bool
_mongoc_aws_credentials_obtain (mongoc_uri_t *uri,
                                _mongoc_aws_credentials_t *creds,
                                bson_error_t *error);

void
_mongoc_aws_credentials_copy_to (const _mongoc_aws_credentials_t *src,
                                 _mongoc_aws_credentials_t *dst);

void
_mongoc_aws_credentials_cleanup (_mongoc_aws_credentials_t *creds);

bool
_mongoc_validate_and_derive_region (char *sts_fqdn,
                                    uint32_t sts_fqdn_len,
                                    char **region,
                                    bson_error_t *error);

#endif /* MONGOC_CLUSTER_AWS_PRIVATE_H */
