#include <mongoc.h>
#include <mongoc-socket-private.h>
#include <mongoc-host-list-private.h>
#include <mongoc-util-private.h>

#include "mongoc-client-private.h"

#include "TestSuite.h"
#include "mock_server/mock-server.h"
#include "test-libmongoc.h"

#define TIMEOUT 20000 /* milliseconds */

/* happy eyeballs (he) testing. */
typedef struct he_testcase_server {
   /* { "ipv4", "ipv6", NULL } */
   char *type;
   /* if true, this closes the server socket before the client establishes
    * connection. */
   bool close_before_connection;
   /* how long before the mock server calls `listen` on the server socket.
    * this delays the client from establishing a connection. */
   int listen_delay_ms;
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
   int duration_min_ms;
   int duration_max_ms;
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
   int should_succeed = strcmp (testcase->expected.conn_succeeds_to, "neither");
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
   bool free_host_str = false;

   if (strcmp (type, "ipv4") == 0) {
      host_str = "127.0.0.1";
   } else if (strcmp (type, "ipv6") == 0) {
      host_str = "[::1]";
   } else {
      host_str = test_framework_getenv ("MONGOC_TEST_IPV4_AND_IPV6_HOST");
      if (host_str) {
         free_host_str = true;
      } else {
         /* default to localhost. */
         host_str = "localhost";
      }
   }

   host_and_port = bson_strdup_printf ("%s:%hu", host_str, port);
   _mongoc_host_list_from_string (host, host_and_port);
   if (free_host_str) {
      bson_free (host_str);
   }
   /* we should only have one host. */
   BSON_ASSERT (!host->next);
   bson_free (host_and_port);
}

static void
_testcase_setup (he_testcase_t *testcase)
{
   /* port is initially zero. the first mock server uses any available port.
    * if there is a second mock server needed by the testcase, it will bind
    * to the same port (on a different family). */
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

   ASSERT_WITH_MSG (strcmp (expected, actual) == 0,
                    "%s: expected %s stream but got %s stream\n",
                    message,
                    expected,
                    actual);
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

   mongoc_topology_scanner_add (
      ts, &testcase->state.host, 1 /* any server id is ok. */);
   mongoc_topology_scanner_scan (ts, 1);
   /* how many commands should we have initially? */
   ASSERT_CMPINT ((int) (ts->async->ncmds), ==, expected->initial_acmds);

   mongoc_topology_scanner_work (ts);

   duration_ms = (bson_get_monotonic_time () - start) / (1000);
   ASSERT_WITHIN_TIME_INTERVAL ((int) duration_ms,
                                (int) expected->duration_min_ms,
                                (int) expected->duration_max_ms);

   node = mongoc_topology_scanner_get_node (ts, 1);
   _check_stream (node->stream,
                  expected->conn_succeeds_to,
                  "checking client's final connection");
}

#define CLIENT(client) \
   {                   \
      #client          \
   }

#define CLIENT_WITH_DNS_CACHE_TIMEOUT(type, timeout) \
   {                                                 \
      #type, timeout                                 \
   }
#define HANGUP true
#define LISTEN false
#define SERVER(type, hangup) \
   {                         \
      #type, hangup          \
   }
#define DELAYED_SERVER(type, hangup, delay) \
   {                                        \
      #type, hangup, delay                  \
   }
#define SERVERS(...) \
   {                 \
      __VA_ARGS__    \
   }
#define DELAY_MS(val) val
#define DURATION_MS(min, max) (min), (max)
#define EXPECT(type, num_acmds, duration) \
   {                                      \
      #type, num_acmds, duration          \
   }
#define NCMDS(n) (n)

static void
test_happy_eyeballs (void)
{
   const int e = 100; /* epsilon. wiggle room for time constraints. */
   int i, ntests;

   he_testcase_t testcases[] = {
      /* client ipv4. */
      {
         CLIENT (ipv4),
         SERVERS (SERVER (ipv4, LISTEN)),
         EXPECT (ipv4, NCMDS (1), DURATION_MS (0, e)),
      },
      {
         CLIENT (ipv4),
         SERVERS (SERVER (ipv6, LISTEN)),
         EXPECT (neither, NCMDS (1), DURATION_MS (0, e)),
      },
      {CLIENT (ipv4),
       SERVERS (SERVER (ipv4, LISTEN), SERVER (ipv6, HANGUP)),
       EXPECT (ipv4, NCMDS (1), DURATION_MS (0, e))},
      {
         CLIENT (ipv4),
         SERVERS (SERVER (ipv4, HANGUP), SERVER (ipv6, HANGUP)),
         EXPECT (neither, NCMDS (1), DURATION_MS (0, e)),
      },
      /* client ipv6. */
      {
         CLIENT (ipv6),
         SERVERS (SERVER (ipv4, LISTEN)),
         EXPECT (neither, NCMDS (1), DURATION_MS (0, e)),
      },
      {
         CLIENT (ipv6),
         SERVERS (SERVER (ipv6, LISTEN)),
         EXPECT (ipv6, NCMDS (1), DURATION_MS (0, e)),
      },
      {
         CLIENT (ipv6),
         SERVERS (SERVER (ipv4, LISTEN), SERVER (ipv6, LISTEN)),
         EXPECT (ipv6, NCMDS (1), DURATION_MS (0, e)),
      },
      {
         CLIENT (ipv6),
         SERVERS (SERVER (ipv4, LISTEN), SERVER (ipv6, HANGUP)),
         EXPECT (neither, NCMDS (1), DURATION_MS (0, e)),
      },
      /* client both ipv4 and ipv6. */
      {
         CLIENT (both),
         SERVERS (SERVER (ipv4, LISTEN)),
         /* no delay, ipv6 fails immediately and ipv4 succeeds. */
         EXPECT (ipv4, NCMDS (2), DURATION_MS (0, e)),
      },
      {
         CLIENT (both),
         SERVERS (SERVER (ipv6, LISTEN)),
         /* no delay, ipv6 succeeds immediately. */
         EXPECT (ipv6, NCMDS (2), DURATION_MS (0, e)),
      },
      {
         CLIENT (both),
         SERVERS (SERVER (ipv4, LISTEN), SERVER (ipv6, LISTEN)),
         /* no delay, ipv6 succeeds immediately. */
         EXPECT (ipv6, NCMDS (2), DURATION_MS (0, e)),
      },
      {
         CLIENT (both),
         SERVERS (SERVER (ipv4, LISTEN), SERVER (ipv6, HANGUP)),
         /* no delay, ipv6 fails immediately and ipv4 succeeds. */
         EXPECT (ipv4, NCMDS (2), DURATION_MS (0, e)),
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
test_happy_eyeballs_with_delays (void)
{
   const int he =
      250; /* delay before starting second connection if first does not
              complete. */
   const int e = 100; /* epsilon. wiggle room for time constraints. */
   int i, ntests;
   he_testcase_t testcases[] = {
      /* when both client is connecting to both ipv4 and ipv6 and server is
       * listening on both ipv4 and ipv6, test delaying the connections at
       * various times. */
      /* ipv6 {succeeds, fails} before ipv4 starts and {succeeds, fails} */

      {CLIENT (both),
       SERVERS (SERVER (ipv4, HANGUP), SERVER (ipv6, HANGUP)),
       EXPECT (neither, NCMDS (2), DURATION_MS (0, e))},
      /* ipv6 {succeeds, fails} after ipv4 starts but before ipv4 {succeeds,
         fails} */
      {
         CLIENT (both),
         SERVERS (DELAYED_SERVER (ipv4, LISTEN, DELAY_MS (2 * he)),
                  DELAYED_SERVER (ipv6, LISTEN, he)),
         EXPECT (ipv6, NCMDS (2), DURATION_MS (he, he + e)),
      },
      {
         CLIENT (both),
         SERVERS (DELAYED_SERVER (ipv4, LISTEN, DELAY_MS (2 * he)),
                  DELAYED_SERVER (ipv6, HANGUP, DELAY_MS (he))),
         EXPECT (ipv4, NCMDS (2), DURATION_MS (2 * he, 2 * he + e)),
      },
      {
         CLIENT (both),
         SERVERS (DELAYED_SERVER (ipv4, HANGUP, DELAY_MS (2 * he)),
                  DELAYED_SERVER (ipv6, HANGUP, DELAY_MS (he))),
         EXPECT (neither, NCMDS (2), DURATION_MS (2 * he, 2 * he + e)),
      },
      /* ipv4 {succeeds,fails} after ipv6 {succeeds, fails}. */
      {
         CLIENT (both),
         SERVERS (SERVER (ipv4, LISTEN),
                  DELAYED_SERVER (ipv6, LISTEN, DELAY_MS (he + e))),
         /* ipv6 is delayed too long, ipv4 succeeds. */
         EXPECT (ipv4, NCMDS (2), DURATION_MS (he, he + e)),
      },
      {
         CLIENT (both),
         SERVERS (SERVER (ipv4, HANGUP),
                  DELAYED_SERVER (ipv6, LISTEN, DELAY_MS (he + e))),
         /* ipv6 is delayed, but ipv4 fails. */
         EXPECT (ipv6, NCMDS (2), DURATION_MS (he + e, he + 2 * e)),
      },
      {
         CLIENT (both),
         SERVERS (SERVER (ipv4, HANGUP),
                  DELAYED_SERVER (ipv6, HANGUP, DELAY_MS (he + e))),
         EXPECT (neither, NCMDS (2), DURATION_MS (he + e, he + 2 * e)),
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
test_happy_eyeballs_dns_cache (void)
{
   const int e = 100;
   he_testcase_t testcase = {
      CLIENT_WITH_DNS_CACHE_TIMEOUT (both, 300),
      SERVERS (SERVER (ipv4, LISTEN), SERVER (ipv6, LISTEN)),
      EXPECT (ipv6, NCMDS (2), DURATION_MS (0, e)),
   };
   _testcase_setup (&testcase);
   _testcase_run (&testcase);
   /* disconnect the node so we perform another DNS lookup. */
   mongoc_topology_scanner_node_disconnect (testcase.state.ts->nodes, false);

   /* after running once, the topology scanner should have cached the DNS
    * result for IPv6. It should complete immediately. */
   testcase.expected.initial_acmds = 1;
   _testcase_run (&testcase);

   /* disconnect the node so we perform another DNS lookup. */
   mongoc_topology_scanner_node_disconnect (testcase.state.ts->nodes, false);

   /* wait for DNS cache to expire. */
   _mongoc_usleep (310 * 1000);

   /* after running once, the topology scanner should have cached the DNS
    * result for IPv6. It should complete immediately. */
   testcase.expected.initial_acmds = 2;
   _testcase_run (&testcase);

   _testcase_teardown (&testcase);
}

void
test_happy_eyeballs_install (TestSuite *suite)
{
   TestSuite_AddMockServerTest (
      suite, "/TOPOLOGY/happy_eyeballs/", test_happy_eyeballs);
   /* TODO CDRIVER-2534: run delay tests on platforms other than macOS. */
   TestSuite_AddMockServerTest (suite,
                                "/TOPOLOGY/happy_eyeballs/with_delays",
                                test_happy_eyeballs_with_delays,
                                test_framework_skip_if_not_apple);
   TestSuite_AddMockServerTest (suite,
                                "/TOPOLOGY/happy_eyeballs/dns_cache/",
                                test_happy_eyeballs_dns_cache);
}