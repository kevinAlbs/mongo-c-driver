#include <mongoc.h>

#include "mongoc-crypto-private.h"
#include "mongoc-scram-private.h"

#include "TestSuite.h"

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
#endif
}
