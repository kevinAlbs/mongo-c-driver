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

#define SET_ERROR(...) _mongoc_set_error (error, MONGOC_ERROR_CLIENT, MONGOC_ERROR_CLIENT_AUTHENTICATE, __VA_ARGS__)

static char *
get_access_token (mongoc_topology_t *tp, bson_error_t *error)
{
   char *access_token = NULL;

   bson_mutex_lock (&tp->oidc.cache.lock);

   mongoc_oidc_credential_t *cred = tp->oidc.cache.cred;

   if (NULL != cred) {
      // Credential is cached.
      access_token = bson_strdup (mongoc_oidc_credential_get_access_token (cred));
      goto done;
   }

   if (!tp->oidc.callback) {
      SET_ERROR ("MONGODB-OIDC requested, but no callback set. Use mongoc_client_set_oidc_callback or "
                 "mongoc_client_pool_set_oidc_callback.");
      goto done;
   }

   mongoc_oidc_callback_params_t *params = mongoc_oidc_callback_params_new ();
   mongoc_oidc_callback_params_set_user_data (params, mongoc_oidc_callback_get_user_data (tp->oidc.callback));
   cred = mongoc_oidc_callback_get_fn (tp->oidc.callback) (params);
   mongoc_oidc_callback_params_destroy (params);

   if (!cred) {
      SET_ERROR ("MONGODB-OIDC callback failed.");
      goto done;
   }

   access_token = bson_strdup (mongoc_oidc_credential_get_access_token (cred));
   // Transfer ownership to cache.
   tp->oidc.cache.cred = cred;
   cred = NULL;

done:
   bson_mutex_unlock (&tp->oidc.cache.lock);
   return access_token;
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
   bson_t cmd = BSON_INITIALIZER;
   bson_t reply = BSON_INITIALIZER;
   char *access_token = NULL;
   mongoc_server_stream_t *server_stream = NULL;


   access_token = get_access_token (cluster->client->topology, error);
   if (!access_token) {
      goto fail;
   }

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
   bson_free (access_token);
   bson_destroy (&cmd);
   return ok;
}
