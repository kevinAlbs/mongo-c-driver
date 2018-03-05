#include <mongoc.h>
#include <mongoc-socket-private.h>
#include <mongoc-host-list-private.h>
#include <mongoc-util-private.h>

#include "mongoc-client-private.h"

#include "TestSuite.h"
#include "mock_server/mock-server.h"

#define TIMEOUT 20000 /* milliseconds */

/* happy eyeballs (he) testing. */
typedef struct he_testcase_server {
   /* { "ipv4", "ipv6", NULL } */
   char *type;
   /* how long before the mock server calls `listen` on the server socket.
    * this delays the client from establishing a connection. */
   /* TODO: is this only for ipv6? */
   int listen_delay_ms;
   /* if true, this closes the server socket before the client establishes
    * connection. */
   bool close_before_connection;
} he_testcase_server_t;

typedef struct he_testcase_client {
   /* { "ipv4", "ipv6", "both" } */
   char *type;
   int64_t dns_cache_timeout_ms;
} he_testcase_client_t;

typedef struct he_testcase_expected {
   /* { "ipv4", "ipv6", "neither" }. which connection succeeds (if any). */
   char *conn_succeeds_to;
   /* how many async commands should be created at the start. */
   int initial_acmds;
   /* bounds for the server selection to finish. */
   int complete_after_ms; /* TODO: rename to complete_duration_min */
   int complete_before_ms;
} he_testcase_expected_t;

typedef struct he_testcase_state {
   mock_server_t *mock_servers[2];
   mongoc_host_list_t host;
   mongoc_topology_scanner_t *ts;
} he_testcase_state_t;

typedef struct he_testcase {
   he_testcase_client_t client;
   he_testcase_server_t servers[2];
   he_testcase_expected_t expected;
   he_testcase_state_t state;
} he_testcase_t;

static void
_test_scanner_callback (uint32_t id,
                        const bson_t *bson,
                        int64_t rtt_msec,
                        void *data,
                        const bson_error_t *error /* IN */)
{
   he_testcase_t *testcase = (he_testcase_t *) data;
   bool should_succeed =
      strcmp (testcase->expected.conn_succeeds_to, "neither");
   if (should_succeed) {
      ASSERT_OR_PRINT (!error->code, (*error));
   } else {
      ASSERT_ERROR_CONTAINS ((*error),
                             MONGOC_ERROR_STREAM,
                             MONGOC_ERROR_STREAM_CONNECT,
                             "connection refused");
   }
}

static void
_init_host (mongoc_host_list_t *host, uint16_t port, const char *type)
{
   char *host_str, *host_and_port;

   if (strcmp (type, "ipv4") == 0) {
      host_str = "127.0.0.1";
   } else if (strcmp (type, "ipv6") == 0) {
      host_str = "[::1]";
   } else {
      host_str = "localhost"; /* TODO: change to env var */
   }

   host_and_port = bson_strdup_printf ("%s:%hu", host_str, port);
   _mongoc_host_list_from_string (host, host_and_port);
   /* we should only have one host. */
   BSON_ASSERT (!host->next);
   bson_free (host_and_port);
}

static void
_testcase_setup (he_testcase_t *testcase)
{
   uint16_t port = 0;
   int i;

   for (i = 0; i < 2; i++) {
      he_testcase_server_t *server = testcase->servers + i;
      mock_server_t *mock_server = NULL;
      mock_server_bind_opts_t opts = {0};


      opts.listen_delay_ms = server->listen_delay_ms;
      opts.close_before_connection = server->close_before_connection;

      if (!server->type) {
         continue;
      }

      if (strcmp (server->type, "ipv4") == 0) {
         struct sockaddr_in ipv4_addr = {0};

         ipv4_addr.sin_family = AF_INET;
         /* TODO: explain how port saving works. */
         ipv4_addr.sin_port = htons (port);
         inet_pton (AF_INET, "127.0.0.1", &ipv4_addr.sin_addr);
         opts.bind_addr = &ipv4_addr;
         opts.bind_addr_len = sizeof (ipv4_addr);
         opts.family = AF_INET;
         opts.ipv6_only = false;

         mock_server = mock_server_with_autoismaster (WIRE_VERSION_MAX);
         mock_server_set_bind_opts (mock_server, &opts);
         mock_server_run (mock_server);
         port = mock_server_get_port (mock_server);
         testcase->state.mock_servers[i] = mock_server;
      }

      if (strcmp (server->type, "ipv6") == 0) {
         struct sockaddr_in6 ipv6_addr = {0};

         ipv6_addr.sin6_family = AF_INET6;
         /* use the same port of the ipv4 server (if one was started). */
         ipv6_addr.sin6_port = htons (port);
         inet_pton (AF_INET6, "::1", &ipv6_addr.sin6_addr);
         opts.bind_addr = (struct sockaddr_in *) &ipv6_addr;
         opts.bind_addr_len = sizeof (ipv6_addr);
         opts.family = AF_INET6;
         opts.ipv6_only = true;

         mock_server = mock_server_with_autoismaster (WIRE_VERSION_MAX);
         mock_server_set_bind_opts (mock_server, &opts);
         mock_server_run (mock_server);
         port = mock_server_get_port (mock_server);
         testcase->state.mock_servers[i] = mock_server;
      }
   }

   _init_host (&testcase->state.host, port, testcase->client.type);

   testcase->state.ts = mongoc_topology_scanner_new (
      NULL, NULL, &_test_scanner_callback, testcase, TIMEOUT);

   if (testcase->client.dns_cache_timeout_ms > 0) {
      _mongoc_topology_scanner_set_dns_cache_timeout (
         testcase->state.ts, testcase->client.dns_cache_timeout_ms);
   }
}

static void
_testcase_teardown (he_testcase_t *testcase)
{
   int i;
   for (i = 0; i < 2; i++) {
      if (testcase->state.mock_servers[i]) {
         mock_server_destroy (testcase->state.mock_servers[i]);
      }
   }
   mongoc_topology_scanner_destroy (testcase->state.ts);
}

static void
_check_stream (mongoc_stream_t *stream, const char *expected, char *message)
{
   /* check the socket that the scanner found. */
   char *actual = "neither";
   if (stream) {
      mongoc_socket_t *sock =
         mongoc_stream_socket_get_socket ((mongoc_stream_socket_t *) stream);
      actual = (sock->domain == AF_INET) ? "ipv4" : "ipv6";
   }

   if (strcmp (expected, actual) != 0) {
      fprintf (stderr,
               "%s: expected %s stream but got %s stream\n",
               message,
               expected,
               actual);
      ASSERT (false);
   }
}

static void
_testcase_run (he_testcase_t *testcase)
{
   /* construct mock servers. */
   mongoc_topology_scanner_t *ts = testcase->state.ts;
   mongoc_topology_scanner_node_t *node;
   he_testcase_expected_t *expected = &testcase->expected;
   uint64_t start, duration_ms;

   start = bson_get_monotonic_time ();

   mongoc_topology_scanner_add (ts, &testcase->state.host, 1 /* any server id is ok. */);
   mongoc_topology_scanner_scan (ts, 1);
   /* how many commands should we have initially? */
   ASSERT_CMPINT ((int) (ts->async->ncmds), ==, expected->initial_acmds);

   mongoc_topology_scanner_work (ts);

   duration_ms = (bson_get_monotonic_time () - start) / (1000);
   if (duration_ms > expected->complete_before_ms) {
      /* TODO: ASSERT_WITH_MSG */
      /* TODO: check for duration macros (ASSERT_CMPTIME), maybe add one. Or use MONGOC_ERROR */
      fprintf (stderr,
               "test completed in %dms but should have completed before %dms\n",
               (int) duration_ms,
               expected->complete_before_ms);
      ASSERT (false);
   } else if (duration_ms < expected->complete_after_ms) {
      fprintf (stderr,
               "test completed in %dms but should have completed after %dms\n",
               (int) duration_ms,
               expected->complete_after_ms);
      ASSERT (false);
   }

   node = mongoc_topology_scanner_get_node (ts, 1);
   _check_stream (node->stream,
                  expected->conn_succeeds_to,
                  "checking client's final connection");
}

// #define CLIENT (client) { #client }
static void
test_happy_eyeballs ()
{
   const int he =
      250; /* delay before starting second connection if first does not
              complete. */
   const int e = 100; /* epsilon. wiggle room for time constraints. */
   int i, ntests;

   /* TODO: define SUCCEED=false, HANGUP=true */
   he_testcase_t testcases[] = {
      /* client ipv4. */
      {
         /* client */
         {"ipv4"},
         /* server sockets. */
         {{"ipv4", 0, false}}, /* TODO: confirm rest of struct is zeroed out always */
         /* expected. */
         {"ipv4", 1, 0, e},
      },
      {
         /* client. */
         {"ipv4"},
         /* server sockets. */
         {{"ipv6", 0, false}},
         /* expected. */
         {"neither", 1, 0, e},
      },
      {
         /* client. */
         {"ipv4"},
         /* server sockets. */
         {{"ipv4", 0, false}, {"ipv6", 0, false}},
         /* expected. */
         {"ipv4", 1, 0, e},
      },
      {
         /* client. */
         {"ipv4"},
         /* server sockets. */
         {{"ipv4", 0, true}, {"ipv6", 0, false}},
         /* expected. */
         {"neither", 1, 0, e},
      },
      /* client ipv6. */
      {
         /* client. */
         {"ipv6"},
         /* server sockets. */
         {{"ipv4", 0, false}},
         /* expected. */
         {"neither", 1, 0, e},
      },
      {
         /* client. */
         {"ipv6"},
         /* server sockets. */
         {{"ipv6", 0, false}},
         /* expected. */
         {"ipv6", 1, 0, e},
      },
      {
         /* client. */
         {"ipv6"},
         /* server sockets. */
         {{"ipv4", 0, false}, {"ipv6", 0, false}},
         /* expected. */
         {"ipv6", 1, 0, e},
      },
      {
         /* client. */
         {"ipv6"},
         /* server sockets. */
         {{"ipv4", 0, false}, {"ipv6", 0, true}},
         /* expected. */
         {"neither", 1, 0, e},
      },
      /* client both ipv4 and ipv6. */
      {
         /* client. */
         {"both"},
         /* server sockets. */
         {{"ipv4", 0, false}},
         /* expected. no delay, ipv6 fails immediately and ipv4 succeeds. */
         {"ipv4", 2, 0, e},
      },
      {
         /* client. */
         {"both"},
         /* server sockets. */
         {{"ipv6", 0, false}},
         /* expected. no delay, ipv6 succeeds immediately. */
         {"ipv6", 2, 0, e},
      },
      /* when both client is connecting to both ipv4 and ipv6 and server is
       * listening on both ipv4 and ipv6, test delaying the connections at
       * various times. */
      /* ipv6 {succeeds, fails} before ipv4 starts and {succeeds, fails} */
      {
         /* client. */
         {"both"},
         /* server sockets. */
         {{"ipv4", 0, false}, {"ipv6", 0, false}},
         /* expected. no delay, ipv6 succeeds immediately. */
         {"ipv6", 2, 0, e},
      },
      {
         /* client. */
         {"both"},
         /* server sockets. */
         {{"ipv4", 0, false}, {"ipv6", 0, true}},
         /* expected. no delay, ipv6 fails immediately and ipv4 succeeds. */
         {"ipv4", 2, 0, e},
      },
      {
         /* client. */
         {"both"},
         /* server sockets. */
         {{"ipv4", 0, true}, {"ipv6", 0, true}},
         /* expected. */
         {"neither", 2, 0, e},
      },
      /* ipv6 {succeeds, fails} after ipv4 starts but before ipv4 {succeeds,
         fails} */
      {
         /* client. */
         {"both"},
         /* server sockets. */
         {{"ipv4", 2 * he, false}, {"ipv6", he, false}},
         /* expected. */
         {"ipv6", 2, he, he + e},
      },
      {
         /* client. */
         {"both"},
         /* server sockets. */
         {{"ipv4", 2 * he, false}, {"ipv6", he, true}},
         /* expected. */
         {"ipv4", 2, 2 * he, 2 * he + e},
      },
      {
         /* client. */
         {"both"},
         /* server sockets. */
         {{"ipv4", 2 * he, true}, {"ipv6", he, true}},
         /* expected. */
         {"neither", 2, 2 * he, 2 * he + e},
      },
      /* ipv4 {succeeds,fails} after ipv6 {succeeds, fails}. */
      {
         /* client. */
         {"both"},
         /* server sockets. */
         {{"ipv4", 0, false}, {"ipv6", he + e, false}},
         /* expected. ipv6 is delayed too long, ipv4 succeeds. */
         {"ipv4", 2, he, he + e},
      },
      {
         /* client. */
         {"both"},
         /* server sockets. */
         {{"ipv4", 0, true}, {"ipv6", he + e, false}},
         /* expected. ipv6 is delayed, but ipv4 fails. */
         {"ipv6", 2, he + e, he + 2 * e},
      },
      {
         /* client. */
         {"both"},
         /* server sockets. */
         {{"ipv4", 0, true}, {"ipv6", he + e, true}},
         /* expected. both fail. */
         {"neither", 2, he + e, he + 2 * e},
      },
   };

   ntests = sizeof (testcases) / sizeof (he_testcase_t);

   for (i = 0; i < ntests; i++) {
      _testcase_setup (testcases + i);
      _testcase_run (testcases + i);
      _testcase_teardown (testcases + i);
   }
}

static void
test_happy_eyeballs_dns_cache ()
{
   const int he = 250;
   const int e = 100;
   he_testcase_t testcase = {
      /* client. */
      {"both"},
      /* server sockets. */
      {{"ipv4", 0, false}, {"ipv6", he + e, false}},
      /* expected. IPv4 succeeds after delay. */
      {"ipv4", 2, he, he + e},
   };
   _testcase_setup (&testcase);
   _testcase_run (&testcase);
   /* disconnect the node so we perform another DNS lookup. */
   mongoc_topology_scanner_node_disconnect (testcase.state.ts->nodes, false);

   /* after running once, the topology scanner should have cached the DNS
    * result for IPv4. It should complete immediately. */
   testcase.expected.initial_acmds = 1;
   testcase.expected.complete_after_ms = 0;
   testcase.expected.complete_before_ms = e;
   _testcase_run (&testcase);
   _testcase_teardown (&testcase);
}

static void
test_happy_eyeballs_dns_cache_timeout ()
{
   const int he = 250;
   const int e = 100;
   he_testcase_t testcase = {
      /* client. */
      {"both", 100 /* dns cache timeout */},
      /* server sockets. */
      {{"ipv4", 0, false}, {"ipv6", he + e, false}},
      /* expected. IPv4 succeeds after delay. */
      {"ipv4", 2, he, he + e},
   };
   _testcase_setup (&testcase);
   _testcase_run (&testcase);
   /* disconnect the node so we perform another DNS lookup. */
   mongoc_topology_scanner_node_disconnect (testcase.state.ts->nodes, false);

   /* after running once, the topology scanner should have cached the DNS
    * result for IPv4. Wait for 100ms for cache to expire. */
   _mongoc_usleep (110 * 1000);

   /* since the cache is expired, we try connecting to both again. There's no
    * longer a delay applied to the IPv6 connection so it succeeds. */
   testcase.expected.initial_acmds = 2;
   testcase.expected.conn_succeeds_to = "ipv6";
   testcase.expected.complete_after_ms = 0;
   testcase.expected.complete_before_ms = e;
   _testcase_run (&testcase);
   _testcase_teardown (&testcase);
}

/* TODO: audit these test cases, think of other ways to inspect state.
 */

void
test_happy_eyeballs_install (TestSuite *suite)
{
   TestSuite_AddMockServerTest (
      suite, "/TOPOLOGY/happy_eyeballs/", test_happy_eyeballs);
   TestSuite_AddMockServerTest (suite,
                                "/TOPOLOGY/happy_eyeballs/dns_cache/",
                                test_happy_eyeballs_dns_cache);
   TestSuite_AddMockServerTest (suite,
                                "/TOPOLOGY/happy_eyeballs/dns_cache/timeout",
                                test_happy_eyeballs_dns_cache_timeout);
}