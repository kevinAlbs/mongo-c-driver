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
#include "mongoc-cluster-aws-private.h"
#include "mongoc-client-private.h"

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

// creds_eq returns true if `a` and `b` contain the same credentials.
static bool
creds_eq (_mongoc_aws_credentials_t *a, _mongoc_aws_credentials_t *b)
{
   BSON_ASSERT_PARAM (a);
   BSON_ASSERT_PARAM (b);

   if (0 != strcmp (a->access_key_id, b->access_key_id)) {
      return false;
   }
   if (0 != strcmp (a->secret_access_key, b->secret_access_key)) {
      return false;
   }
   if (0 != strcmp (a->session_token, b->session_token)) {
      return false;
   }
   if (a->expiration_ms != b->expiration_ms) {
      return false;
   }
   return true;
}

// typedef struct {
//    char str[1024];
// } creds_str_ret;

// static creds_str_ret
// creds_str (_mongoc_aws_credentials_t *creds)
// {
//    creds_str_ret ret;
//    bson_snprintf (ret.str,
//                   sizeof (ret.str),
//                   "access_key_id=%s, secret_access_key=%s, session_token=%s,
//                   " "expiration_ms=%" PRId64, creds->access_key_id,
//                   creds->secret_access_key,
//                   creds->session_token,
//                   creds->expiration_ms);
//    return ret;
// }

static void
do_find (mongoc_client_t *client, const char *expect_error_message)
{
   bson_t *filter = bson_new ();
   mongoc_collection_t *coll =
      mongoc_client_get_collection (client, "aws", "coll");
   mongoc_cursor_t *cursor = mongoc_collection_find_with_opts (
      coll, filter, NULL /* opts */, NULL /* read prefs */);
   const bson_t *doc;
   while (mongoc_cursor_next (cursor, &doc))
      ;
   bson_error_t error;
   if (NULL == expect_error_message) {
      ASSERTF (!mongoc_cursor_error (cursor, &error),
               "unexpected error in mongoc_cursor_next: %s",
               error.message);
   } else {
      ASSERTF (mongoc_cursor_error (cursor, &error),
               "%s",
               "expected error on mongoc_cursor_next, but got success");
      ASSERTF (NULL != strstr (error.message, expect_error_message),
               "expected error to contain '%s', but got '%s'",
               expect_error_message,
               error.message);
   }
   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (coll);
   bson_destroy (filter);
}

// test_cache implements the "Cached Credentials" tests from the specification.
static void
test_cache (const mongoc_uri_t *uri)
{
   // TODO: consider conditioning this test to only run in expected
   // environments.

   // Clear the cache.
   _mongoc_aws_credentials_cache_clear ();

   // Create a new client.
   // Ensure that a ``find`` operation adds credentials to the cache.
   {
      mongoc_client_t *client = mongoc_client_new_from_uri (uri);
      ASSERT (client);

      do_find (client, NULL /* expect success */);
      _mongoc_aws_credentials_t creds;
      bool found = _mongoc_aws_credentials_cache_get (&creds);
      ASSERT (found);
      _mongoc_aws_credentials_cleanup (&creds);
      mongoc_client_destroy (client);
   }
   // Override the cached credentials with an "Expiration" that is within one
   // minute of the current UTC time.
   _mongoc_aws_credentials_t first_cached;
   {
      ASSERT (mongoc_aws_credentials_cache.cached.set);
      struct timeval now;
      ASSERT (0 == bson_gettimeofday (&now));
      uint64_t now_ms = (1000 * now.tv_sec) + (now.tv_usec / 1000);
      mongoc_aws_credentials_cache.cached.value.expiration_ms =
         now_ms + (60 * 1000);
      _mongoc_aws_credentials_copy_to (
         &mongoc_aws_credentials_cache.cached.value, &first_cached);
   }

   // Create a new client.
   // Ensure that a ``find`` operation updates the credentials in the cache.
   {
      mongoc_client_t *client = mongoc_client_new_from_uri (uri);
      ASSERT (client);
      do_find (client, NULL /* expect success */);

      _mongoc_aws_credentials_t creds;
      bool found = _mongoc_aws_credentials_cache_get (&creds);
      ASSERT (found);
      ASSERTF (
         !creds_eq (&first_cached, &mongoc_aws_credentials_cache.cached.value),
         "%s",
         "expected unequal credentials, got equal");
      _mongoc_aws_credentials_cleanup (&creds);
      mongoc_client_destroy (client);
   }
   _mongoc_aws_credentials_cleanup (&first_cached);

   // TODO implement the following tests:

   // Poison the cache with an invalid access key id.
   // Create a new client.
   // Ensure that a ``find`` operation results in an error.
   // Ensure that the cache has been cleared.
   // Ensure that a subsequent ``find`` operation succeeds.
   // Ensure that the cache has been set.
   {
      ASSERT (mongoc_aws_credentials_cache.cached.set);
      bson_free (mongoc_aws_credentials_cache.cached.value.access_key_id);
      mongoc_aws_credentials_cache.cached.value.access_key_id =
         bson_strdup ("invalid");

      mongoc_client_t *client = mongoc_client_new_from_uri (uri);
      ASSERT (client);

      do_find (client, "Authentication failed");
      _mongoc_aws_credentials_t creds;
      bool found = _mongoc_aws_credentials_cache_get (&creds);
      ASSERT (!found);
      _mongoc_aws_credentials_cleanup (&creds);

      do_find (client, NULL /* expect success */);
      found = _mongoc_aws_credentials_cache_get (&creds);
      ASSERT (found);
      _mongoc_aws_credentials_cleanup (&creds);

      mongoc_client_destroy (client);
   }

   // Create a new client.
   // Ensure that a ``find`` operation adds credentials to the cache.
   // Set the AWS environment variables based on the cached credentials.
   // Clear the cache.
   // Ensure that a ``find`` operation succeeds and does not add credentials to
   // the cache.
   // Set the AWS environment variables to invalid values.
   // Ensure that a ``find`` operation results in an error.

   // Create a new client.
   // Ensure that a ``find`` operation adds credentials to the cache.
   // Set the AWS environment variables to invalid values.
   // Ensure that a ``find`` operation succeeds.
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
   test_cache (uri);

   mongoc_database_destroy (db);
   mongoc_uri_destroy (uri);
   mongoc_client_destroy (client);

   printf ("%s tests passed\n", argv[0]);

   mongoc_cleanup ();
   return EXIT_SUCCESS;
}
