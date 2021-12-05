/*
 * Copyright 2021-present MongoDB, Inc.
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

#include "perf_util.h"
#include "parallel_single.h"
#include "perf_defines.h"

struct _parallel_single_fixture_t {
   mongoc_client_t *clients[MONGOC_DEFAULT_MAX_POOL_SIZE];
   bson_string_t *errmsg;
   bson_t ping;
};

parallel_single_fixture_t *
parallel_single_fixture_new () {
   parallel_single_fixture_t *fixture;

   fixture = bson_malloc0 (sizeof (parallel_single_fixture_t));
   fixture->errmsg = bson_string_new (NULL);
   return fixture;
}

void
parallel_single_fixture_destroy (parallel_single_fixture_t *fixture) {
   if (!fixture) {
      return;
   }
   bson_string_free (fixture->errmsg, true /* free_segment */);
   bson_free (fixture);
}

bool
parallel_single_fixture_setup (parallel_single_fixture_t *fixture)
{
   bool ret = false;
   bson_error_t error;
   mongoc_uri_t *uri = NULL;
   char *uristr = NULL;
   int i;
   bson_t *logcmd = BCON_NEW ("setParameter", BCON_INT32(1), "logLevel", BCON_INT32(0));

   uristr = perf_getenv (MONGODB_URI_ENV);
   uri = uristr ? mongoc_uri_new (uristr)
                : mongoc_uri_new ("mongodb://localhost:27017");

   bson_init (&fixture->ping);
   BCON_APPEND (&fixture->ping, "ping", BCON_INT32(1));

   /* Run one operation to open all application connections on each client. */
   for (i = 0; i < MONGOC_DEFAULT_MAX_POOL_SIZE; i++) {
      fixture->clients[i] = mongoc_client_new_from_uri (uri);

      if (!mongoc_client_command_simple (fixture->clients[i],
                                         "db",
                                         &fixture->ping,
                                         NULL /* read_prefs */,
                                         NULL /* reply */,
                                         &error)) {
         bson_string_append_printf (
            fixture->errmsg,
            "error in first ping with mongoc_client_command_simple: %s",
            error.message);
         goto done;
      }
   }

   /* Disable verbose logging. Verbose logging increases server latency of a
    * single "ping" or "find" operation. */
   if (!mongoc_client_command_simple (fixture->clients[0],
                                      "admin",
                                      logcmd,
                                      NULL /* read prefs */,
                                      NULL /* reply */,
                                      &error)) {
      bson_string_append_printf (
         fixture->errmsg,
         "error disabling verbose logging in mongoc_client_command_simple: %s",
         error.message);
      goto done;
   }

   ret = true;
done:
   bson_free (uristr);
   bson_destroy (logcmd);
   mongoc_uri_destroy (uri);

   return ret;
}

bool
parallel_single_fixture_teardown (parallel_single_fixture_t* fixture) {
   int i;

   bson_destroy (&fixture->ping);
   for (i = 0; i < MONGOC_DEFAULT_MAX_POOL_SIZE; i++) {
      mongoc_client_destroy (fixture->clients[i]);
   }
   return true;
}

const char*
parallel_single_fixture_get_error (parallel_single_fixture_t *fixture) {
   return fixture->errmsg->str;
}

bool
parallel_single_fixture_ping (parallel_single_fixture_t *fixture, int thread_index)
{
   bool ret = false;
   mongoc_client_t *client;
   bson_error_t error;

   client = fixture->clients[thread_index];

   if (!mongoc_client_command_simple (client,
                                      "db",
                                      &fixture->ping,
                                      NULL /* read prefs */,
                                      NULL /* reply */,
                                      &error)) {
      bson_string_append_printf (
         fixture->errmsg,
         "error sending ping in mongoc_client_command_simple: %s",
         error.message);
      goto done;
   }

   ret = true;
done:
   return ret;
}
