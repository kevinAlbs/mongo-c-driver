/*
 * Copyright 2009-present MongoDB, Inc.
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

#include <common-bson-dsl-private.h>
#include <mongoc/mongoc-client-private.h>
#include <mongoc/mongoc-cluster-private.h>
#include <mongoc/mongoc-error-private.h>
#include <mongoc/mongoc-oidc-callback-private.h>
#include <mongoc/mongoc-server-description-private.h>
#include <mongoc/mongoc-stream-private.h>

struct mongoc_oidc_t {
   // oidc.callback is owned. NULL if unset. Setting oidc.callback is only expected before
   // creating connections. Setting oidc_callback does not require locking.
   mongoc_oidc_callback_t *callback;

   struct {
      // access_token is a cached OIDC access token.
      char *access_token;

      // lock is used to prevent concurrent calls to oidc.callback and guard access to oidc.cache.
      bson_mutex_t lock;
   } cache;
};

mongoc_oidc_t *
mongoc_oidc_new (void)
{
   mongoc_oidc_t *oidc = bson_malloc0 (sizeof (mongoc_oidc_t));
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

// mongoc_oidc_overwrite_access_token is used by tests to override cached tokens.
void
mongoc_oidc_overwrite_access_token (mongoc_oidc_t *oidc, const char *access_token)
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

#define SET_ERROR(...) _mongoc_set_error (error, MONGOC_ERROR_CLIENT, MONGOC_ERROR_CLIENT_AUTHENTICATE, __VA_ARGS__)

bool
mongoc_oidc_append_speculative_auth (mongoc_oidc_t *oidc,
                                     mongoc_set_t *oidc_connection_cache,
                                     uint32_t server_id,
                                     bson_t *cmd,
                                     bool *has_auth,
                                     bson_error_t *error)
{
   BSON_ASSERT_PARAM (oidc);
   BSON_ASSERT_PARAM (cmd);
   BSON_ASSERT_PARAM (has_auth);
   BSON_OPTIONAL_PARAM (error);
   bool ok = false;

   char *access_token = NULL;
   {
      bson_mutex_lock (&oidc->cache.lock);
      if (oidc->cache.access_token) {
         access_token = bson_strdup (oidc->cache.access_token);
      }
      bson_mutex_unlock (&oidc->cache.lock);
   }

   if (!access_token) {
      // No token cached yet. Not an error, but do not add auth command.
      *has_auth = false;
      return true;
   }

   // TODO: use a specific interface for updating connection cache.
   {
      // Remove cached token if exists.
      mongoc_set_rm (oidc_connection_cache, server_id);
      mongoc_set_add (oidc_connection_cache, server_id, (void *) bson_strdup (access_token));
   }

   *has_auth = true;

   // Build `saslStart` command:
   {
      bsonBuildDecl (jwt_doc, kv ("jwt", cstr (access_token)));
      if (bsonBuildError) {
         SET_ERROR ("BSON error: %s", bsonBuildError);
         goto fail;
      }

      bson_init (cmd);
      bsonBuild (*cmd,
                 kv ("saslStart", int32 (1)),
                 kv ("mechanism", cstr ("MONGODB-OIDC")),
                 kv ("payload", binary (BSON_SUBTYPE_BINARY, bson_get_data (&jwt_doc), jwt_doc.len)),
                 kv ("db", cstr ("$external")));

      if (bsonBuildError) {
         SET_ERROR ("BSON error: %s", bsonBuildError);
         bson_destroy (&jwt_doc);
         goto fail;
      }

      bson_destroy (&jwt_doc);
   }

   ok = true;
fail:
   bson_free (access_token);
   return ok;
}

static char *
get_access_token (mongoc_client_t *client, bool *is_cache, bson_error_t *error)
{
   BSON_ASSERT_PARAM (client);
   BSON_ASSERT_PARAM (is_cache);

   mongoc_topology_t *tp = client->topology;
   char *access_token = NULL;
   mongoc_oidc_credential_t *cred = NULL;

   *is_cache = false;

   bson_mutex_lock (&tp->oidc->cache.lock);

   if (NULL != tp->oidc->cache.access_token) {
      // Access token is cached.
      access_token = bson_strdup (tp->oidc->cache.access_token);
      *is_cache = true;
      goto done;
   }

   // From spec: "If both ENVIRONMENT and an OIDC Callback [...] are provided the driver MUST raise an error."
   bson_t authMechanismProperties = BSON_INITIALIZER;
   mongoc_uri_get_mechanism_properties (client->uri, &authMechanismProperties);
   if (tp->oidc->callback && bson_has_field (&authMechanismProperties, "ENVIRONMENT")) {
      SET_ERROR ("MONGODB-OIDC requested with both ENVIRONMENT and an OIDC Callback. Use one or the other.");
      goto done;
   }

   if (!tp->oidc->callback) {
      SET_ERROR ("MONGODB-OIDC requested, but no callback set. Use mongoc_client_set_oidc_callback or "
                 "mongoc_client_pool_set_oidc_callback.");
      goto done;
   }

   mongoc_oidc_callback_params_t *params = mongoc_oidc_callback_params_new ();
   mongoc_oidc_callback_params_set_user_data (params, mongoc_oidc_callback_get_user_data (tp->oidc->callback));
   {
      // From spec: "If CSOT is not applied, then the driver MUST use 1 minute as the timeout."
      // The timeout parameter (when set) is meant to be directly compared against bson_get_monotonic_time(). It is a
      // time point, not a duration.
      mongoc_oidc_callback_params_set_timeout (params, bson_get_monotonic_time () + 60 * 1000 * 1000);
   }
   cred = mongoc_oidc_callback_get_fn (tp->oidc->callback) (params);
   mongoc_oidc_callback_params_destroy (params);

   if (!cred) {
      SET_ERROR ("MONGODB-OIDC callback failed.");
      goto done;
   }

   access_token = bson_strdup (mongoc_oidc_credential_get_access_token (cred));
   tp->oidc->cache.access_token = bson_strdup (access_token); // Cache a copy.

done:
   bson_mutex_unlock (&tp->oidc->cache.lock);
   mongoc_oidc_credential_destroy (cred);
   return access_token;
}

static bool
run_sasl_start (mongoc_cluster_t *cluster,
                mongoc_stream_t *stream,
                mongoc_server_description_t *sd,
                const char *access_token,
                bson_error_t *error)
{
   BSON_ASSERT_PARAM (cluster);
   BSON_ASSERT_PARAM (stream);
   BSON_ASSERT_PARAM (sd);
   BSON_ASSERT_PARAM (access_token);
   BSON_OPTIONAL_PARAM (error);
   mongoc_server_stream_t *server_stream = NULL;
   bson_t cmd = BSON_INITIALIZER;
   bson_t reply = BSON_INITIALIZER;
   bool ok = false;
   // Build `saslStart` command:
   {
      bsonBuildDecl (jwt_doc, kv ("jwt", cstr (access_token)));
      if (bsonBuildError) {
         SET_ERROR ("BSON error: %s", bsonBuildError);
         goto fail;
      }

      bsonBuild (cmd,
                 kv ("saslStart", int32 (1)),
                 kv ("mechanism", cstr ("MONGODB-OIDC")),
                 kv ("payload", binary (BSON_SUBTYPE_BINARY, bson_get_data (&jwt_doc), jwt_doc.len)));

      if (bsonBuildError) {
         SET_ERROR ("BSON error: %s", bsonBuildError);
         bson_destroy (&jwt_doc);
         goto fail;
      }

      bson_destroy (&jwt_doc);
   }

   // Send command:
   {
      mongoc_cmd_parts_t parts;

      mc_shared_tpld td = mc_tpld_take_ref (BSON_ASSERT_PTR_INLINE (cluster)->client->topology);

      mongoc_cmd_parts_init (&parts, cluster->client, "$external", MONGOC_QUERY_NONE /* unused for OP_MSG */, &cmd);
      parts.prohibit_lsid = true; // Do not append session ids to auth commands per session spec.
      server_stream = _mongoc_cluster_create_server_stream (td.ptr, sd, stream);
      mc_tpld_drop_ref (&td);
      if (!mongoc_cluster_run_command_parts (cluster, server_stream, &parts, &reply, error)) {
         goto fail;
      }
   }

   // Expect successful reply to include `done: true`:
   {
      bsonParse (reply, require (allOf (key ("done"), isTrue), nop));
      if (bsonParseError) {
         SET_ERROR ("Error in OIDC reply: %s", bsonParseError);
         goto fail;
      }
   }

   ok = true;

fail:
   mongoc_server_stream_cleanup (server_stream);
   bson_destroy (&cmd);
   return ok;
}

static void
invalidate_cache (mongoc_cluster_t *cluster, const char *access_token)
{
   BSON_ASSERT_PARAM (cluster);
   BSON_ASSERT_PARAM (access_token);

   mongoc_topology_t *tp = cluster->client->topology;

   bson_mutex_lock (&tp->oidc->cache.lock);

   if (tp->oidc->cache.access_token && 0 == strcmp (tp->oidc->cache.access_token, access_token)) {
      bson_free (tp->oidc->cache.access_token);
      tp->oidc->cache.access_token = NULL;
   }

   bson_mutex_unlock (&tp->oidc->cache.lock);
}

bool
_mongoc_cluster_auth_node_oidc (mongoc_cluster_t *cluster,
                                mongoc_stream_t *stream,
                                mongoc_server_description_t *sd,
                                bson_error_t *error)
{
   BSON_ASSERT_PARAM (cluster);
   BSON_ASSERT_PARAM (stream);
   BSON_ASSERT_PARAM (sd);
   BSON_ASSERT_PARAM (error);

   bool ok = false;
   char *access_token = NULL;


   bool is_cache = false;
   access_token = get_access_token (cluster->client, &is_cache, error);
   if (!access_token) {
      goto done;
   }

   if (is_cache) {
      mongoc_cluster_set_oidc_connection_cache_token (cluster, sd->id, access_token);
      if (!run_sasl_start (cluster, stream, sd, access_token, error)) {
         if (error->code != MONGOC_SERVER_ERR_AUTHENTICATION) {
            goto done;
         }
         // Retry getting the access token once:
         invalidate_cache (cluster, access_token);
         access_token = get_access_token (cluster->client, &is_cache, error);
      } else {
         ok = true;
         goto done;
      }
   }

   if (!access_token) {
      goto done;
   }
   mongoc_cluster_set_oidc_connection_cache_token (cluster, sd->id, access_token);
   if (!run_sasl_start (cluster, stream, sd, access_token, error)) {
      goto done;
   }

   ok = true;
done:
   bson_free (access_token);
   return ok;
}

bool
_mongoc_cluster_reauth_node_oidc (mongoc_cluster_t *cluster,
                                  mongoc_stream_t *stream,
                                  mongoc_server_description_t *sd,
                                  bson_error_t *error)
{
   char *connection_cached_token = mongoc_cluster_get_oidc_connection_cache_token (cluster, sd->id);
   if (connection_cached_token) {
      // Invalidate shared cache:
      invalidate_cache (cluster, connection_cached_token); // Does nothing if token was already invalidated.
      // Clear connection cached:
      mongoc_cluster_set_oidc_connection_cache_token (cluster, sd->id, NULL);
   }
   return _mongoc_cluster_auth_node_oidc (cluster, stream, sd, error);
}
