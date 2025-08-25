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

static void
test_oidc (void *unused)
{
   BSON_UNUSED (unused);

   // Test authenticating with MONGODB-OIDC and no environment or callback is specified.
   {
      mongoc_client_t *client = mongoc_client_new ("mongodb://localhost/?authMechanism=MONGODB-OIDC");
      bson_error_t error;
      bool ok = mongoc_client_command_simple (client, "db", tmp_bson ("{'ping': 1}"), NULL, NULL, &error);
      ASSERT (!ok);
      ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_CLIENT, MONGOC_ERROR_CLIENT_AUTHENTICATE, "no callback set");
      mongoc_client_destroy (client);
   }
}

void
test_oidc_auth_install (TestSuite *suite)
{
   TestSuite_AddFull (suite, "/oidc", test_oidc, NULL, NULL, NULL);
}
