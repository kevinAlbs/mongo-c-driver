#include <mongoc/mongoc.h>
#include <mongoc/mongoc-ssl-private.h>

#ifdef MONGOC_ENABLE_SSL_OPENSSL
#include <mongoc/mongoc-openssl-private.h>
#endif

#ifdef MONGOC_ENABLE_SSL_SECURE_CHANNEL
#include <mongoc/mongoc-secure-channel-private.h>
#endif

#include <mongoc/mongoc-host-list-private.h>

#include "TestSuite.h"
#include "test-libmongoc.h"
#include "test-conveniences.h"         // tmp_bson
#include <mongoc/mongoc-log-private.h> // _mongoc_log_get_handler

#ifdef MONGOC_ENABLE_OCSP_OPENSSL
/* Test parsing a DER encoded tlsfeature extension contents for the
 * status_request (value 5). This is a SEQUENCE of INTEGER. libmongoc assumes
 * this is a sequence of one byte integers. */

static void
_expect_malformed (const char *data, int32_t len)
{
   bool ret;

   ret = _mongoc_tlsfeature_has_status_request ((const uint8_t *) data, len);
   BSON_ASSERT (!ret);
   ASSERT_CAPTURED_LOG ("mongoc", MONGOC_LOG_LEVEL_ERROR, "malformed");
   clear_captured_logs ();
}

static void
_expect_no_status_request (const char *data, int32_t len)
{
   bool ret;
   ret = _mongoc_tlsfeature_has_status_request ((const uint8_t *) data, len);
   BSON_ASSERT (!ret);
   ASSERT_NO_CAPTURED_LOGS ("mongoc");
}

static void
_expect_status_request (const char *data, int32_t len)
{
   bool ret;
   ret = _mongoc_tlsfeature_has_status_request ((const uint8_t *) data, len);
   BSON_ASSERT (ret);
   ASSERT_NO_CAPTURED_LOGS ("mongoc");
}

static void
test_tlsfeature_parsing (void)
{
   capture_logs (true);
   /* A sequence of one integer = 5. */
   _expect_status_request ("\x30\x03\x02\x01\x05", 5);
   /* A sequence of one integer = 6. */
   _expect_no_status_request ("\x30\x03\x02\x01\x06", 5);
   /* A sequence of two integers = 5,6. */
   _expect_status_request ("\x30\x03\x02\x01\x05\x02\x01\x06", 8);
   /* A sequence of two integers = 6,5. */
   _expect_status_request ("\x30\x03\x02\x01\x06\x02\x01\x05", 8);
   /* A sequence containing a non-integer. Parsing fails. */
   _expect_malformed ("\x30\x03\x03\x01\x05\x02\x01\x06", 8);
   /* A non-sequence. It will not read past the first byte (despite the >1
    * length). */
   _expect_malformed ("\xFF", 2);
   /* A sequence with a length represented in more than one byte. Parsing fails.
    */
   _expect_malformed ("\x30\x82\x04\x48", 4);
   /* An integer with length > 1. Parsing fails. */
   _expect_malformed ("\x30\x03\x02\x02\x05\x05", 6);
}
#endif /* MONGOC_ENABLE_OCSP_OPENSSL */

#ifdef MONGOC_ENABLE_SSL
static void
create_x509_user (void)
{
   bson_error_t error;

   mongoc_client_t *client = test_framework_new_default_client ();
   bool ok =
      mongoc_client_command_simple (client,
                                    "$external",
                                    tmp_bson (BSON_STR ({
                                       "createUser" : "C=US,ST=New York,L=New York City,O=MDB,OU=Drivers,CN=client",
                                       "roles" : [ {"role" : "readWrite", "db" : "db"} ]
                                    })),
                                    NULL /* read_prefs */,
                                    NULL /* reply */,
                                    &error);
   ASSERT_OR_PRINT (ok, error);
   mongoc_client_destroy (client);
}

static void
drop_x509_user (bool ignore_notfound)
{
   bson_error_t error;

   mongoc_client_t *client = test_framework_new_default_client ();
   bool ok = mongoc_client_command_simple (
      client,
      "$external",
      tmp_bson (BSON_STR ({"dropUser" : "C=US,ST=New York,L=New York City,O=MDB,OU=Drivers,CN=client"})),
      NULL /* read_prefs */,
      NULL /* reply */,
      &error);

   if (!ok) {
      ASSERT_OR_PRINT (ignore_notfound && NULL != strstr (error.message, "not found"), error);
   }
   mongoc_client_destroy (client);
}

static mongoc_uri_t *
get_x509_uri (void)
{
   bson_error_t error;
   char *uristr_noauth = test_framework_get_uri_str_no_auth ("db");
   mongoc_uri_t *uri = mongoc_uri_new_with_error (uristr_noauth, &error);
   ASSERT_OR_PRINT (uri, error);
   ASSERT (mongoc_uri_set_auth_mechanism (uri, "MONGODB-X509"));
   ASSERT (mongoc_uri_set_auth_source (uri, "$external"));
   bson_free (uristr_noauth);
   return uri;
}

static bool
try_insert (mongoc_client_t *client, bson_error_t *error)
{
   mongoc_collection_t *coll = mongoc_client_get_collection (client, "db", "coll");
   bool ok = mongoc_collection_insert_one (coll, tmp_bson ("{}"), NULL, NULL, error);
   mongoc_collection_destroy (coll);
   return ok;
}

static void
test_x509_auth (void *unused)
{
   BSON_UNUSED (unused);

   drop_x509_user (true /* ignore "not found" error */);
   create_x509_user ();

   // Test auth works with PKCS8 key:
   {
      // Create URI:
      mongoc_uri_t *uri = get_x509_uri ();
      {
         ASSERT (mongoc_uri_set_option_as_utf8 (
            uri, MONGOC_URI_TLSCERTIFICATEKEYFILE, CERT_TEST_DIR "/client-pkcs8-unencrypted.pem"));
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCAFILE, CERT_CA));
      }

      // Try auth:
      bson_error_t error = {0};
      bool ok;
      {
         mongoc_client_t *client = test_framework_client_new_from_uri (uri, NULL);
         ok = try_insert (client, &error);
         mongoc_client_destroy (client);
      }

      ASSERT_OR_PRINT (ok, error);
      mongoc_uri_destroy (uri);
   }

   // Test auth works:
   {
      // Create URI:
      mongoc_uri_t *uri = get_x509_uri ();
      {
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCERTIFICATEKEYFILE, CERT_CLIENT));
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCAFILE, CERT_CA));
      }

      // Try auth:
      bson_error_t error = {0};
      bool ok;
      {
         mongoc_client_t *client = test_framework_client_new_from_uri (uri, NULL);
         ok = try_insert (client, &error);
         mongoc_client_destroy (client);
      }

      ASSERT_OR_PRINT (ok, error);
      mongoc_uri_destroy (uri);
   }

   // Test auth fails with no client certificate:
   {
      // Create URI:
      mongoc_uri_t *uri = get_x509_uri ();
      {
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCAFILE, CERT_CA));
      }

      // Try auth:
      bson_error_t error = {0};
      bool ok;
      {
         mongoc_client_t *client = test_framework_client_new_from_uri (uri, NULL);
         ok = try_insert (client, &error);
         mongoc_client_destroy (client);
      }

      ASSERT (!ok);
      ASSERT_ERROR_CONTAINS (error,
                             MONGOC_ERROR_CLIENT,
                             MONGOC_ERROR_CLIENT_AUTHENTICATE,
                             "" /* message differs between server versions */);
      mongoc_uri_destroy (uri);
   }

   // Test auth works with explicit username:
   {
      // Create URI:
      mongoc_uri_t *uri = get_x509_uri ();
      {
         ASSERT (mongoc_uri_set_username (uri, "C=US,ST=New York,L=New York City,O=MDB,OU=Drivers,CN=client"));
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCERTIFICATEKEYFILE, CERT_CLIENT));
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCAFILE, CERT_CA));
      }

      // Try auth:
      bson_error_t error = {0};
      bool ok;
      {
         mongoc_client_t *client = test_framework_client_new_from_uri (uri, NULL);
         ok = try_insert (client, &error);
         mongoc_client_destroy (client);
      }

      ASSERT_OR_PRINT (ok, error);
      mongoc_uri_destroy (uri);
   }

   // Test auth fails with wrong username:
   {
      // Create URI:
      mongoc_uri_t *uri = get_x509_uri ();
      {
         ASSERT (mongoc_uri_set_username (uri, "bad"));
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCERTIFICATEKEYFILE, CERT_CLIENT));
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCAFILE, CERT_CA));
      }

      // Try auth:
      bson_error_t error = {0};
      bool ok;
      {
         mongoc_client_t *client = test_framework_client_new_from_uri (uri, NULL);
         ok = try_insert (client, &error);
         mongoc_client_destroy (client);
      }

      ASSERT (!ok);
      ASSERT_ERROR_CONTAINS (error,
                             MONGOC_ERROR_CLIENT,
                             MONGOC_ERROR_CLIENT_AUTHENTICATE,
                             "" /* message differs between server versions */);
      mongoc_uri_destroy (uri);
   }

   // Test auth fails with correct username but wrong certificate:
   {
      // Create URI:
      mongoc_uri_t *uri = get_x509_uri ();
      {
         ASSERT (mongoc_uri_set_username (uri, "C=US,ST=New York,L=New York City,O=MDB,OU=Drivers,CN=client"));
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCERTIFICATEKEYFILE, CERT_SERVER));
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCAFILE, CERT_CA));
      }

      // Try auth:
      bson_error_t error = {0};
      bool ok;
      {
         mongoc_client_t *client = test_framework_client_new_from_uri (uri, NULL);
         ok = try_insert (client, &error);
         mongoc_client_destroy (client);
      }

      ASSERT (!ok);
      ASSERT_ERROR_CONTAINS (error,
                             MONGOC_ERROR_CLIENT,
                             MONGOC_ERROR_CLIENT_AUTHENTICATE,
                             "" /* message differs between server versions */);
      mongoc_uri_destroy (uri);
   }

   // Test auth fails when client certificate does not contain public certificate:
   {
      // Create URI:
      mongoc_uri_t *uri = get_x509_uri ();
      {
         ASSERT (
            mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCERTIFICATEKEYFILE, CERT_TEST_DIR "/client-private.pem"));
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCAFILE, CERT_CA));
         ASSERT (mongoc_uri_set_option_as_bool (uri, MONGOC_URI_SERVERSELECTIONTRYONCE, true)); // Fail quickly.
      }

      // Try auth:
      bson_error_t error = {0};
      bool ok;
      {
         capture_logs (true); // Capture logs before connecting. OpenSSL reads PEM file during client construction.
         mongoc_client_t *client = test_framework_client_new_from_uri (uri, NULL);
         ok = try_insert (client, &error);
#if defined(MONGOC_ENABLE_SSL_SECURE_TRANSPORT)
         ASSERT_CAPTURED_LOG ("tls", MONGOC_LOG_LEVEL_ERROR, "Type is not supported");
#elif defined(MONGOC_ENABLE_SSL_SECURE_CHANNEL)
         ASSERT_CAPTURED_LOG ("tls", MONGOC_LOG_LEVEL_ERROR, "Can't find public certificate");
#elif defined(MONGOC_ENABLE_SSL_OPENSSL)
         ASSERT_CAPTURED_LOG ("tls", MONGOC_LOG_LEVEL_ERROR, "Cannot find certificate");
#endif
         mongoc_client_destroy (client);
      }

      ASSERT (!ok);
#if defined(MONGOC_ENABLE_SSL_OPENSSL) || defined(MONGOC_ENABLE_SSL_SECURE_TRANSPORT)
      // OpenSSL and Secure Transport fail to create stream (prior to TLS). Resulting in a server selection error.
      ASSERT_ERROR_CONTAINS (
         error, MONGOC_ERROR_SERVER_SELECTION, MONGOC_ERROR_SERVER_SELECTION_FAILURE, "connection error");
#else
      ASSERT_ERROR_CONTAINS (error,
                             MONGOC_ERROR_CLIENT,
                             MONGOC_ERROR_CLIENT_AUTHENTICATE,
                             "" /* message differs between server versions */);
#endif
      mongoc_uri_destroy (uri);
   }

   // Test auth fails when client certificate does not exist:
   {
      // Create URI:
      mongoc_uri_t *uri = get_x509_uri ();
      {
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCERTIFICATEKEYFILE, CERT_TEST_DIR "/foobar.pem"));
         ASSERT (mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCAFILE, CERT_CA));
         ASSERT (mongoc_uri_set_option_as_bool (uri, MONGOC_URI_SERVERSELECTIONTRYONCE, true)); // Fail quickly.
      }

      // Try auth:
      bson_error_t error = {0};
      bool ok;
      {
         mongoc_client_t *client = test_framework_client_new_from_uri (uri, NULL);
         capture_logs (true);
         ok = try_insert (client, &error);
#if defined(MONGOC_ENABLE_SSL_SECURE_TRANSPORT)
         ASSERT_CAPTURED_LOG ("tls", MONGOC_LOG_LEVEL_ERROR, "Cannot find certificate");
#elif defined(MONGOC_ENABLE_SSL_SECURE_CHANNEL)
         ASSERT_CAPTURED_LOG ("tls", MONGOC_LOG_LEVEL_ERROR, "Failed to open file");
#elif defined(MONGOC_ENABLE_SSL_OPENSSL)
         ASSERT_NO_CAPTURED_LOGS ("tls");
#endif
         mongoc_client_destroy (client);
      }

      ASSERT (!ok);
#if defined(MONGOC_ENABLE_SSL_OPENSSL) || defined(MONGOC_ENABLE_SSL_SECURE_TRANSPORT)
      // OpenSSL fails to create stream (prior to TLS). Resulting in a server selection error.
      ASSERT_ERROR_CONTAINS (
         error, MONGOC_ERROR_SERVER_SELECTION, MONGOC_ERROR_SERVER_SELECTION_FAILURE, "connection error");
#else
      ASSERT_ERROR_CONTAINS (error,
                             MONGOC_ERROR_CLIENT,
                             MONGOC_ERROR_CLIENT_AUTHENTICATE,
                             "" /* message differs between server versions */);
#endif
      mongoc_uri_destroy (uri);
   }
   drop_x509_user (false);
}

#ifdef MONGOC_ENABLE_SSL_SECURE_CHANNEL
static void
remove_crl_for_secure_channel (const char *crl_path)
{
   // Load CRL from file to query system store.
   PCCRL_CONTEXT crl_from_file = mongoc_secure_channel_load_crl (crl_path);
   ASSERT (crl_from_file);

   HCERTSTORE cert_store = CertOpenStore (CERT_STORE_PROV_SYSTEM,                  /* provider */
                                          X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, /* certificate encoding */
                                          0,                                       /* unused */
                                          CERT_SYSTEM_STORE_LOCAL_MACHINE,         /* dwFlags */
                                          L"Root"); /* system store name. "My" or "Root" */
   ASSERT (cert_store);

   PCCRL_CONTEXT crl_from_store = CertFindCRLInStore (cert_store, 0, 0, CRL_FIND_EXISTING, crl_from_file, NULL);
   ASSERT (crl_from_store);

   if (!CertDeleteCRLFromStore (crl_from_store)) {
      test_error (
         "Failed to delete CRL from store. Delete CRL manually to avoid test errors verifying server certificate.");
   }
   CertFreeCRLContext (crl_from_file);
   CertFreeCRLContext (crl_from_store);
   CertCloseStore (cert_store, 0);
}
#endif // MONGOC_ENABLE_SSL_SECURE_CHANNEL

// test_crl tests connection fails when server certificate is in CRL list.
static void
test_crl (void *unused)
{
   BSON_UNUSED (unused);

#if defined(MONGOC_ENABLE_SSL_SECURE_CHANNEL)
   if (!test_framework_getenv_bool ("MONGOC_TEST_SCHANNEL_CRL")) {
      printf ("Skipping. Test temporarily adds CRL to Windows certificate store. If removing the CRL fails, this may "
              "cause later test failures and require removing the CRL file manually. To run test anyway, set the "
              "environment variable MONGOC_TEST_SCHANNEL_CRL=ON\n");
      return;
   }
#elif defined(MONGOC_ENABLE_SSL_SECURE_TRANSPORT)
   printf ("Skipping. Secure Transport does not support crl_file.\n");
   return;
#endif

   // Create URI:
   mongoc_uri_t *uri = test_framework_get_uri ();
   ASSERT (mongoc_uri_set_option_as_bool (uri, MONGOC_URI_SERVERSELECTIONTRYONCE, true)); // Fail quickly.

   // Create SSL options with CRL file:
   mongoc_ssl_opt_t ssl_opts = *test_framework_get_ssl_opts ();
   ssl_opts.crl_file = CERT_TEST_DIR "/crl.pem";

   // Try insert:
   bson_error_t error = {0};
   mongoc_client_t *client = test_framework_client_new_from_uri (uri, NULL);
   mongoc_client_set_ssl_opts (client, &ssl_opts);
   capture_logs (true);
   bool ok = try_insert (client, &error);
#ifdef MONGOC_ENABLE_SSL_SECURE_CHANNEL
   remove_crl_for_secure_channel (ssl_opts.crl_file);
   ASSERT_CAPTURED_LOG ("tls", MONGOC_LOG_LEVEL_ERROR, "Mutual Authentication failed");
#else
   ASSERT_NO_CAPTURED_LOGS ("tls");
#endif
   ASSERT (!ok);
   ASSERT_ERROR_CONTAINS (
      error, MONGOC_ERROR_SERVER_SELECTION, MONGOC_ERROR_SERVER_SELECTION_FAILURE, "no suitable servers");

   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
}
#endif // MONGOC_ENABLE_SSL

#ifdef MONGOC_ENABLE_SSL_SECURE_CHANNEL

static void
secure_channel_cred_deleter (void *data)
{
   mongoc_secure_channel_cred *cred = data;
   mongoc_secure_channel_cred_destroy (cred);
}

static void
try_connect (mongoc_shared_ptr cred_ptr)
{
   bson_error_t error;
   mongoc_host_list_t host;
   const int32_t connect_timout_ms = 1000;

   // Use IPv4 literal to avoid 1 second delay when server not listening on IPv6 (see CDRIVER-????).
   ASSERT_OR_PRINT (_mongoc_host_list_from_string_with_err (&host, "127.0.0.1:27017", &error), error);
   mongoc_stream_t *stream = mongoc_client_connect_tcp (connect_timout_ms, &host, &error);
   ASSERT_OR_PRINT (stream, error);

   mongoc_ssl_opt_t ssl_opt = {.pem_file = CERT_TEST_DIR "/client-pkcs8-unencrypted.pem"};
   stream = mongoc_stream_tls_secure_channel_new_with_creds (stream, &ssl_opt, cred_ptr);
   ASSERT (stream);

   bool ok = mongoc_stream_tls_handshake_block (stream, host.host, connect_timout_ms, &error);
   ASSERT_OR_PRINT (ok, error);
   mongoc_stream_destroy (stream);
}

static BSON_THREAD_FUN (thread_fn, ctx)
{
   bson_error_t error;

   mongoc_shared_ptr *cred_ptr = ctx;

   for (size_t i = 0; i < 100; i++) {
      try_connect (*cred_ptr);
   }
   BSON_THREAD_RETURN;
}

// Test many threads doing client-auth with Secure Channel.
static void
test_secure_channel_multithreaded (void *unused)
{
   BSON_UNUSED (unused);

   bson_thread_t threads[10];

   // Test with no sharing:
   {
      mongoc_shared_ptr nullptr = MONGOC_SHARED_PTR_NULL;

      int64_t start = bson_get_monotonic_time ();
      MONGOC_DEBUG ("Connecting ... starting");
      for (size_t i = 0; i < 10; i++) {
         mcommon_thread_create (&threads[i], thread_fn, &nullptr);
      }
      MONGOC_DEBUG ("Connecting ... joining");
      for (size_t i = 0; i < 10; i++) {
         mcommon_thread_join (threads[i]);
      }
      MONGOC_DEBUG ("Connecting ... done");
      int64_t end = bson_get_monotonic_time ();
      MONGOC_DEBUG ("No sharing took: %.02fms", (double) (end - start) / 1000.0);
   }

   // Test with sharing:
   {
      int64_t start = bson_get_monotonic_time ();
      mongoc_ssl_opt_t ssl_opt = {.pem_file = CERT_TEST_DIR "/client-pkcs8-unencrypted.pem"};
      mongoc_shared_ptr cred_ptr =
         mongoc_shared_ptr_create (mongoc_secure_channel_cred_new (&ssl_opt), secure_channel_cred_deleter);
      MONGOC_DEBUG ("Connecting ... starting");
      for (size_t i = 0; i < 10; i++) {
         mcommon_thread_create (&threads[i], thread_fn, &cred_ptr);
      }
      MONGOC_DEBUG ("Connecting ... joining");
      for (size_t i = 0; i < 10; i++) {
         mcommon_thread_join (threads[i]);
      }
      MONGOC_DEBUG ("Connecting ... done");
      int64_t end = bson_get_monotonic_time ();
      MONGOC_DEBUG ("Sharing took: %.02fms", (double) (end - start) / 1000.0);
      mongoc_shared_ptr_reset_null (&cred_ptr);
   }
}

static bool
connect_with_secure_channel_cred (mongoc_ssl_opt_t *ssl_opt, mongoc_shared_ptr cred_ptr, bson_error_t *error)
{
   mongoc_host_list_t host;
   const int32_t connect_timout_ms = 1000;

   *error = (bson_error_t) {0};

   // Use IPv4 literal to avoid 1 second delay when server not listening on IPv6 (see CDRIVER-????).
   if (!_mongoc_host_list_from_string_with_err (&host, "127.0.0.1:27017", error)) {
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

static void
test_secure_channel_shared_creds_stream (void *unused)
{
   BSON_UNUSED (unused);

   bool ok;
   bson_error_t error;
   mongoc_ssl_opt_t ssl_opt = {.ca_file = CERT_TEST_DIR "/ca.pem",
                               .pem_file = CERT_TEST_DIR "/client-pkcs8-unencrypted.pem"};
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

typedef struct {
   size_t failures;
   size_t failures2;
} cert_failures;


void
count_cert_failures (mongoc_log_level_t log_level, const char *log_domain, const char *message, void *user_data)
{
   printf ("Would have logged: %s\n", message);
   cert_failures *cf = user_data;
   if (strstr (message, "Failed to open file: 'does-not-exist.pem'")) {
      cf->failures++;
   }
   if (strstr (message, "Failed to open file: 'does-not-exist-2.pem'")) {
      cf->failures2++;
   }
}

static bool
try_ping_with_reconnect (mongoc_client_t *client, bson_error_t *error)
{
   // Force a connection error with a failpoint:
   if (!mongoc_client_command_simple (client,
                                      "admin",
                                      tmp_bson (BSON_STR ({
                                         "configureFailPoint" : "failCommand",
                                         "mode" : {"times" : 1},
                                         "data" : {"closeConnection" : true, "failCommands" : ["ping"]}
                                      })),
                                      NULL,
                                      NULL,
                                      error)) {
      return false;
   }

   // Expect first ping to fail:
   if (mongoc_client_command_simple (client, "admin", tmp_bson (BSON_STR ({"ping" : 1})), NULL, NULL, error)) {
      bson_set_error (error, 0, 0, "unexpected: ping succeeded, but expected to fail");
      return false;
   }

   // Ping again:
   return mongoc_client_command_simple (client, "admin", tmp_bson (BSON_STR ({"ping" : 1})), NULL, NULL, error);
}

static void
test_secure_channel_shared_creds_client (void *unused)
{
   BSON_UNUSED (unused);

   bson_error_t error;

   // Save log function:
   mongoc_log_func_t saved_log_func;
   void *saved_log_data;
   _mongoc_log_get_handler (&saved_log_func, &saved_log_data);

   // Set log function to count failed attempts to load client cert:
   cert_failures cf = {0};
   mongoc_log_set_handler (count_cert_failures, &cf);

   // Test client:
   {
      mongoc_client_t *client = test_framework_new_default_client ();

      // Set client cert to a bad path:
      {
         mongoc_ssl_opt_t ssl_opt = *test_framework_get_ssl_opts ();
         ssl_opt.pem_file = "does-not-exist.pem";
         mongoc_client_set_ssl_opts (client, &ssl_opt);
      }

      // Expect insert OK. Cert fails to load, but server configured with --tlsAllowConnectionsWithoutCertificates:
      {
         bool ok = try_insert (client, &error);
         ASSERT_OR_PRINT (ok, error);
      }

      // Expect exactly one attempt to load the client cert:
      ASSERT_CMPSIZE_T (1, ==, cf.failures);
   }

   cf = (cert_failures) {0};

   // Test pool:
   {
      mongoc_client_pool_t *pool = test_framework_new_default_client_pool ();

      // Set client cert to a bad path:
      {
         mongoc_ssl_opt_t ssl_opt = *test_framework_get_ssl_opts ();
         ssl_opt.pem_file = "does-not-exist.pem";
         mongoc_client_pool_set_ssl_opts (pool, &ssl_opt);
      }

      mongoc_client_t *client = mongoc_client_pool_pop (pool);

      // Expect insert OK. Cert fails to load, but server configured with --tlsAllowConnectionsWithoutCertificates:
      {
         bool ok = try_insert (client, &error);
         ASSERT_OR_PRINT (ok, error);
      }

      mongoc_client_pool_push (pool, client);

      // Expect exactly one attempt to load the client cert:
      ASSERT_CMPSIZE_T (1, ==, cf.failures);

      mongoc_client_pool_destroy (pool);
   }

   cf = (cert_failures) {0};

   // Test client changing TLS options after connecting:
   // Changing TLS options after connecting is discouraged, but is tested for backwards compatibility.
   {
      mongoc_client_t *client = test_framework_new_default_client ();

      // Set client cert to a bad path:
      {
         mongoc_ssl_opt_t ssl_opt = *test_framework_get_ssl_opts ();
         ssl_opt.pem_file = "does-not-exist.pem";
         mongoc_client_set_ssl_opts (client, &ssl_opt);
      }

      // Expect insert OK. Cert fails to load, but server configured with --tlsAllowConnectionsWithoutCertificates:
      {
         bool ok = try_insert (client, &error);
         ASSERT_OR_PRINT (ok, error);
      }

      // Expect exactly one attempt to load the client cert:
      ASSERT_CMPSIZE_T (1, ==, cf.failures);
      ASSERT_CMPSIZE_T (0, ==, cf.failures2);

      // Change the client cert:
      {
         mongoc_ssl_opt_t ssl_opt = *test_framework_get_ssl_opts ();
         ssl_opt.pem_file = "does-not-exist-2.pem";
         mongoc_client_set_ssl_opts (client, &ssl_opt);
      }

      // Force a reconnect.
      {
         bool ok = try_ping_with_reconnect (client, &error);
         ASSERT_OR_PRINT (ok, error);
      }

      // Expect an attempt to load the new cert:
      ASSERT_CMPSIZE_T (1, ==, cf.failures); // Unchanged.
      ASSERT_CMPSIZE_T (1, ==, cf.failures2);

      mongoc_client_destroy (client);
   }

   // Restore log handler:
   mongoc_log_set_handler (saved_log_func, saved_log_data);
}


#endif // MONGOC_ENABLE_SSL_SECURE_CHANNEL

void
test_x509_install (TestSuite *suite)
{
#ifdef MONGOC_ENABLE_SSL
   TestSuite_AddFull (suite,
                      "/X509/auth",
                      test_x509_auth,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_auth,
                      test_framework_skip_if_no_server_ssl);
   TestSuite_AddFull (suite, "/X509/crl", test_crl, NULL, NULL, test_framework_skip_if_no_server_ssl);
#endif

#ifdef MONGOC_ENABLE_OCSP_OPENSSL
   TestSuite_Add (suite, "/X509/tlsfeature_parsing", test_tlsfeature_parsing);
#endif

#ifdef MONGOC_ENABLE_SSL_SECURE_CHANNEL
   TestSuite_AddFull (suite,
                      "/X509/secure_channel/multithreaded",
                      test_secure_channel_multithreaded,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_auth,
                      test_framework_skip_if_no_server_ssl);
   TestSuite_AddFull (suite,
                      "/X509/secure_channel/shared_creds/stream",
                      test_secure_channel_shared_creds_stream,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_auth,
                      test_framework_skip_if_no_server_ssl);

   TestSuite_AddFull (suite,
                      "/X509/secure_channel/shared_creds/client",
                      test_secure_channel_shared_creds_client,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_server_ssl);
#endif
}
