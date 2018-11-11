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

/* test encrypting a document. */

#include "TestSuite.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"
#include "mongoc/mongoc-crypt-private.h"

void
test_encryption (void)
{
   mongoc_client_t *client;
   mongoc_uri_t *uri;
   bson_t encrypted;
   bson_error_t error;

   uri = test_framework_get_uri ();
   mongoc_uri_set_option_as_bool (uri, "encryption", true);
   client = mongoc_client_new_from_uri (uri);
   if (!mongoc_crypt_encrypt (
      client, tmp_bson ("{'a': 1, 'encryptMe': 2, 'b': { 'encryptMe': 3 }}"), &encrypted, &error))
      printf("error: %s\n", error.message);
   printf("got back: %s\n", bson_as_json(&encrypted, NULL));
}

void
test_crypt_install (TestSuite *suite)
{
   TestSuite_AddLive (suite, "/crypt", test_encryption);
}