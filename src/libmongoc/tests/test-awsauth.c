/**
 * Copyright 2022 MongoDB, Inc.
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

// test-awsauth.c tests authentication with the MONGODB-AWS authMechanism.
// It may be run in an AWS ECS task or EC2 instance.

#include <mongoc/mongoc.h>

// Ensure stdout and stderr are flushed prior to possible following abort().
#define MONGOC_STDERR_PRINTF(format, ...)    \
   if (1) {                                  \
      fflush (stdout);                       \
      fprintf (stderr, format, __VA_ARGS__); \
      fflush (stderr);                       \
   } else                                    \
      ((void) 0)

#define ASSERT(Cond)                                                           \
   if (1) {                                                                    \
      if (!(Cond)) {                                                           \
         MONGOC_STDERR_PRINTF ("FAIL:%s:%d  %s()\n  Condition '%s' failed.\n", \
                               __FILE__,                                       \
                               __LINE__,                                       \
                               BSON_FUNC,                                      \
                               BSON_STR (Cond));                               \
         abort ();                                                             \
      }                                                                        \
   } else                                                                      \
      ((void) 0)

#define ASSERTF(Cond, Fmt, ...)                                                \
   if (1) {                                                                    \
      if (!(Cond)) {                                                           \
         MONGOC_STDERR_PRINTF ("FAIL:%s:%d  %s()\n  Condition '%s' failed.\n", \
                               __FILE__,                                       \
                               __LINE__,                                       \
                               BSON_FUNC,                                      \
                               BSON_STR (Cond));                               \
         MONGOC_STDERR_PRINTF ("MESSAGE: " Fmt "\n", __VA_ARGS__);             \
         abort ();                                                             \
      }                                                                        \
   } else                                                                      \
      ((void) 0)

#define FAILF(Fmt, ...)                                                     \
   if (1) {                                                                 \
      MONGOC_STDERR_PRINTF ("FAIL:%s:%d  %s()\n  Condition '%s' failed.\n", \
                            __FILE__,                                       \
                            __LINE__,                                       \
                            BSON_FUNC,                                      \
                            BSON_STR (Cond));                               \
      MONGOC_STDERR_PRINTF ("MESSAGE: " Fmt "\n", __VA_ARGS__);             \
      abort ();                                                             \
   } else                                                                   \
      ((void) 0)

static void
test_auth (mongoc_database_t *db, bool expect_failure)
{
   bson_error_t error;
   bson_t *ping = BCON_NEW ("ping", BCON_INT32 (1));
   bool ok = mongoc_database_command_with_opts (db,
                                                ping,
                                                NULL /* read_prefs */,
                                                NULL /* opts */,
                                                NULL /* reply */,
                                                &error);
   if (expect_failure) {
      ASSERTF (!ok, "%s", "Expected auth failure, but got success");
   } else {
      ASSERTF (ok, "Expected auth success, but got error: %s", error.message);
   }
   bson_destroy (ping);
}

int
main (int argc, char *argv[])
{
   mongoc_database_t *db;
   mongoc_client_t *client;
   bson_error_t error;
   mongoc_uri_t *uri;
   bool expect_failure;

   if (argc != 3) {
      FAILF ("usage: %s URI [EXPECT_SUCCESS|EXPECT_FAILURE]\n", argv[0]);
   }

   mongoc_init ();

   uri = mongoc_uri_new_with_error (argv[1], &error);
   ASSERTF (uri, "Failed to create URI: %s", error.message);

   if (0 == strcmp (argv[2], "EXPECT_FAILURE")) {
      expect_failure = true;
   } else if (0 == strcmp (argv[2], "EXPECT_SUCCESS")) {
      expect_failure = false;
   } else {
      FAILF (
         "Expected 'EXPECT_FAILURE' or 'EXPECT_SUCCESS' for argument. Got: %s",
         argv[2]);
   }

   client = mongoc_client_new_from_uri (uri);
   ASSERT (client);

   mongoc_client_set_error_api (client, MONGOC_ERROR_API_VERSION_2);
   db = mongoc_client_get_database (client, "test");
   test_auth (db, expect_failure);

   mongoc_database_destroy (db);
   mongoc_uri_destroy (uri);
   mongoc_client_destroy (client);

   return EXIT_SUCCESS;
}
