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

#include <mongoc/mongoc-oidc-callback.h>

#include <TestSuite.h>
#include <test-conveniences.h>
#include <test-libmongoc.h>

static mongoc_oidc_credential_t *
oidc_callback_fn (mongoc_oidc_callback_params_t *params)
{
   return NULL;
}

static bool
do_find (mongoc_client_t *client, bson_error_t *error)
{
   mongoc_collection_t *coll = NULL;
   mongoc_cursor_t *cursor = NULL;
   bool ret = false;

   coll = mongoc_client_get_collection (client, "db", "coll");
   cursor = mongoc_collection_find_with_opts (coll, tmp_bson ("{}"), NULL, NULL);

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

int
main (void)
{
   mongoc_init ();
   bson_error_t error;

   // Prose test: 1.1 Callback is called during authentication
   {
      mongoc_uri_t *uri = test_framework_get_uri ();
      mongoc_uri_set_option_as_bool (uri, MONGOC_URI_RETRYREADS, false);
      mongoc_uri_set_auth_mechanism (uri, "MONGODB-OIDC");

      mongoc_client_t *client = mongoc_client_new_from_uri_with_error (uri, &error);
      ASSERT_OR_PRINT (client, error);

      // Configure OIDC callback:
      mongoc_oidc_callback_t *oidc_callback = mongoc_oidc_callback_new (oidc_callback_fn);
      mongoc_client_set_oidc_callback (client, oidc_callback);

      // Expect auth to succeed:
      ASSERT_OR_PRINT (do_find (client, &error), error);

      mongoc_oidc_callback_destroy (oidc_callback);
      mongoc_client_destroy (client);
      mongoc_uri_destroy (uri);
   }

   ASSERT (false && "Not yet implemented");
   mongoc_cleanup ();
}
