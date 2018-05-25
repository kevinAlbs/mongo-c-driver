#include <mongoc.h>

#include "mongoc-crypto-private.h"
#include "mongoc-scram-private.h"

#include "TestSuite.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"

#ifdef MONGOC_ENABLE_SSL
static void
test_mongoc_scram_step_username_not_set (void)
{
   mongoc_scram_t scram;
   bool success;
   uint8_t buf[4096] = {0};
   uint32_t buflen = 0;
   bson_error_t error;

   _mongoc_scram_init (&scram, MONGOC_CRYPTO_ALGORITHM_SHA_1);
   _mongoc_scram_set_pass (&scram, "password");

   success = _mongoc_scram_step (
      &scram, buf, buflen, buf, sizeof buf, &buflen, &error);

   ASSERT (!success);
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_SCRAM,
                          MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                          "SCRAM Failure: username is not set");

   _mongoc_scram_destroy (&scram);
}

typedef struct {
   const char *original;
   const char *normalized;
   bool should_be_required;
   bool should_succeed;
} sasl_prep_testcase_t;


/* test that an error is reported if the server responds with an iteration
 * count that is less than 4096 */
static void
test_iteration_count (int count, bool should_succeed)
{
   mongoc_scram_t scram;
   uint8_t buf[4096] = {0};
   uint32_t buflen = 0;
   bson_error_t error;
   const char *client_nonce = "YWJjZA==";
   char *server_response;
   bool success;

   server_response = bson_strdup_printf (
      "r=YWJjZA==YWJjZA==,s=r6+P1iLmSJvhrRyuFi6Wsg==,i=%d", count);
   /* set up the scram state to immediately test step 2. */
   _mongoc_scram_init (&scram, MONGOC_CRYPTO_ALGORITHM_SHA_1);
   _mongoc_scram_set_pass (&scram, "password");
   memcpy (scram.encoded_nonce, client_nonce, sizeof (scram.encoded_nonce));
   scram.encoded_nonce_len = (int32_t) strlen (client_nonce);
   scram.auth_message = bson_malloc0 (4096);
   scram.auth_messagemax = 4096;
   /* prepare the server's "response" from step 1 as the input for step 2. */
   memcpy (buf, server_response, strlen (server_response) + 1);
   buflen = (int32_t) strlen (server_response);
   scram.step = 1;
   success = _mongoc_scram_step (
      &scram, buf, buflen, buf, sizeof buf, &buflen, &error);
   if (should_succeed) {
      ASSERT_OR_PRINT (success, error);
   } else {
      BSON_ASSERT (!success);
      ASSERT_ERROR_CONTAINS (error,
                             MONGOC_ERROR_SCRAM,
                             MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                             "SCRAM Failure: iterations must be at least 4096");
   }
   bson_free (server_response);
   _mongoc_scram_destroy (&scram);
}

static void
test_mongoc_scram_iteration_count (void)
{
   test_iteration_count (1000, false);
   test_iteration_count (4095, false);
   test_iteration_count (4096, true);
   test_iteration_count (10000, true);
}

static void
test_mongoc_scram_sasl_prep (void)
{
#ifdef MONGOC_ENABLE_ICU
   int i, ntests;
   char *normalized;
   bson_error_t err;
   /* examples from RFC 4013 section 3. */
   sasl_prep_testcase_t tests[] = {{"\x65\xCC\x81", "\xC3\xA9", true, true},
                                   {"I\xC2\xADX", "IX", true, true},
                                   {"user", "user", false, true},
                                   {"USER", "USER", false, true},
                                   {"\xC2\xAA", "a", true, true},
                                   {"\xE2\x85\xA8", "IX", true, true},
                                   {"\x07", "(invalid)", true, false},
                                   {"\xD8\xA7\x31", "(invalid)", true, false}};
   ntests = sizeof (tests) / sizeof (sasl_prep_testcase_t);
   for (i = 0; i < ntests; i++) {
      ASSERT_CMPINT (tests[i].should_be_required,
                     ==,
                     _mongoc_sasl_prep_required (tests[i].original));
      memset (&err, 0, sizeof (err));
      normalized = _mongoc_sasl_prep (
         tests[i].original, strlen (tests[i].original), &err);
      if (tests[i].should_succeed) {
         ASSERT_CMPSTR (tests[i].normalized, normalized);
         ASSERT_CMPINT (err.code, ==, 0);
         bson_free (normalized);
      } else {
         ASSERT_CMPINT (err.code, ==, MONGOC_ERROR_SCRAM_PROTOCOL_ERROR);
         ASSERT_CMPINT (err.domain, ==, MONGOC_ERROR_SCRAM);
         BSON_ASSERT (normalized == NULL);
      }
   }
#endif
}
#endif

static bool
skip_if_scram_auth_not_enabled (void)
{
   return true; /* TODO */
}

static void
_create_scram_users (void)
{
   mongoc_client_t *client;
   bool res;
   bson_error_t error;
   client = test_framework_client_new ();
   res = mongoc_client_command_simple (
      client,
      "admin",
      tmp_bson ("{'createUser': 'sha1', 'pwd': 'sha1', 'roles': ['root'], "
                "'mechanisms': ['SCRAM-SHA-1']}"),
      NULL /* read_prefs */,
      NULL /* reply */,
      &error);
   ASSERT_OR_PRINT (res, error);
   res = mongoc_client_command_simple (
      client,
      "admin",
      tmp_bson ("{'createUser': 'sha256', 'pwd': 'sha256', 'roles': ['root'], "
                "'mechanisms': ['SCRAM-SHA-256']}"),
      NULL /* read_prefs */,
      NULL /* reply */,
      &error);
   ASSERT_OR_PRINT (res, error);
   res = mongoc_client_command_simple (
      client,
      "admin",
      tmp_bson ("{'createUser': 'both', 'pwd': 'both', 'roles': ['root'], "
                "'mechanisms': ['SCRAM-SHA-1', 'SCRAM-SHA-256']}"),
      NULL /* read_prefs */,
      NULL /* reply */,
      &error);
   ASSERT_OR_PRINT (res, error);
   mongoc_client_destroy (client);
}

static void
_drop_scram_users (void)
{
   mongoc_client_t *client;
   mongoc_database_t *db;
   bool res;
   bson_error_t error;
   client = test_framework_client_new ();
   db = mongoc_client_get_database (client, "admin");
   res = mongoc_database_remove_user (db, "sha1", &error);
   ASSERT_OR_PRINT (res, error);
   res = mongoc_database_remove_user (db, "sha256", &error);
   ASSERT_OR_PRINT (res, error);
   res = mongoc_database_remove_user (db, "both", &error);
   ASSERT_OR_PRINT (res, error);
   mongoc_client_destroy (client);
}

typedef struct {
   bool attempted_auth;
   char mechanism_used[64];
} scram_ctx_t;

static void
_cmd_started_scram_cb (const mongoc_apm_command_started_t *event)
{
   const char *cmd_name;
   const char *mechanism_used;
   scram_ctx_t *scram_ctx;
   const bson_t *cmd;
   cmd_name = mongoc_apm_command_started_get_command_name (event);
   cmd = mongoc_apm_command_started_get_command (event);
   printf ("%s\n", bson_as_json (cmd, NULL));
   if (strcmp (cmd_name, "saslStart") != 0) {
      return;
   }
   cmd = mongoc_apm_command_started_get_command (event);
   printf ("%s\n", bson_as_json (cmd, NULL));
   scram_ctx = (scram_ctx_t *) mongoc_apm_command_started_get_context (event);
   scram_ctx->attempted_auth = true;

   mechanism_used = bson_lookup_utf8 (cmd, "mechanism");
   memcpy (
      scram_ctx->mechanism_used, mechanism_used, strlen (mechanism_used) + 1);
}

static void
_try_auth (const char *user,
           const char *pwd,
           const char *mechanism_expected,
           bool should_succeed)
{
   mongoc_uri_t *uri;
   mongoc_client_t *client;
   bson_error_t error;
   bson_t reply;
   bool res;
   mongoc_apm_callbacks_t *callbacks;
   scram_ctx_t scram_ctx = {0};

   uri = test_framework_get_uri ();
   mongoc_uri_set_username (uri, user);
   mongoc_uri_set_password (uri, pwd);
   client = mongoc_client_new_from_uri (uri);
   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_started_cb (callbacks, _cmd_started_scram_cb);
   mongoc_client_set_apm_callbacks (client, callbacks, &scram_ctx);
   mongoc_apm_callbacks_destroy (callbacks);
   mongoc_client_set_error_api (client, 2);
   res = mongoc_client_command_simple (client,
                                       "admin",
                                       tmp_bson ("{'dbstats': 1}"),
                                       NULL /* read_prefs. */,
                                       &reply,
                                       &error);
   if (should_succeed) {
      ASSERT_OR_PRINT (res, error);
      ASSERT_MATCH (&reply, "{'db': 'admin', 'ok': 1}");
      ASSERT_CMPSTR (scram_ctx.mechanism_used, mechanism_expected);
   } else {
      ASSERT (!res);
      ASSERT_ERROR_CONTAINS (error,
                             MONGOC_ERROR_CLIENT,
                             MONGOC_ERROR_CLIENT_AUTHENTICATE,
                             "Authentication failed");
   }
   mongoc_uri_destroy (uri);
   mongoc_client_destroy (client);
}

static void
test_mongoc_scram_auth (void *ctx)
{
   _create_scram_users ();
   _try_auth ("sha1", "sha1", "SCRAM-SHA-1", true);
   _drop_scram_users ();
}

void
test_scram_install (TestSuite *suite)
{
#ifdef MONGOC_ENABLE_SSL
   TestSuite_Add (suite,
                  "/scram/username_not_set",
                  test_mongoc_scram_step_username_not_set);
   TestSuite_Add (suite, "/scram/sasl_prep", test_mongoc_scram_sasl_prep);
   TestSuite_Add (
      suite, "/scram/iteration_count", test_mongoc_scram_iteration_count);
   TestSuite_AddFull (suite,
                      "/scram/auth_tests",
                      test_mongoc_scram_auth,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      skip_if_scram_auth_not_enabled,
                      TestSuite_CheckLive);
#endif
}
