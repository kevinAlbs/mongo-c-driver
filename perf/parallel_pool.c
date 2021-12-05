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
#include "parallel_pool.h"
#include "perf_defines.h"

struct _parallel_pool_fixture_t {
   mongoc_client_pool_t *pool;
   bson_string_t *errmsg;
   bson_t ping;
};

parallel_pool_fixture_t *
parallel_pool_fixture_new () {
   parallel_pool_fixture_t *fixture;

   fixture = bson_malloc0 (sizeof (parallel_pool_fixture_t));
   fixture->errmsg = bson_string_new (NULL);
   return fixture;
}

void
parallel_pool_fixture_destroy (parallel_pool_fixture_t *fixture) {
   if (!fixture) {
      return;
   }
   bson_string_free (fixture->errmsg, true /* free_segment */);
   bson_free (fixture);
}

bool
parallel_pool_fixture_setup (parallel_pool_fixture_t *fixture)
{
   bool ret = false;
   bson_error_t error;
   mongoc_uri_t *uri = NULL;
   char *uristr = NULL;
   mongoc_client_t *clients[MONGOC_DEFAULT_MAX_POOL_SIZE] = {0};
   int i;
   bson_t *logcmd = BCON_NEW ("setParameter", BCON_INT32(1), "logLevel", BCON_INT32(0));
   bson_t *pingcmd = BCON_NEW ("ping", BCON_INT32 (1));

   uristr = perf_getenv (MONGODB_URI_ENV);
   uri = uristr ? mongoc_uri_new (uristr)
                : mongoc_uri_new ("mongodb://localhost:27017");
   fixture->pool = mongoc_client_pool_new (uri);

   /* Pop all clients and run one operation to open all application connections.
    */
   for (i = 0; i < MONGOC_DEFAULT_MAX_POOL_SIZE; i++) {
      clients[i] = mongoc_client_pool_pop (fixture->pool);

      if (!mongoc_client_command_simple (clients[i],
                                         "db",
                                         pingcmd,
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
   if (!mongoc_client_command_simple (clients[0],
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

   for (i = 0; i < MONGOC_DEFAULT_MAX_POOL_SIZE; i++) {
      mongoc_client_pool_push (fixture->pool, clients[i]);
   }

   bson_init (&fixture->ping);
   BCON_APPEND (&fixture->ping, "ping", BCON_INT32(1));
   ret = true;
done:
   bson_destroy (pingcmd);
   bson_free (uristr);
   bson_destroy (logcmd);
   mongoc_uri_destroy (uri);

   return ret;
}

bool
parallel_pool_fixture_teardown (parallel_pool_fixture_t* fixture) {
   bson_destroy (&fixture->ping);
   mongoc_client_pool_destroy (fixture->pool);
   return true;
}

const char*
parallel_pool_fixture_get_error (parallel_pool_fixture_t *fixture) {
   return fixture->errmsg->str;
}

bool
parallel_pool_fixture_ping (parallel_pool_fixture_t *fixture, int thread_index)
{
   bool ret = false;
   mongoc_client_t *client;
   bson_error_t error;

   client = mongoc_client_pool_pop (fixture->pool);

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
   mongoc_client_pool_push (fixture->pool, client);
   return ret;
}
