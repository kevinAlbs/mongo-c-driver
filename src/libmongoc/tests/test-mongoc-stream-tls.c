#include <mongoc/mongoc.h>


#ifdef MONGOC_ENABLE_SSL_OPENSSL
#include <openssl/err.h>
#endif

#include "ssl-test.h"
#include "TestSuite.h"
#include "test-libmongoc.h"
#include "test-conveniences.h"

#if defined(MONGOC_ENABLE_SSL_SECURE_CHANNEL)
#include <mongoc/mongoc-stream-tls-secure-channel-private.h>
#include <mongoc/mongoc-host-list-private.h> // _mongoc_host_list_from_string_with_err
#endif

#if !defined(MONGOC_ENABLE_SSL_SECURE_CHANNEL)

static void
test_mongoc_tls_no_certs (void)
{
   mongoc_ssl_opt_t sopt = {0};
   mongoc_ssl_opt_t copt = {0};
   ssl_test_result_t sr;
   ssl_test_result_t cr;

   capture_logs (true);
   /* No server cert is not valid TLS at all */
   ssl_test (&copt, &sopt, "doesnt_matter", &cr, &sr);

   ASSERT_CMPINT (cr.result, !=, SSL_TEST_SUCCESS);
   ASSERT_CMPINT (sr.result, !=, SSL_TEST_SUCCESS);
}


#ifdef MONGOC_ENABLE_SSL_OPENSSL
static void
test_mongoc_tls_password (void)
{
   mongoc_ssl_opt_t sopt = {0};
   mongoc_ssl_opt_t copt = {0};
   ssl_test_result_t sr;
   ssl_test_result_t cr;

   sopt.ca_file = CERT_CA;
   sopt.pem_file = CERT_SERVER;

   copt.ca_file = CERT_CA;
   copt.pem_file = CERT_PASSWORD_PROTECTED;
   copt.pem_pwd = CERT_PASSWORD;

   /* Password protected key */
   ssl_test (&copt, &sopt, "localhost", &cr, &sr);
   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SUCCESS);
   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SUCCESS);
}

static void
test_mongoc_tls_bad_password (void)
{
   mongoc_ssl_opt_t sopt = {0};
   mongoc_ssl_opt_t copt = {0};
   ssl_test_result_t sr;
   ssl_test_result_t cr;

   sopt.ca_file = CERT_CA;
   sopt.pem_file = CERT_SERVER;

   copt.ca_file = CERT_CA;
   copt.pem_file = CERT_PASSWORD_PROTECTED;
   copt.pem_pwd = "incorrect password";


   capture_logs (true);
   /* Incorrect password cannot unlock the key */
   ssl_test (&copt, &sopt, "localhost", &cr, &sr);

   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SSL_HANDSHAKE);
   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SSL_INIT);

   /* Change it to the right password */
   copt.pem_pwd = CERT_PASSWORD;
   ssl_test (&copt, &sopt, "localhost", &cr, &sr);

   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SUCCESS);
   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SUCCESS);
}
#endif


static void
test_mongoc_tls_no_verify (void)
{
   mongoc_ssl_opt_t sopt = {0};
   mongoc_ssl_opt_t copt = {0};
   ssl_test_result_t sr;
   ssl_test_result_t cr;

   sopt.ca_file = CERT_CA;
   sopt.pem_file = CERT_SERVER;

   copt.ca_file = CERT_CA;
   copt.pem_file = CERT_CLIENT;
   /* Weak cert validation allows never fails */
   copt.weak_cert_validation = 1;

   ssl_test (&copt, &sopt, "bad_domain.com", &cr, &sr);

   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SUCCESS);
   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SUCCESS);
}


static void
test_mongoc_tls_allow_invalid_hostname (void)
{
   mongoc_ssl_opt_t sopt = {0};
   mongoc_ssl_opt_t copt = {0};
   ssl_test_result_t sr;
   ssl_test_result_t cr;

   sopt.ca_file = CERT_CA;
   sopt.pem_file = CERT_SERVER;

   copt.ca_file = CERT_CA;
   copt.pem_file = CERT_CLIENT;
   copt.allow_invalid_hostname = 1;

   /* Connect to a domain not listed in the cert */
   ssl_test (&copt, &sopt, "bad_domain.com", &cr, &sr);

   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SUCCESS);
   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SUCCESS);
}


static void
test_mongoc_tls_bad_verify (void)
{
   mongoc_ssl_opt_t sopt = {0};
   mongoc_ssl_opt_t copt = {0};
   ssl_test_result_t sr;
   ssl_test_result_t cr;

   sopt.ca_file = CERT_CA;
   sopt.pem_file = CERT_SERVER;

   copt.ca_file = CERT_CA;
   copt.pem_file = CERT_CLIENT;

   capture_logs (true);
   ssl_test (&copt, &sopt, "bad_domain.com", &cr, &sr);

   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SSL_HANDSHAKE);
   ASSERT_CMPINT (sr.result, !=, SSL_TEST_SUCCESS);


   /* weak_cert_validation allows bad domains */
   copt.weak_cert_validation = 1;
   ssl_test (&copt, &sopt, "bad_domain.com", &cr, &sr);
   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SUCCESS);
   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SUCCESS);
}


static void
test_mongoc_tls_basic (void)
{
   mongoc_ssl_opt_t sopt = {0};
   mongoc_ssl_opt_t copt = {0};
   ssl_test_result_t sr;
   ssl_test_result_t cr;

   sopt.ca_file = CERT_CA;
   sopt.pem_file = CERT_SERVER;

   copt.ca_file = CERT_CA;
   copt.pem_file = CERT_CLIENT;

   ssl_test (&copt, &sopt, "localhost", &cr, &sr);

   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SUCCESS);
   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SUCCESS);
}


#ifdef MONGOC_ENABLE_SSL_OPENSSL
static void
test_mongoc_tls_weak_cert_validation (void)
{
   mongoc_ssl_opt_t sopt = {0};
   mongoc_ssl_opt_t copt = {0};
   ssl_test_result_t sr;
   ssl_test_result_t cr;

   sopt.ca_file = CERT_CA;
   sopt.pem_file = CERT_SERVER;

   copt.ca_file = CERT_CA;
   copt.crl_file = CERT_CRL;
   copt.pem_file = CERT_CLIENT;

   capture_logs (true);
   /* Certificate has has been revoked, this should fail */
   ssl_test (&copt, &sopt, "localhost", &cr, &sr);

   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SSL_HANDSHAKE);
   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SSL_HANDSHAKE);


   /* weak_cert_validation allows revoked certs */
   copt.weak_cert_validation = 1;
   ssl_test (&copt, &sopt, "bad_domain.com", &cr, &sr);

   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SUCCESS);
   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SUCCESS);
}


static void
test_mongoc_tls_crl (void)
{
   mongoc_ssl_opt_t sopt = {0};
   mongoc_ssl_opt_t copt = {0};
   ssl_test_result_t sr;
   ssl_test_result_t cr;

   sopt.ca_file = CERT_CA;
   sopt.pem_file = CERT_SERVER;

   copt.ca_file = CERT_CA;
   copt.pem_file = CERT_CLIENT;

   ssl_test (&copt, &sopt, "localhost", &cr, &sr);

   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SUCCESS);
   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SUCCESS);

   copt.crl_file = CERT_CRL;
   capture_logs (true);
   ssl_test (&copt, &sopt, "localhost", &cr, &sr);

   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SSL_HANDSHAKE);
   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SSL_HANDSHAKE);

   /* Weak cert validation allows revoked */
   copt.weak_cert_validation = true;
   ssl_test (&copt, &sopt, "localhost", &cr, &sr);
   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SUCCESS);
   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SUCCESS);
}
#endif


static void
test_mongoc_tls_expired (void)
{
   mongoc_ssl_opt_t sopt = {0};
   mongoc_ssl_opt_t copt = {0};
   ssl_test_result_t sr;
   ssl_test_result_t cr;

   sopt.ca_file = CERT_CA;
   sopt.pem_file = CERT_EXPIRED;

   copt.ca_file = CERT_CA;
   copt.pem_file = CERT_CLIENT;

   capture_logs (true);
   ssl_test (&copt, &sopt, "localhost", &cr, &sr);

   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SSL_HANDSHAKE);
   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SSL_HANDSHAKE);

   /* Weak cert validation allows expired certs */
   copt.weak_cert_validation = true;
   ssl_test (&copt, &sopt, "localhost", &cr, &sr);
   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SUCCESS);
   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SUCCESS);
}


#if !defined(MONGOC_ENABLE_SSL_SECURE_TRANSPORT)
static void
test_mongoc_tls_common_name (void)
{
   mongoc_ssl_opt_t sopt = {0};
   mongoc_ssl_opt_t copt = {0};
   ssl_test_result_t sr;
   ssl_test_result_t cr;

   sopt.ca_file = CERT_CA;
   sopt.pem_file = CERT_COMMONNAME;

   copt.ca_file = CERT_CA;
   copt.pem_file = CERT_CLIENT;

   /* Match against commonName */
   ssl_test (&copt, &sopt, "commonName.mongodb.org", &cr, &sr);

   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SUCCESS);
   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SUCCESS);
}
#endif // !defined(MONGOC_ENABLE_SSL_SECURE_TRANSPORT)


static void
test_mongoc_tls_altname (void)
{
   mongoc_ssl_opt_t sopt = {0};
   mongoc_ssl_opt_t copt = {0};
   ssl_test_result_t sr;
   ssl_test_result_t cr;

   sopt.ca_file = CERT_CA;
   sopt.pem_file = CERT_ALTNAME;

   copt.ca_file = CERT_CA;
   copt.pem_file = CERT_CLIENT;

   /* Match against secondary Subject Alternative Name (SAN) */
   ssl_test (&copt, &sopt, "alternative.mongodb.com", &cr, &sr);

   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SUCCESS);
   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SUCCESS);
}


static void
test_mongoc_tls_wild (void)
{
   mongoc_ssl_opt_t sopt = {0};
   mongoc_ssl_opt_t copt = {0};
   ssl_test_result_t sr;
   ssl_test_result_t cr;

   sopt.ca_file = CERT_CA;
   sopt.pem_file = CERT_WILD;

   copt.ca_file = CERT_CA;
   copt.pem_file = CERT_CLIENT;

   ssl_test (&copt, &sopt, "anything.mongodb.org", &cr, &sr);

   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SUCCESS);
   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SUCCESS);
}


#ifdef MONGOC_ENABLE_SSL_OPENSSL
static void
test_mongoc_tls_ip (void)
{
   mongoc_ssl_opt_t sopt = {0};
   mongoc_ssl_opt_t copt = {0};
   ssl_test_result_t sr;
   ssl_test_result_t cr;

   sopt.ca_file = CERT_CA;
   sopt.pem_file = CERT_SERVER;

   copt.ca_file = CERT_CA;

   ssl_test (&copt, &sopt, "127.0.0.1", &cr, &sr);

   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SUCCESS);
   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SUCCESS);
}
#endif


#if !defined(__APPLE__) && !defined(_WIN32) && defined(MONGOC_ENABLE_SSL_OPENSSL) && \
   OPENSSL_VERSION_NUMBER >= 0x10000000L
static void
test_mongoc_tls_trust_dir (void)
{
   mongoc_ssl_opt_t sopt = {0};
   mongoc_ssl_opt_t copt = {0};
   ssl_test_result_t sr;
   ssl_test_result_t cr;

   sopt.pem_file = CERT_SERVER;
   sopt.ca_file = CERT_CA;

   copt.ca_dir = CERT_TEST_DIR;

   ssl_test (&copt, &sopt, "localhost", &cr, &sr);

   ASSERT_CMPINT (cr.result, ==, SSL_TEST_SUCCESS);
   ASSERT_CMPINT (sr.result, ==, SSL_TEST_SUCCESS);
}
#endif

#endif /* !MONGOC_ENABLE_SSL_SECURE_CHANNEL */

void
test_mongoc_tls_insecure_nowarning (void)
{
   mongoc_uri_t *uri;
   mongoc_client_t *client;

   if (!test_framework_get_ssl ()) {
      return;
   }
   uri = test_framework_get_uri ();
   mongoc_uri_set_option_as_bool (uri, MONGOC_URI_TLSINSECURE, true);
   client = test_framework_client_new_from_uri (uri, NULL);

   capture_logs (true);
   mongoc_client_command_simple (
      client, "admin", tmp_bson ("{'ping': 1}"), NULL /* read prefs */, NULL /* reply */, NULL /* error */);
   ASSERT_NO_CAPTURED_LOGS ("has no effect");
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
}

#if defined(MONGOC_ENABLE_SSL_SECURE_CHANNEL)

static void
secure_channel_cred_deleter (void *data)
{
   mongoc_secure_channel_cred *cred = data;
   mongoc_secure_channel_cred_destroy (cred);
}

static bool
connect_with_secure_channel_cred (mongoc_ssl_opt_t *ssl_opt, mongoc_shared_ptr cred_ptr, bson_error_t *error)
{
   mongoc_host_list_t host;
   const int32_t connect_timout_ms = 10000;

   *error = (bson_error_t) {0};

   if (!_mongoc_host_list_from_string_with_err (&host, "localhost:27017", error)) {
      return false;
   }
   mongoc_stream_t *tcp_stream = mongoc_client_connect_tcp (connect_timout_ms, &host, error);
   if (!tcp_stream) {
      return false;
   }

   mongoc_stream_t *tls_stream = mongoc_stream_tls_secure_channel_new_with_creds (tcp_stream, ssl_opt, cred_ptr);
   if (!tls_stream) {
      mongoc_stream_destroy (tcp_stream);
      return false;
   }

   if (!mongoc_stream_tls_handshake_block (tls_stream, host.host, connect_timout_ms, error)) {
      mongoc_stream_destroy (tls_stream);
      return false;
   }

   mongoc_stream_destroy (tls_stream);
   return true;
}

// Test a TLS stream can be create with shared Secure Channel credentials.
static void
test_secure_channel_shared_creds (void *unused)
{
   BSON_UNUSED (unused);

   bool ok;
   bson_error_t error;
   mongoc_ssl_opt_t ssl_opt = {.ca_file = CERT_TEST_DIR "/ca.pem", .pem_file = CERT_TEST_DIR "/client.pem"};
   // Test with no sharing:
   {
      ok = connect_with_secure_channel_cred (&ssl_opt, MONGOC_SHARED_PTR_NULL, &error);
      ASSERT_OR_PRINT (ok, error);
   }

   // Test with sharing:
   {
      mongoc_shared_ptr cred_ptr =
         mongoc_shared_ptr_create (mongoc_secure_channel_cred_new (&ssl_opt), secure_channel_cred_deleter);
      ok = connect_with_secure_channel_cred (&ssl_opt, cred_ptr, &error);
      ASSERT_OR_PRINT (ok, error);
      // Use again.
      ok = connect_with_secure_channel_cred (&ssl_opt, cred_ptr, &error);
      ASSERT_OR_PRINT (ok, error);
      mongoc_shared_ptr_reset_null (&cred_ptr);
   }
}
#endif // MONGOC_ENABLE_SSL_SECURE_CHANNEL

void
test_stream_tls_install (TestSuite *suite)
{
   BSON_UNUSED (suite);

#if !defined(MONGOC_ENABLE_SSL_SECURE_CHANNEL)

   /* Disable /TLS/commonName on macOS due to CDRIVER-4256. */
#if !defined(MONGOC_ENABLE_SSL_SECURE_TRANSPORT)
   TestSuite_Add (suite, "/TLS/commonName", test_mongoc_tls_common_name);
#endif
   TestSuite_Add (suite, "/TLS/altname", test_mongoc_tls_altname);
   TestSuite_Add (suite, "/TLS/basic", test_mongoc_tls_basic);
   TestSuite_Add (suite, "/TLS/allow_invalid_hostname", test_mongoc_tls_allow_invalid_hostname);
   TestSuite_Add (suite, "/TLS/wild", test_mongoc_tls_wild);
   TestSuite_Add (suite, "/TLS/no_verify", test_mongoc_tls_no_verify);
   TestSuite_Add (suite, "/TLS/bad_verify", test_mongoc_tls_bad_verify);
   TestSuite_Add (suite, "/TLS/no_certs", test_mongoc_tls_no_certs);

   TestSuite_Add (suite, "/TLS/expired", test_mongoc_tls_expired);

#ifdef MONGOC_ENABLE_SSL_OPENSSL
   TestSuite_Add (suite, "/TLS/ip", test_mongoc_tls_ip);
   TestSuite_Add (suite, "/TLS/password", test_mongoc_tls_password);
   TestSuite_Add (suite, "/TLS/bad_password", test_mongoc_tls_bad_password);
   TestSuite_Add (suite, "/TLS/weak_cert_validation", test_mongoc_tls_weak_cert_validation);
   TestSuite_Add (suite, "/TLS/crl", test_mongoc_tls_crl);
#endif

#if !defined(__APPLE__) && !defined(_WIN32) && defined(MONGOC_ENABLE_SSL_OPENSSL) && \
   OPENSSL_VERSION_NUMBER >= 0x10000000L
   TestSuite_Add (suite, "/TLS/trust_dir", test_mongoc_tls_trust_dir);
#endif

   TestSuite_AddLive (suite, "/TLS/insecure_nowarning", test_mongoc_tls_insecure_nowarning);
#endif

#if defined(MONGOC_ENABLE_SSL_SECURE_CHANNEL)
   TestSuite_AddFull (suite,
                      "/TLS/secure_channel/shared_creds",
                      test_secure_channel_shared_creds,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_server_ssl);
#endif
}
