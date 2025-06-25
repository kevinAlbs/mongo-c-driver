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

#include <mongoc/mongoc.h>
#include "TestSuite.h"

int
main (int argc, char *argv[])
{
   if (argc != 2) {
      fprintf (stderr, "usage: %s URI\n", argv[0]);
      return EXIT_FAILURE;
   }

   mongoc_init ();

   bson_error_t error;
   mongoc_uri_t *uri = mongoc_uri_new (argv[1]);
   // Do not print URI string in case it contains secrets.
   ASSERT (uri);

   mongoc_client_t *client = mongoc_client_new_from_uri (uri);
   ASSERT (client);

   if (getenv ("MONGOC_TEST_CONNECT_THUMBPRINT")) {
      mongoc_ssl_opt_t ssl_opt = {.thumbprint = getenv ("MONGOC_TEST_CONNECT_THUMBPRINT")};
      mongoc_client_set_ssl_opts (client, &ssl_opt);
   }

   ASSERT (mongoc_client_set_error_api (client, MONGOC_ERROR_API_VERSION_2));

   bson_t *ping = BCON_NEW ("ping", BCON_INT32 (1));
   bson_t reply;
   bool ok = mongoc_client_command_simple (client, "db", ping, NULL, &reply, &error);

   if (ok) {
      char *str = bson_as_canonical_extended_json (&reply, NULL);
      printf ("ping replied with: %s\n", str);
      bson_free (str);
   } else {
      test_error ("failed to ping: %s\n", error.message);
   }

   bson_destroy (&reply);
   bson_destroy (ping);
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
   mongoc_cleanup ();

   return EXIT_SUCCESS;
}
