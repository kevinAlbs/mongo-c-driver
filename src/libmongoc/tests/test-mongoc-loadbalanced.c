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

#include "mongoc/mongoc.h"
#include "mongoc/mongoc-client-session-private.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"
#include "TestSuite.h"

typedef struct {
   int server_changed_events;
   int server_opening_events;
   int server_closed_events;
   int topology_changed_events;
   int topology_opening_events;
   int topology_closed_events;
} stats_t;

static void
server_changed (const mongoc_apm_server_changed_t *event)
{
   stats_t *context;

   context = (stats_t *) mongoc_apm_server_changed_get_context (event);
   context->server_changed_events++;
}


static void
server_opening (const mongoc_apm_server_opening_t *event)
{
   stats_t *context;

   context = (stats_t *) mongoc_apm_server_opening_get_context (event);
   context->server_opening_events++;
}


static void
server_closed (const mongoc_apm_server_closed_t *event)
{
   stats_t *context;

   context = (stats_t *) mongoc_apm_server_closed_get_context (event);
   context->server_closed_events++;
}


static void
topology_changed (const mongoc_apm_topology_changed_t *event)
{
   stats_t *context;

   context = (stats_t *) mongoc_apm_topology_changed_get_context (event);
   context->topology_changed_events++;
}


static void
topology_opening (const mongoc_apm_topology_opening_t *event)
{
   stats_t *context;

   context = (stats_t *) mongoc_apm_topology_opening_get_context (event);
   context->topology_opening_events++;
}


static void
topology_closed (const mongoc_apm_topology_closed_t *event)
{
   stats_t *context;

   context = (stats_t *) mongoc_apm_topology_closed_get_context (event);
   context->topology_closed_events++;
}

static mongoc_apm_callbacks_t * make_callbacks (void) {
   mongoc_apm_callbacks_t *cbs;

   cbs = mongoc_apm_callbacks_new ();
   mongoc_apm_set_server_changed_cb (cbs, server_changed);
   mongoc_apm_set_server_opening_cb (cbs, server_opening);
   mongoc_apm_set_server_closed_cb (cbs, server_closed);
   mongoc_apm_set_topology_changed_cb (cbs, topology_changed);
   mongoc_apm_set_topology_opening_cb (cbs, topology_opening);
   mongoc_apm_set_topology_closed_cb (cbs, topology_closed);
   return cbs;
}

static stats_t * set_client_callbacks (mongoc_client_t *client) {
   mongoc_apm_callbacks_t *cbs;
   stats_t *stats;

   stats = bson_malloc0 (sizeof (stats_t));
   cbs = make_callbacks ();
   mongoc_client_set_apm_callbacks (client, cbs, stats);
   mongoc_apm_callbacks_destroy (cbs);
   return stats;
}

static stats_t * set_client_pool_callbacks (mongoc_client_pool_t *pool) {
   mongoc_apm_callbacks_t *cbs;
   stats_t *stats;

   stats = bson_malloc0 (sizeof (stats_t));
   cbs = make_callbacks ();
   mongoc_client_pool_set_apm_callbacks (pool, cbs, stats);
   mongoc_apm_callbacks_destroy (cbs);
   return stats;
}

static void free_and_assert_stats (stats_t *stats) {
   ASSERT_CMPINT (stats->topology_opening_events, ==, 1);
   ASSERT_CMPINT (stats->topology_changed_events, ==, 2);
   ASSERT_CMPINT (stats->server_opening_events, ==, 1);
   ASSERT_CMPINT (stats->server_changed_events, ==, 1);
   ASSERT_CMPINT (stats->server_closed_events, ==, 1);
   ASSERT_CMPINT (stats->topology_closed_events, ==, 1);
   bson_free (stats);
}

static char *
loadbalanced_uri (void)
{
   /* TODO (CDRIVER-4062): This will need to add TLS and auth to the URI when
    * run in evergreen. */
   return test_framework_getenv ("SINGLE_MONGOS_LB_URI");
}

static void
test_loadbalanced_sessions_supported (void *unused)
{
   mongoc_client_t *client;
   mongoc_client_session_t *session;
   char *uristr = loadbalanced_uri ();
   bson_error_t error;

   client = mongoc_client_new (uristr);
   session = mongoc_client_start_session (client, NULL /* opts */, &error);
   ASSERT_OR_PRINT (session, error);

   mongoc_client_session_destroy (session);
   bson_free (uristr);
   mongoc_client_destroy (client);
}

static void
test_loadbalanced_sessions_do_not_expire (void *unused)
{
   mongoc_client_t *client;
   mongoc_client_session_t *session1;
   mongoc_client_session_t *session2;
   char *uristr = loadbalanced_uri ();
   bson_error_t error;
   bson_t *session1_lsid;
   bson_t *session2_lsid;

   client = mongoc_client_new (uristr);
   /* Mock a timeout so session expiration applies. */
   client->topology->description.session_timeout_minutes = 1;

   /* Start two sessions, to ensure that pooled sessions remain in the pool when
    * the pool is accessed. */
   session1 = mongoc_client_start_session (client, NULL /* opts */, &error);
   ASSERT_OR_PRINT (session1, error);
   session2 = mongoc_client_start_session (client, NULL /* opts */, &error);
   ASSERT_OR_PRINT (session2, error);

   session1_lsid = bson_copy (mongoc_client_session_get_lsid (session1));
   session2_lsid = bson_copy (mongoc_client_session_get_lsid (session2));

   /* Expire both sessions. */
   session1->server_session->last_used_usec = 1;
   session2->server_session->last_used_usec = 1;
   mongoc_client_session_destroy (session1);
   mongoc_client_session_destroy (session2);

   /* Get a new session, it should reuse the most recently pushed session2. */
   session2 = mongoc_client_start_session (client, NULL /* opts */, &error);
   ASSERT_OR_PRINT (session2, error);
   if (!bson_equal (mongoc_client_session_get_lsid (session2), session2_lsid)) {
      test_error ("Session not reused: %s != %s",
                  tmp_json (mongoc_client_session_get_lsid (session2)),
                  tmp_json (session2_lsid));
   }

   session1 = mongoc_client_start_session (client, NULL /* opts */, &error);
   ASSERT_OR_PRINT (session1, error);
   if (!bson_equal (mongoc_client_session_get_lsid (session1), session1_lsid)) {
      test_error ("Session not reused: %s != %s",
                  tmp_json (mongoc_client_session_get_lsid (session1)),
                  tmp_json (session1_lsid));
   }

   bson_destroy (session1_lsid);
   bson_destroy (session2_lsid);
   bson_free (uristr);
   mongoc_client_session_destroy (session1);
   mongoc_client_session_destroy (session2);
   mongoc_client_destroy (client);
}

/* Test that invalid loadBalanced URI configurations are validated during client
 * construction. */
static void
test_loadbalanced_client_uri_validation (void *unused)
{
   mongoc_client_t *client;
   mongoc_uri_t *uri;
   bson_error_t error;
   bool ret;

   uri = mongoc_uri_new ("mongodb://localhost:27017");
   mongoc_uri_set_option_as_bool (uri, MONGOC_URI_LOADBALANCED, true);
   mongoc_uri_set_option_as_bool (uri, MONGOC_URI_DIRECTCONNECTION, true);
   client = mongoc_client_new_from_uri (uri);

   ret = mongoc_client_command_simple (client,
                                       "admin",
                                       tmp_bson ("{'ping': 1}"),
                                       NULL /* read prefs */,
                                       NULL /* reply */,
                                       &error);
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_SERVER_SELECTION,
                          MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                          "URI with \"loadBalanced\" enabled must not contain "
                          "option \"directConnection\" enabled");
   BSON_ASSERT (!ret);

   mongoc_uri_destroy (uri);
   mongoc_client_destroy (client);
}

/* Test basic connectivity to a load balanced cluster. */
static void
test_loadbalanced_connect_single (void *unused)
{
   mongoc_client_t *client;
   char *uristr = loadbalanced_uri ();
   bson_error_t error;
   bool ok;
   mongoc_server_description_t *monitor_sd;
   stats_t *stats;

   client = mongoc_client_new (uristr);
   stats = set_client_callbacks (client);
   ok = mongoc_client_command_simple (client,
                                      "admin",
                                      tmp_bson ("{'ping': 1}"),
                                      NULL /* read prefs */,
                                      NULL /* reply */,
                                      &error);
   ASSERT_OR_PRINT (ok, error);

   /* Ensure the server description is unchanged and remains as type LoadBalancer. */
   monitor_sd = mongoc_client_select_server (
      client, true /* for writes */, NULL /* read prefs */, &error);
   ASSERT_OR_PRINT (monitor_sd, error);
   ASSERT_CMPSTR ("LoadBalancer", mongoc_server_description_type (monitor_sd));

   mongoc_server_description_destroy (monitor_sd);
   bson_free (uristr);
   mongoc_client_destroy (client);
   free_and_assert_stats (stats);
}

static void
test_loadbalanced_connect_pooled (void *unused)
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   char *uristr;
   mongoc_uri_t *uri;
   bson_error_t error;
   bool ok;
   mongoc_server_description_t *monitor_sd;
   stats_t *stats;

   uristr = loadbalanced_uri ();
   uri = mongoc_uri_new (uristr);
   pool = mongoc_client_pool_new (uri);
   stats = set_client_pool_callbacks (pool);
   client = mongoc_client_pool_pop (pool);

   ok = mongoc_client_command_simple (client,
                                    "admin",
                                    tmp_bson ("{'ping': 1}"),
                                    NULL /* read prefs */,
                                    NULL /* reply */,
                                    &error);
   ASSERT_OR_PRINT (ok, error);

   /* Ensure the server description is unchanged and remains as type LoadBalancer. */
   monitor_sd = mongoc_client_select_server (
      client, true /* for writes */, NULL /* read prefs */, &error);
   ASSERT_OR_PRINT (monitor_sd, error);
   ASSERT_CMPSTR ("LoadBalancer", mongoc_server_description_type (monitor_sd));

   mongoc_server_description_destroy (monitor_sd);
   bson_free (uristr);
   mongoc_uri_destroy (uri);
   mongoc_client_pool_push (pool, client);
   mongoc_client_pool_destroy (pool);
   free_and_assert_stats (stats);   
}

/* Ensure that server selection on single threaded clients establishes a
 * connection against load balanced clusters. */
static void
test_loadbalanced_server_selection_establishes_connection_single (void *unused)
{
   mongoc_client_t *client;
   char *uristr = loadbalanced_uri ();
   bson_error_t error;
   mongoc_server_description_t *monitor_sd;
   mongoc_server_description_t *handshake_sd;

   client = mongoc_client_new (uristr);
   monitor_sd = mongoc_client_select_server (
      client, true /* for writes */, NULL /* read prefs */, &error);
   ASSERT_OR_PRINT (monitor_sd, error);
   ASSERT_CMPSTR ("LoadBalancer", mongoc_server_description_type (monitor_sd));

   /* Ensure that a connection has been established by getting the handshake's
    * server description. */
   handshake_sd = mongoc_client_get_handshake_description (
      client, monitor_sd->id, NULL /* opts */, &error);
   ASSERT_OR_PRINT (handshake_sd, error);
   ASSERT_CMPSTR ("Mongos", mongoc_server_description_type (handshake_sd));

   bson_free (uristr);
   mongoc_server_description_destroy (monitor_sd);
   mongoc_server_description_destroy (handshake_sd);
   mongoc_client_destroy (client);
}

/* Test that the 5 second cooldown does not apply when establishing a new connection to the load balancer after a network error. */
static void
test_loadbalanced_network_error_bypasses_cooldown_single (void *unused) {

}

static int
skip_if_not_loadbalanced (void)
{
   char *val = loadbalanced_uri ();
   if (!val) {
      return 0;
   }
   bson_free (val);
   return 1;
}

void
test_loadbalanced_install (TestSuite *suite)
{
   TestSuite_AddFull (suite,
                      "/loadbalanced/sessions/supported",
                      test_loadbalanced_sessions_supported,
                      NULL /* ctx */,
                      NULL /* dtor */,
                      skip_if_not_loadbalanced);
   TestSuite_AddFull (suite,
                      "/loadbalanced/sessions/do_not_expire",
                      test_loadbalanced_sessions_do_not_expire,
                      NULL /* ctx */,
                      NULL /* dtor */,
                      skip_if_not_loadbalanced);
   TestSuite_AddFull (suite,
                      "/loadbalanced/client_uri_validation",
                      test_loadbalanced_client_uri_validation,
                      NULL /* ctx */,
                      NULL /* dtor */,
                      NULL);

   TestSuite_AddFull (suite,
                      "/loadbalanced/connect/single",
                      test_loadbalanced_connect_single,
                      NULL /* ctx */,
                      NULL /* dtor */,
                      skip_if_not_loadbalanced);

   TestSuite_AddFull (suite,
                      "/loadbalanced/connect/pooled",
                      test_loadbalanced_connect_pooled,
                      NULL /* ctx */,
                      NULL /* dtor */,
                      skip_if_not_loadbalanced);

   TestSuite_AddFull (
      suite,
      "/loadbalanced/server_selection_establishes_connection/single",
      test_loadbalanced_server_selection_establishes_connection_single,
      NULL /* ctx */,
      NULL /* dtor */,
      skip_if_not_loadbalanced);
}
