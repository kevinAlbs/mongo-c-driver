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

#include <mongoc/mongoc-topology-private.h> // To poison the client cache.

#include <mongoc/mongoc-oidc-callback.h>

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

   if (ctx->return_null) {
      return NULL;
   }

   if (ctx->return_bad_token) {
      return mongoc_oidc_credential_new ("bad_token");
   }

   if (ctx->validate_params) {
      const int64_t *timeout = mongoc_oidc_callback_params_get_timeout (params);
      ASSERT (timeout);
      // Expect the timeout to be set to 60 seconds from the start.
      ASSERT_CMPINT64 (*timeout, >=, bson_get_monotonic_time ());
      ASSERT_CMPINT64 (*timeout, <=, bson_get_monotonic_time () + 60 * 1000 * 1000);

      int version = mongoc_oidc_callback_params_get_version (params);
      ASSERT_CMPINT (version, ==, 1);

      const char *username = mongoc_oidc_callback_params_get_username (params);
      ASSERT (!username);
   }

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

static BSON_THREAD_FUN (do_100_finds, pool_void)
{
   mongoc_client_pool_t *pool = pool_void;
   for (int i = 0; i < 100; i++) {
      mongoc_client_t *client = mongoc_client_pool_pop (pool);
      bson_error_t error;
      bool ok = do_find (client, &error);
      ASSERT_OR_PRINT (ok, error);
      mongoc_client_pool_push (pool, client);
   }
   BSON_THREAD_RETURN;
}

static void
poison_client_cache (mongoc_client_t *client)
{
   BSON_ASSERT_PARAM (client);
   mongoc_topology_t *tp = client->topology;
   BSON_ASSERT_PARAM (tp);
   bson_mutex_lock (&tp->oidc.cache.lock);

   if (tp->oidc.cache.cred) {
      mongoc_oidc_credential_destroy (tp->oidc.cache.cred);
      tp->oidc.cache.cred = NULL;
   }

   // Insert an invalid token into the cache:
   tp->oidc.cache.cred = mongoc_oidc_credential_new ("bad_token");

   bson_mutex_unlock (&tp->oidc.cache.lock);
}

#define PROSE_TEST(desc) \
   for (int ctrl = printf ("%s ...\n", desc); ctrl != -1; ctrl = -1, printf ("%s ... done\n", desc))

int
main (void)
{
   mongoc_init ();
   bson_error_t error;

   PROSE_TEST ("1.1 Callback is called during authentication")
   {
      mongoc_uri_t *uri = mongoc_uri_new ("mongodb://localhost:27017/?retryReads=false&authMechanism=MONGODB-OIDC");
      mongoc_client_t *client = mongoc_client_new_from_uri_with_error (uri, &error);
      ASSERT_OR_PRINT (client, error);

      // Configure OIDC callback:
      mongoc_oidc_callback_t *oidc_callback = mongoc_oidc_callback_new (oidc_callback_fn);
      callback_ctx_t ctx = {0};
      mongoc_oidc_callback_set_user_data (oidc_callback, &ctx);
      mongoc_client_set_oidc_callback (client, oidc_callback);

      // Expect auth to succeed:
      ASSERT_OR_PRINT (do_find (client, &error), error);

      // Expect callback was called exactly once.
      ASSERT_CMPINT (ctx.call_count, ==, 1);

      mongoc_oidc_callback_destroy (oidc_callback);
      mongoc_client_destroy (client);
      mongoc_uri_destroy (uri);
   }

   PROSE_TEST ("Prose test: 1.2 Callback is called once for multiple connections")
   {
      mongoc_uri_t *uri = mongoc_uri_new ("mongodb://localhost:27017/?retryReads=false&authMechanism=MONGODB-OIDC");
      mongoc_client_pool_t *pool = mongoc_client_pool_new_with_error (uri, &error);
      ASSERT_OR_PRINT (pool, error);

      // Configure OIDC callback:
      mongoc_oidc_callback_t *oidc_callback = mongoc_oidc_callback_new (oidc_callback_fn);
      callback_ctx_t ctx = {0};
      mongoc_oidc_callback_set_user_data (oidc_callback, &ctx);
      mongoc_client_pool_set_oidc_callback (pool, oidc_callback);

      // Start 10 threads. Each thread runs 100 find operations:
      bson_thread_t threads[10];
      for (int i = 0; i < 10; i++) {
         ASSERT (0 == mcommon_thread_create (&threads[i], do_100_finds, pool));
      }

      // Wait for threads to finish:
      for (int i = 0; i < 10; i++) {
         mcommon_thread_join (threads[i]);
      }

      // Expect callback was called exactly once.
      ASSERT_CMPINT (ctx.call_count, ==, 1);

      mongoc_oidc_callback_destroy (oidc_callback);
      mongoc_uri_destroy (uri);
   }

   PROSE_TEST ("Prose test: 2.1 Valid Callback Inputs")
   {
      mongoc_uri_t *uri = mongoc_uri_new ("mongodb://localhost:27017/?retryReads=false&authMechanism=MONGODB-OIDC");
      mongoc_client_t *client = mongoc_client_new_from_uri_with_error (uri, &error);
      ASSERT_OR_PRINT (client, error);

      // Configure OIDC callback:
      mongoc_oidc_callback_t *oidc_callback = mongoc_oidc_callback_new (oidc_callback_fn);
      callback_ctx_t ctx = {.validate_params = true};
      mongoc_oidc_callback_set_user_data (oidc_callback, &ctx);
      mongoc_client_set_oidc_callback (client, oidc_callback);

      // Expect auth to succeed:
      ASSERT_OR_PRINT (do_find (client, &error), error);

      mongoc_oidc_callback_destroy (oidc_callback);
      mongoc_client_destroy (client);
      mongoc_uri_destroy (uri);
   }

   PROSE_TEST ("Prose test: 2.2 OIDC Callback Returns Null")
   {
      mongoc_uri_t *uri = mongoc_uri_new ("mongodb://localhost:27017/?retryReads=false&authMechanism=MONGODB-OIDC");
      mongoc_client_t *client = mongoc_client_new_from_uri_with_error (uri, &error);
      ASSERT_OR_PRINT (client, error);

      // Configure OIDC callback:
      mongoc_oidc_callback_t *oidc_callback = mongoc_oidc_callback_new (oidc_callback_fn);
      callback_ctx_t ctx = {.return_null = true};
      mongoc_oidc_callback_set_user_data (oidc_callback, &ctx);
      mongoc_client_set_oidc_callback (client, oidc_callback);

      // Expect auth to fail:
      ASSERT (!do_find (client, &error));
      ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_CLIENT, MONGOC_ERROR_CLIENT_AUTHENTICATE, "OIDC callback failed");

      mongoc_oidc_callback_destroy (oidc_callback);
      mongoc_client_destroy (client);
      mongoc_uri_destroy (uri);
   }

   PROSE_TEST ("Prose test: 2.3 OIDC Callback Returns Missing Data")
   {
      mongoc_uri_t *uri = mongoc_uri_new ("mongodb://localhost:27017/?retryReads=false&authMechanism=MONGODB-OIDC");
      mongoc_client_t *client = mongoc_client_new_from_uri_with_error (uri, &error);
      mongoc_client_set_error_api (client, MONGOC_ERROR_API_VERSION_2);
      ASSERT_OR_PRINT (client, error);

      // Configure OIDC callback:
      mongoc_oidc_callback_t *oidc_callback = mongoc_oidc_callback_new (oidc_callback_fn);
      // mongoc_oidc_credential_t cannot be partially created. Instead of "missing" data, return a bad token.
      callback_ctx_t ctx = {.return_bad_token = true};
      mongoc_oidc_callback_set_user_data (oidc_callback, &ctx);
      mongoc_client_set_oidc_callback (client, oidc_callback);

      // Expect auth to fail:
      ASSERT (!do_find (client, &error));
      ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_SERVER, 18, "Authentication failed");

      mongoc_oidc_callback_destroy (oidc_callback);
      mongoc_client_destroy (client);
      mongoc_uri_destroy (uri);
   }

   PROSE_TEST ("Prose test: 2.4 Invalid Client Configuration with Callback")
   {
      mongoc_uri_t *uri =
         mongoc_uri_new ("mongodb://localhost:27017/"
                         "?retryReads=false&authMechanism=MONGODB-OIDC&authMechanismProperties=ENVIRONMENT:test");
      mongoc_client_t *client = mongoc_client_new_from_uri_with_error (uri, &error);
      mongoc_client_set_error_api (client, MONGOC_ERROR_API_VERSION_2);
      ASSERT_OR_PRINT (client, error);

      // Configure OIDC callback:
      mongoc_oidc_callback_t *oidc_callback = mongoc_oidc_callback_new (oidc_callback_fn);
      // mongoc_oidc_credential_t cannot be partially created. Instead of "missing" data, return a bad token.
      callback_ctx_t ctx = {0};
      mongoc_oidc_callback_set_user_data (oidc_callback, &ctx);
      mongoc_client_set_oidc_callback (client, oidc_callback);

      // Expect auth to fail:
      ASSERT (!do_find (client, &error));
      ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_CLIENT, MONGOC_ERROR_CLIENT_AUTHENTICATE, "Use one or the other");

      mongoc_oidc_callback_destroy (oidc_callback);
      mongoc_client_destroy (client);
      mongoc_uri_destroy (uri);
   }

   PROSE_TEST ("Prose test: 2.5 Invalid use of ALLOWED_HOSTS")
   {
      mongoc_uri_t *uri = mongoc_uri_new_with_error (
         "mongodb://localhost:27017/"
         "?retryReads=false&authMechanism=MONGODB-OIDC&authMechanismProperties=ENVIRONMENT:azure,ALLOWED_HOSTS:",
         &error);
      ASSERT (!uri);
      ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "Unsupported");
      mongoc_uri_destroy (uri);
   }

   PROSE_TEST ("Prose test: 3.1 Authentication failure with cached tokens fetch a new token and retry auth")
   {
      mongoc_uri_t *uri = mongoc_uri_new ("mongodb://localhost:27017/?retryReads=false&authMechanism=MONGODB-OIDC");
      mongoc_client_t *client = mongoc_client_new_from_uri_with_error (uri, &error);
      ASSERT_OR_PRINT (client, error);

      // Configure OIDC callback:
      mongoc_oidc_callback_t *oidc_callback = mongoc_oidc_callback_new (oidc_callback_fn);
      callback_ctx_t ctx = {0};
      mongoc_oidc_callback_set_user_data (oidc_callback, &ctx);
      mongoc_client_set_oidc_callback (client, oidc_callback);

      poison_client_cache (client);

      // Expect auth to succeed:
      ASSERT_OR_PRINT (do_find (client, &error), error);

      // Expect callback was called exactly once.
      ASSERT_CMPINT (ctx.call_count, ==, 1);

      mongoc_oidc_callback_destroy (oidc_callback);
      mongoc_client_destroy (client);
      mongoc_uri_destroy (uri);
   }

   PROSE_TEST ("3.2 Authentication failures without cached tokens return an error")
   {
      mongoc_uri_t *uri = mongoc_uri_new ("mongodb://localhost:27017/?retryReads=false&authMechanism=MONGODB-OIDC");
      mongoc_client_t *client = mongoc_client_new_from_uri_with_error (uri, &error);
      ASSERT_OR_PRINT (client, error);
      mongoc_client_set_error_api (client, MONGOC_ERROR_API_VERSION_2);

      // Configure OIDC callback:
      mongoc_oidc_callback_t *oidc_callback = mongoc_oidc_callback_new (oidc_callback_fn);
      callback_ctx_t ctx = {.return_bad_token = true};
      mongoc_oidc_callback_set_user_data (oidc_callback, &ctx);
      mongoc_client_set_oidc_callback (client, oidc_callback);

      poison_client_cache (client);

      // Expect auth to fail:
      ASSERT (!do_find (client, &error));
      ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_SERVER, 18, "Authentication failed");

      // Expect callback was called exactly once.
      ASSERT_CMPINT (ctx.call_count, ==, 1);

      mongoc_oidc_callback_destroy (oidc_callback);
      mongoc_client_destroy (client);
      mongoc_uri_destroy (uri);
   }

   ASSERT (false && "Not yet implemented");
   mongoc_cleanup ();
}
