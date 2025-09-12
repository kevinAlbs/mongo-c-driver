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

#include <TestSuite.h>
#include <test-conveniences.h>
#include <test-libmongoc.h>

static char *
read_test_token (void)
{
   FILE *token_file = fopen ("/tmp/tokens/test_machine", "r");
   ASSERT (token_file);

   // Determine length of token:
   ASSERT (0 == fseek (token_file, 0, SEEK_END));
   long token_len = ftell (token_file);
   ASSERT (token_len > 0);
   ASSERT (0 == fseek (token_file, 0, SEEK_SET));

   // Read file into buffer:
   char *token = bson_malloc (token_len + 1);
   size_t nread = fread (token, 1, token_len, token_file);
   ASSERT (nread == (size_t) token_len);
   token[token_len] = '\0';
   fclose (token_file);
   return token;
}

typedef struct {
   int call_count;
   bool validate_params;
   bool return_null;
   bool return_bad_token;
} callback_ctx_t;

static mongoc_oidc_credential_t *
oidc_callback_fn (mongoc_oidc_callback_params_t *params)
{
   callback_ctx_t *ctx = mongoc_oidc_callback_params_get_user_data (params);
   ASSERT (ctx);
   ctx->call_count += 1;

   char *token = read_test_token ();
   mongoc_oidc_credential_t *cred = mongoc_oidc_credential_new (token);
   bson_free (token);
   return cred;
}


static bool
do_find (mongoc_client_t *client, bson_error_t *error)
{
   mongoc_collection_t *coll = NULL;
   mongoc_cursor_t *cursor = NULL;
   bool ret = false;
   bson_t filter = BSON_INITIALIZER;

   coll = mongoc_client_get_collection (client, "test", "test");
   cursor = mongoc_collection_find_with_opts (coll, &filter, NULL, NULL);

   const bson_t *doc;
   while (mongoc_cursor_next (cursor, &doc))
      ;

   if (mongoc_cursor_error (cursor, error)) {
      goto fail;
   }

   ret = true;
fail:
   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (coll);
   return ret;
}

static void
configure_failpoint (const char *failpoint_json)
// Configure failpoint on a separate client:
{
   bson_error_t error;
   mongoc_uri_t *uri = mongoc_uri_new ("mongodb://localhost:27017/?retryReads=false&authMechanism=MONGODB-OIDC");
   mongoc_client_t *client = mongoc_client_new_from_uri_with_error (uri, &error);
   ASSERT_OR_PRINT (client, error);

   // Configure OIDC callback:
   mongoc_oidc_callback_t *oidc_callback = mongoc_oidc_callback_new (oidc_callback_fn);
   callback_ctx_t ctx = {0};
   mongoc_oidc_callback_set_user_data (oidc_callback, &ctx);
   mongoc_client_set_oidc_callback (client, oidc_callback);

   // Configure fail point:
   bson_t *failpoint = tmp_bson (failpoint_json);

   ASSERT_OR_PRINT (mongoc_client_command_simple (client, "admin", failpoint, NULL, NULL, &error), error);

   mongoc_oidc_callback_destroy (oidc_callback);
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
}

static void
test_oidc (void *unused)
{
   BSON_UNUSED (unused);
   bson_error_t error;

   // Test authenticating with MONGODB-OIDC and no environment or callback is specified.
   {
      mongoc_client_t *client = mongoc_client_new ("mongodb://localhost/?authMechanism=MONGODB-OIDC");
      bool ok = mongoc_client_command_simple (client, "db", tmp_bson ("{'ping': 1}"), NULL, NULL, &error);
      ASSERT (!ok);
      ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_CLIENT, MONGOC_ERROR_CLIENT_AUTHENTICATE, "no callback set");
      mongoc_client_destroy (client);
   }

   // Test set and get connection cache: single-threaded.
   {
      mongoc_uri_t *uri = mongoc_uri_new ("mongodb://localhost:27017/?retryReads=false&authMechanism=MONGODB-OIDC");
      mongoc_client_t *client = mongoc_client_new_from_uri_with_error (uri, &error);
      ASSERT_OR_PRINT (client, error);

      // Configure OIDC callback:
      mongoc_oidc_callback_t *oidc_callback = mongoc_oidc_callback_new (oidc_callback_fn);
      callback_ctx_t ctx = {0};
      mongoc_oidc_callback_set_user_data (oidc_callback, &ctx);
      mongoc_client_set_oidc_callback (client, oidc_callback);


      // Expect nothing cached yet:
      ASSERT (!mongoc_cluster_get_oidc_connection_cache_token (&client->cluster, 1));

      // Expect auth to succeed:
      ASSERT_OR_PRINT (do_find (client, &error), error);

      // Expect a token is cached:
      ASSERT (mongoc_cluster_get_oidc_connection_cache_token (&client->cluster, 1));

      // Overwrite cached token and get it back:
      {
         mongoc_cluster_set_oidc_connection_cache_token (&client->cluster, 1, "foobar");
         char *got = mongoc_cluster_get_oidc_connection_cache_token (&client->cluster, 1);
         ASSERT (got);
         ASSERT_CMPSTR (got, "foobar");
         bson_free (got);
      }

      // Clear cached token:
      {
         mongoc_cluster_set_oidc_connection_cache_token (&client->cluster, 1, NULL);
         ASSERT (!mongoc_cluster_get_oidc_connection_cache_token (&client->cluster, 1));
      }

      mongoc_client_destroy (client);
      mongoc_uri_destroy (uri);
   }

   // Test set and get connection cache: pooled
   {
      mongoc_uri_t *uri = mongoc_uri_new ("mongodb://localhost:27017/?retryReads=false&authMechanism=MONGODB-OIDC");
      mongoc_client_pool_t *pool = mongoc_client_pool_new_with_error (uri, &error);
      ASSERT_OR_PRINT (pool, error);

      // Configure OIDC callback:
      mongoc_oidc_callback_t *oidc_callback = mongoc_oidc_callback_new (oidc_callback_fn);
      callback_ctx_t ctx = {0};
      mongoc_oidc_callback_set_user_data (oidc_callback, &ctx);
      mongoc_client_pool_set_oidc_callback (pool, oidc_callback);

      mongoc_client_t *client = mongoc_client_pool_pop (pool);

      // Expect nothing cached yet:
      ASSERT (!mongoc_cluster_get_oidc_connection_cache_token (&client->cluster, 1));

      // Expect auth to succeed:
      ASSERT_OR_PRINT (do_find (client, &error), error);

      // Expect a token is cached:
      ASSERT (mongoc_cluster_get_oidc_connection_cache_token (&client->cluster, 1));

      // Overwrite cached token and get it back:
      {
         mongoc_cluster_set_oidc_connection_cache_token (&client->cluster, 1, "foobar");
         char *got = mongoc_cluster_get_oidc_connection_cache_token (&client->cluster, 1);
         ASSERT (got);
         ASSERT_CMPSTR (got, "foobar");
         bson_free (got);
      }

      // Clear cached token:
      {
         mongoc_cluster_set_oidc_connection_cache_token (&client->cluster, 1, NULL);
         ASSERT (!mongoc_cluster_get_oidc_connection_cache_token (&client->cluster, 1));
      }

      mongoc_oidc_callback_destroy (oidc_callback);
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
      mongoc_uri_destroy (uri);
   }

   // Expect a minimum required time between OIDC calls:
   {
      mongoc_uri_t *uri = mongoc_uri_new ("mongodb://localhost:27017/?retryReads=false&authMechanism=MONGODB-OIDC");
      mongoc_client_t *client = mongoc_client_new_from_uri_with_error (uri, &error);
      ASSERT_OR_PRINT (client, error);

      // Configure OIDC callback:
      mongoc_oidc_callback_t *oidc_callback = mongoc_oidc_callback_new (oidc_callback_fn);
      callback_ctx_t ctx = {0};
      mongoc_oidc_callback_set_user_data (oidc_callback, &ctx);
      mongoc_client_set_oidc_callback (client, oidc_callback);

      // Configure failpoint:
      configure_failpoint (BSON_STR ({
         "configureFailPoint" : "failCommand",
         "mode" : {"times" : 1},
         "data" : {"failCommands" : ["find"], "errorCode" : 391}
      }));

      int64_t start_us = bson_get_monotonic_time ();

      // Expect auth to succeed:
      ASSERT_OR_PRINT (do_find (client, &error), error);

      // Expect callback was called twice: once for initial auth, once for reauth.
      ASSERT_CMPINT (ctx.call_count, ==, 2);

      int64_t end_us = bson_get_monotonic_time ();

      ASSERT_CMPINT64 (end_us - start_us, >=, 100 * 1000); // At least 100ms between calls to the callback.

      mongoc_oidc_callback_destroy (oidc_callback);
      mongoc_client_destroy (client);
      mongoc_uri_destroy (uri);
   }
}

// TODO: add a test skip if environment does not support OIDC.

void
test_oidc_auth_install (TestSuite *suite)
{
   TestSuite_AddFull (suite, "/oidc", test_oidc, NULL, NULL, NULL);
}
