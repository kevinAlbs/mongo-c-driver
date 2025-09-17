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

#include <common-thread-private.h>
#include <mongoc/mongoc-error-private.h>
#include <mongoc/mongoc-oidc-callback-private.h>
#include <mongoc/mongoc-oidc-private.h>

#define SET_ERROR(...) _mongoc_set_error (error, MONGOC_ERROR_CLIENT, MONGOC_ERROR_CLIENT_AUTHENTICATE, __VA_ARGS__)

struct mongoc_oidc_t {
   // callback is owned. NULL if unset. Setting callback is only expected before
   //  creating connections. Setting callback does not require locking.
   mongoc_oidc_callback_t *callback;

   struct {
      // access_token is a cached OIDC access token.
      char *access_token;

      // Time of last call.
      int64_t last_called;

      // lock is used to prevent concurrent calls to oidc.callback and guard access to oidc.cache and oidc.last_called.
      bson_mutex_t lock;
   } cache;

   // usleep_fn is used to sleep between calls to the callback.
   mongoc_usleep_func_t usleep_fn;
   void *usleep_data;
};

mongoc_oidc_t *
mongoc_oidc_new (void)
{
   mongoc_oidc_t *oidc = bson_malloc0 (sizeof (mongoc_oidc_t));
   oidc->usleep_fn = mongoc_usleep_default_impl;
   bson_mutex_init (&oidc->cache.lock);
   return oidc;
}

void
mongoc_oidc_set_callback (mongoc_oidc_t *oidc, const mongoc_oidc_callback_t *cb)
{
   BSON_ASSERT_PARAM (oidc);
   BSON_ASSERT_PARAM (cb);
   oidc->callback = mongoc_oidc_callback_copy (cb);
}

const mongoc_oidc_callback_t *
mongoc_oidc_get_callback (mongoc_oidc_t *oidc)
{
   BSON_ASSERT_PARAM (oidc);
   return oidc->callback;
}

void
mongoc_oidc_set_usleep_fn (mongoc_oidc_t *oidc, mongoc_usleep_func_t usleep_fn, void *usleep_data)
{
   BSON_ASSERT_PARAM (oidc);
   oidc->usleep_fn = usleep_fn;
   oidc->usleep_data = usleep_data;
}

void
mongoc_oidc_destroy (mongoc_oidc_t *oidc)
{
   if (!oidc) {
      return;
   }
   bson_mutex_destroy (&oidc->cache.lock);
   mongoc_oidc_callback_destroy (oidc->callback);
   bson_free (oidc);
}

char *
mongoc_oidc_get_cached_token (mongoc_oidc_t *oidc)
{
   BSON_ASSERT_PARAM (oidc);
   bson_mutex_lock (&oidc->cache.lock);
   const char *token = oidc->cache.access_token;
   bson_mutex_unlock (&oidc->cache.lock);
   return token ? bson_strdup (token) : NULL;
}

// mongoc_oidc_set_cached_token is used by tests to override cached tokens.
void
mongoc_oidc_set_cached_token (mongoc_oidc_t *oidc, const char *access_token)
{
   BSON_ASSERT_PARAM (oidc);
   bson_mutex_lock (&oidc->cache.lock);

   if (oidc->cache.access_token) {
      bson_free (oidc->cache.access_token);
      oidc->cache.access_token = NULL;
   }

   oidc->cache.access_token = access_token ? bson_strdup (access_token) : NULL;
   bson_mutex_unlock (&oidc->cache.lock);
}

char *
mongoc_oidc_get_token (mongoc_oidc_t *oidc, bool *is_cache, bson_error_t *error)
{
   BSON_ASSERT_PARAM (oidc);
   BSON_ASSERT_PARAM (is_cache);

   char *access_token = NULL;
   mongoc_oidc_credential_t *cred = NULL;

   *is_cache = false;

   bson_mutex_lock (&oidc->cache.lock);

   if (NULL != oidc->cache.access_token) {
      // Access token is cached.
      access_token = bson_strdup (oidc->cache.access_token);
      *is_cache = true;
      goto done;
   }

   if (!oidc->callback) {
      SET_ERROR ("MONGODB-OIDC requested, but no callback set. Use mongoc_client_set_oidc_callback or "
                 "mongoc_client_pool_set_oidc_callback.");
      goto done;
   }

   mongoc_oidc_callback_params_t *params = mongoc_oidc_callback_params_new ();
   mongoc_oidc_callback_params_set_user_data (params, mongoc_oidc_callback_get_user_data (oidc->callback));
   {
      // From spec: "If CSOT is not applied, then the driver MUST use 1 minute as the timeout."
      // The timeout parameter (when set) is meant to be directly compared against bson_get_monotonic_time(). It is a
      // time point, not a duration.
      mongoc_oidc_callback_params_set_timeout (params, bson_get_monotonic_time () + 60 * 1000 * 1000);
   }

   // From spec: "Wait until it has been at least 100ms since the last callback invocation"
   if (oidc->cache.last_called != 0) {
      int64_t time_since = bson_get_monotonic_time () - oidc->cache.last_called;
      if (time_since < 100 * 1000) {
         int64_t remaining = 100 * 1000 - time_since;
         oidc->usleep_fn (remaining, oidc->usleep_data);
      }
   }

   cred = mongoc_oidc_callback_get_fn (oidc->callback) (params);

   oidc->cache.last_called = bson_get_monotonic_time ();
   mongoc_oidc_callback_params_destroy (params);

   if (!cred) {
      SET_ERROR ("MONGODB-OIDC callback failed.");
      goto done;
   }

   access_token = bson_strdup (mongoc_oidc_credential_get_access_token (cred));
   oidc->cache.access_token = bson_strdup (access_token); // Cache a copy.

done:
   bson_mutex_unlock (&oidc->cache.lock);
   mongoc_oidc_credential_destroy (cred);
   return access_token;
}

void
mongoc_oidc_invalidate_cached_token (mongoc_oidc_t *oidc, const char *access_token)
{
   BSON_ASSERT_PARAM (oidc);
   BSON_ASSERT_PARAM (access_token);

   bson_mutex_lock (&oidc->cache.lock);

   if (oidc->cache.access_token && 0 == strcmp (oidc->cache.access_token, access_token)) {
      bson_free (oidc->cache.access_token);
      oidc->cache.access_token = NULL;
   }

   bson_mutex_unlock (&oidc->cache.lock);
}
