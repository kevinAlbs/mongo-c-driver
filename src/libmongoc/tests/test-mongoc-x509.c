#include <mongoc/mongoc.h>
#include <mongoc/mongoc-ssl-private.h>

#ifdef MONGOC_ENABLE_SSL_OPENSSL
#include <mongoc/mongoc-openssl-private.h>
#endif

#ifdef MONGOC_ENABLE_SSL_SECURE_CHANNEL
#include <mongoc/mongoc-secure-channel-private.h>
#endif

#include "TestSuite.h"
#include "test-libmongoc.h"
#include "test-conveniences.h" // tmp_bson

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

   // Test auth works with certificate selector (Secure Channel only).
   {
      // Create URI:
      mongoc_uri_t *uri = get_x509_uri ();

      // Set TLS options with mongoc_ssl_opt_t instead of URI. URI does not support a certificate selector.
      mongoc_ssl_opt_t ssl_opt = {
         .ca_file = CERT_CA,
         .selector_thumbprint = "934494bc44ac628ccccd697a4a2bea7c62b5092f"
      };

      // Try auth:
      bson_error_t error = {0};
      bool ok;
      {
         mongoc_client_t *client = test_framework_client_new_from_uri (uri, NULL);
         mongoc_client_set_ssl_opts (client, &ssl_opt);
         
         capture_logs (true);
         ok = try_insert (client, &error);
#if defined(MONGOC_ENABLE_SSL_SECURE_TRANSPORT) || defined(MONGOC_ENABLE_SSL_OPENSSL)
         ASSERT_CAPTURED_LOG ("tls", MONGOC_LOG_LEVEL_ERROR, "certificate selector not supported");
#elif defined(MONGOC_ENABLE_SSL_SECURE_CHANNEL)
         ASSERT_NO_CAPTURED_LOGS ("tls");
         ASSERT_OR_PRINT (ok, error);
#else
         BSON_UNREACHABLE();
#endif
         mongoc_client_destroy (client);
      }

      mongoc_uri_destroy (uri);
   }

#if defined(MONGOC_ENABLE_SSL_SECURE_CHANNEL)
   // Test passing both certificate selector and client certificate is an error.
   {
      test_error ("Not yet implemented");
   }

   // Test passing an invalid certificate selector is an error.
   {
      test_error ("Not yet implemented");
   }
#endif // MONGOC_ENABLE_SSL_SECURE_CHANNEL

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

#include <mongoc/mongoc-host-list-private.h>
#include <mongoc/mongoc-stream-tls-secure-channel-private.h>
#include <mongoc/mongoc-secure-channel-private.h>

static bool try_connect (const char* host_and_port, PCCERT_CONTEXT cert, bson_error_t *handshake_error) {
   bson_error_t error;
   mongoc_host_list_t host;
   const int32_t connect_timout_ms = 1000;

   ASSERT_OR_PRINT (_mongoc_host_list_from_string_with_err(&host, host_and_port, &error), error);
   mongoc_stream_t *stream = mongoc_client_connect_tcp (connect_timout_ms, &host, &error);
   ASSERT_OR_PRINT (stream, error);
   
   mongoc_ssl_opt_t ssl_opt = {0};
   // Copy `cert`. TLS stream frees on destroy.
   stream = mongoc_stream_tls_secure_channel_new_with_PCERT_CONTEXT (
      stream, host.host, &ssl_opt, true, CertDuplicateCertificateContext (cert));
   if (!stream) {
      *handshake_error = (bson_error_t) {0};
      return false;
   }

   bool ok = mongoc_stream_tls_handshake_block (stream, host.host, connect_timout_ms, handshake_error);
   mongoc_stream_destroy (stream);
   return ok;
}

static size_t
count_files (const char *dir_path)
{
   size_t count = 0;
   char *dir_path_plus_star;
   intptr_t handle;
   struct _finddata_t info;

   char child_path[MAX_TEST_NAME_LENGTH];

   dir_path_plus_star = bson_strdup_printf ("%s/*", dir_path);
   handle = _findfirst (dir_path_plus_star, &info);
   ASSERT (handle != -1);

   while (1) {
      count++;
      if (_findnext (handle, &info) == -1) {
         break;
      }
   }

   bson_free (dir_path_plus_star);
   _findclose (handle);

   return count;
}

static size_t
count_capi_keys (void)
{
   return count_files ("C:\\Users\\Administrator\\AppData\\Roaming\\Microsoft\\Crypto\\RSA\\S-1-5-21-"
                "238269189-1184923242-2057656851-500");
}

static size_t
count_cng_keys (void)
{
   return count_files ("C:\\Users\\Administrator\\AppData\\Roaming\\Microsoft\\Crypto\\Keys");
}

#include <ncrypt.h>

extern char *
read_file_and_null_terminate (const char *filename, size_t *out_len);

extern LPBYTE
decode_pem_base64 (const char *base64_in, DWORD *out_len, const char *descriptor, const char *filename);

static void test_certs (void) {
   char *CLOUD_PROD_HOST = test_framework_getenv ("CLOUD_PROD_HOST");
   char *CLOUD_PROD_CERT = test_framework_getenv ("CLOUD_PROD_CERT");

   char *CLOUD_DEV_HOST = test_framework_getenv ("CLOUD_DEV_HOST");
   char *CLOUD_DEV_CERT = test_framework_getenv ("CLOUD_DEV_CERT");
   char *CLOUD_DEV_CERT_PKCS12 = test_framework_getenv ("CLOUD_DEV_CERT_PKCS12");



   // Test import with allow overwrite.
   // With CAPI: appears to repeatedly add files.
   // With CNG: appears to result in later call to AcquireCredentialsHandle failing...
   {
      size_t key_count_capi = count_capi_keys ();
      size_t key_count_cng = count_cng_keys ();

      bson_error_t error;


      size_t out_len;
      char *blob = read_file_and_null_terminate (CLOUD_DEV_CERT_PKCS12, &out_len);
      ASSERT (blob);
      ASSERT (mlib_in_range (DWORD, out_len));
      CRYPT_DATA_BLOB datablob = {.cbData = out_len, .pbData = blob};
      LPWSTR pwd = L"";

      // Test CAPI:
      {
         HCERTSTORE cert_store = PFXImportCertStore (&datablob, pwd, PKCS12_ALLOW_OVERWRITE_KEY);
         ASSERT (cert_store);

         // Imports as a CAPI key and adds a file:
         ASSERT_CMPSIZE_T (key_count_capi + 1, ==, count_capi_keys ());
         ASSERT_CMPSIZE_T (key_count_cng, ==, count_cng_keys ());

         PCCERT_CONTEXT cert = CertFindCertificateInStore (
            cert_store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_HAS_PRIVATE_KEY, NULL, NULL);

         ASSERT (cert);
         bool ok = try_connect (CLOUD_PROD_HOST, cert, &error);
         ASSERT_OR_PRINT (ok, error);

         CertFreeCertificateContext (cert);
         CertCloseStore (cert_store, 0);
      }

      // Test CNG:
      {
         HCERTSTORE cert_store = PFXImportCertStore (&datablob, pwd, PKCS12_ALLOW_OVERWRITE_KEY | PKCS12_ALWAYS_CNG_KSP);
         ASSERT (cert_store);

         // Imports as a CAPI key and adds a file:
         ASSERT_CMPSIZE_T (key_count_capi + 1, ==, count_capi_keys ());
         ASSERT_CMPSIZE_T (key_count_cng, ==, count_cng_keys ());

         PCCERT_CONTEXT cert = CertFindCertificateInStore (
            cert_store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_HAS_PRIVATE_KEY, NULL, NULL);

         ASSERT (cert);
         bool ok = try_connect (CLOUD_PROD_HOST, cert, &error);
         ASSERT (!ok); // Fails!?
         /// No error set.

         CertFreeCertificateContext (cert);
         CertCloseStore (cert_store, 0);

      }

         bson_free (blob);
   }



   
   // Test cloud-prod:
   {
      bson_error_t error;
      mongoc_ssl_opt_t ssl_opt = {.pem_file = CLOUD_PROD_CERT};
      PCCERT_CONTEXT cert = mongoc_secure_channel_setup_certificate (&ssl_opt);
      ASSERT_OR_PRINT (try_connect(CLOUD_PROD_HOST, cert, &error), error);
      CertFreeCertificateContext (cert);
   }

   // Test cloud-dev:
   {
      bson_error_t error;
      mongoc_ssl_opt_t ssl_opt = {.pem_file = CLOUD_DEV_CERT};
      PCCERT_CONTEXT cert = mongoc_secure_channel_setup_certificate (&ssl_opt);
      ASSERT (!try_connect(CLOUD_DEV_HOST, cert, &error));
      // Expect error. See CDRIVER-5998. Appears due to client only using RSA+SHA1 for Certificate Verify.
      ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_SOCKET, "do not possess a common algorithm");
      CertFreeCertificateContext (cert);
      // Q: Why does test-libmongoc fail when this is run with forking? A:
   }

   // Test cloud-dev with dotnet runtime approach (load PKCS#12 persisted, then delete later)
   {
      size_t key_count_capi = count_capi_keys ();
      size_t key_count_cng = count_cng_keys ();

      bson_error_t error;

      size_t out_len;
      char* blob = read_file_and_null_terminate (CLOUD_DEV_CERT_PKCS12, &out_len);
      ASSERT (blob);
      ASSERT (mlib_in_range (DWORD, out_len));
      CRYPT_DATA_BLOB datablob = {.cbData = out_len, .pbData = blob};
      // Assume empty password for now.
      LPWSTR pwd = L"";
      HCERTSTORE cert_store = PFXImportCertStore (&datablob, pwd, 0);
      ASSERT (cert_store);

      // Imports as a CAPI key:
      ASSERT_CMPSIZE_T (key_count_capi + 1, ==, count_capi_keys ());
      ASSERT_CMPSIZE_T (key_count_cng, ==, count_cng_keys ());

      PCCERT_CONTEXT cert = CertFindCertificateInStore (
         cert_store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_HAS_PRIVATE_KEY, NULL, NULL);

      ASSERT (cert);      
      ASSERT_OR_PRINT (try_connect (CLOUD_PROD_HOST, cert, &error), error);


      // Delete imported key:
      {
         DWORD count = 0; // size contained in pcbData often exceeds the size of the base structure.
         ASSERT (CertGetCertificateContextProperty (cert, CERT_KEY_PROV_INFO_PROP_ID, NULL, &count));
         uint8_t *bytes = bson_malloc0 (count);
         ASSERT (CertGetCertificateContextProperty (cert, CERT_KEY_PROV_INFO_PROP_ID, bytes, &count));
         PCRYPT_KEY_PROV_INFO keyProviderInfo = (PCRYPT_KEY_PROV_INFO) bytes;

         // dwProvType of 0 means CNG was used. Expect CAPI was used.
         ASSERT_CMPUINT32 ((uint32_t) keyProviderInfo->dwProvType, !=, 0);
         DWORD cryptAcquireContextFlags = keyProviderInfo->dwFlags | CRYPT_DELETE_KEYSET;
         HCRYPTPROV hProv = 0;
         CryptAcquireContextW (&hProv,
                               keyProviderInfo->pwszContainerName,
                               keyProviderInfo->pwszProvName,
                               keyProviderInfo->dwProvType,
                               cryptAcquireContextFlags);
         ASSERT (hProv == 0);
      }

      ASSERT_CMPSIZE_T (key_count_capi, ==, count_capi_keys ());
      ASSERT_CMPSIZE_T (key_count_cng, ==, count_cng_keys ());

      bson_free (blob);
      CertFreeCertificateContext (cert);
      CertCloseStore (cert_store, 0); // Keep open until after deleting persisted key.
   }

   // Test cloud-dev with dotnet runtime approach with non-deprecated API (load PKCS#12 persisted to CNG-only, then delete later)
   {
      size_t key_count_capi = count_capi_keys ();
      size_t key_count_cng = count_cng_keys ();

      bson_error_t error;

      size_t out_len;
      char *blob = read_file_and_null_terminate (CLOUD_DEV_CERT_PKCS12, &out_len);
      ASSERT (blob);
      ASSERT (mlib_in_range (DWORD, out_len));
      CRYPT_DATA_BLOB datablob = {.cbData = out_len, .pbData = blob};
      // Assume empty password for now.
      LPWSTR pwd = L"";
      HCERTSTORE cert_store = PFXImportCertStore (&datablob, pwd, PKCS12_ALWAYS_CNG_KSP);
      ASSERT (cert_store);

      // Imports as a CAPI key:
      ASSERT_CMPSIZE_T (key_count_capi, ==, count_capi_keys ());
      ASSERT_CMPSIZE_T (key_count_cng + 1, ==, count_cng_keys ());

      // Try importing again, but allow overwrite to ensure another key is not persisted.
      {
         HCERTSTORE cert_store2 =
            PFXImportCertStore (&datablob, pwd, PKCS12_ALWAYS_CNG_KSP | PKCS12_ALLOW_OVERWRITE_KEY);
         ASSERT (cert_store2);

         // Imports as a CAPI key:
         ASSERT_CMPSIZE_T (key_count_capi, ==, count_capi_keys ());
         ASSERT_CMPSIZE_T (key_count_cng + 1, ==, count_cng_keys ());

         CertCloseStore (cert_store2, 0);
      }

      PCCERT_CONTEXT cert = CertFindCertificateInStore (
         cert_store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_HAS_PRIVATE_KEY, NULL, NULL);

      ASSERT (cert);
      ASSERT_OR_PRINT (try_connect (CLOUD_PROD_HOST, cert, &error), error);


      // Delete imported key:
      {
         DWORD count = 0; // size contained in pcbData often exceeds the size of the base structure.
         ASSERT (CertGetCertificateContextProperty (cert, CERT_KEY_PROV_INFO_PROP_ID, NULL, &count));
         uint8_t *bytes = bson_malloc0 (count);
         ASSERT (CertGetCertificateContextProperty (cert, CERT_KEY_PROV_INFO_PROP_ID, bytes, &count));
         PCRYPT_KEY_PROV_INFO keyProviderInfo = (PCRYPT_KEY_PROV_INFO) bytes;

         // Imported key name appears generated and non-deterministic.
         wprintf (L"Imported key name: %s\n", keyProviderInfo->pwszContainerName);

         // dwProvType of 0 means CNG was used. Expect CNG was used.
         ASSERT_CMPUINT32 ((uint32_t) keyProviderInfo->dwProvType, ==, 0);
         DWORD ncryptFlags = 0;
         // Check if a machine key (vs user key)
         if (keyProviderInfo->dwFlags & CRYPT_MACHINE_KEYSET) {
            ncryptFlags |= NCRYPT_MACHINE_KEY_FLAG;
         }
         NCRYPT_PROV_HANDLE provider;
         SECURITY_STATUS sec_status = NCryptOpenStorageProvider (&provider, keyProviderInfo->pwszProvName, 0);
         ASSERT_CMPUINT32 (sec_status, ==, SEC_E_OK);

         NCRYPT_PROV_HANDLE keyHandle;
         sec_status = NCryptOpenKey (provider, &keyHandle, keyProviderInfo->pwszContainerName, 0, ncryptFlags);
         ASSERT_CMPUINT32 (sec_status, ==, SEC_E_OK);

         sec_status = NCryptDeleteKey (keyHandle, 0); // Also frees handle.
         ASSERT_CMPUINT32 (sec_status, ==, SEC_E_OK);

         sec_status = NCryptFreeObject (provider);
         ASSERT_CMPUINT32 (sec_status, ==, SEC_E_OK);
      }

      ASSERT_CMPSIZE_T (key_count_capi, ==, count_capi_keys ());
      ASSERT_CMPSIZE_T (key_count_cng, ==, count_cng_keys ());

      bson_free (blob);
      CertFreeCertificateContext (cert);
      CertCloseStore (cert_store, 0); // Keep open until after deleting persisted key.
   }

   // Test importing PKCS#8 key in CNG and persist.
   {
      size_t key_count_capi = count_capi_keys ();
      size_t key_count_cng = count_cng_keys ();
      bson_error_t error;
      

      size_t out_len;
      char *pem = read_file_and_null_terminate (CLOUD_DEV_CERT, &out_len);
      ASSERT (pem);
      char* pem_private = strstr (pem, "-----BEGIN PRIVATE KEY-----");
      ASSERT (pem_private);

      // Find the private-key portion of the blob.
      DWORD privateKeyBlobLen;
      LPBYTE privateKeyBlob = decode_pem_base64 (pem_private, &privateKeyBlobLen, "private key", CLOUD_DEV_CERT);

      NCRYPT_PROV_HANDLE hProv;
      // Open the software key storage provider
      SECURITY_STATUS status = NCryptOpenStorageProvider (&hProv, MS_KEY_STORAGE_PROVIDER, 0);
      ASSERT_CMPUINT32 (status, ==, 0);
      
      // Supply a key name to persist the key:
      NCryptBuffer buffer;
      NCryptBufferDesc bufferDesc;

      WCHAR keyName[] = L"TestKey"; // TODO: Replace with unique name? Q: What key name does PFXImportCertStore use? A:
      buffer.cbBuffer = (ULONG) (wcslen (keyName) + 1) * sizeof (WCHAR);
      buffer.BufferType = NCRYPTBUFFER_PKCS_KEY_NAME;
      buffer.pvBuffer = keyName;

      bufferDesc.ulVersion = NCRYPTBUFFER_VERSION;
      bufferDesc.cBuffers = 1;
      bufferDesc.pBuffers = &buffer;

      // Import the private key blob
      NCRYPT_KEY_HANDLE hKey;
      ASSERT (mlib_in_range (DWORD, out_len));
      status =
         NCryptImportKey (hProv, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, &bufferDesc, &hKey, privateKeyBlob, privateKeyBlobLen, 0);
      if (status != SEC_E_OK) {
         test_error ("Failed to import key: %s", mongoc_winerr_to_string (status));
      }

      // Key is imported into CNG:
      ASSERT_CMPSIZE_T (key_count_capi, ==, count_capi_keys ());
      ASSERT_CMPSIZE_T (key_count_cng + 1, ==, count_cng_keys ());

      // Create certificate:
      const char* pem_public = strstr (pem, "-----BEGIN CERTIFICATE-----");
      ASSERT (pem_public);
      DWORD encoded_cert_len;
      LPBYTE encoded_cert = decode_pem_base64 (pem_public, &encoded_cert_len, "public key", CLOUD_DEV_CERT);
      ASSERT (encoded_cert);
      PCCERT_CONTEXT cert = CertCreateCertificateContext (X509_ASN_ENCODING, encoded_cert, encoded_cert_len);
      ASSERT (cert);

      // Attach private key to certificate:
       CRYPT_KEY_PROV_INFO keyProvInfo = {
          .pwszContainerName = keyName, .dwProvType = 0 /* CNG */, .dwFlags = 0, .dwKeySpec = AT_KEYEXCHANGE};
       bool ok = CertSetCertificateContextProperty (cert, CERT_KEY_PROV_INFO_PROP_ID, 0, &keyProvInfo);
       ASSERT (ok);

      ASSERT_OR_PRINT (try_connect (CLOUD_PROD_HOST, cert, &error), error);

      // Delete key:
      {
         DWORD count = 0; // size contained in pcbData often exceeds the size of the base structure.
         ASSERT (CertGetCertificateContextProperty (cert, CERT_KEY_PROV_INFO_PROP_ID, NULL, &count));
         uint8_t *bytes = bson_malloc0 (count);
         ASSERT (CertGetCertificateContextProperty (cert, CERT_KEY_PROV_INFO_PROP_ID, bytes, &count));
         PCRYPT_KEY_PROV_INFO keyProviderInfo = (PCRYPT_KEY_PROV_INFO) bytes;

         // dwProvType of 0 means CNG was used. Expect CNG was used.
         ASSERT_CMPUINT32 ((uint32_t) keyProviderInfo->dwProvType, ==, 0);
         DWORD ncryptFlags = 0;
         // Check if a machine key (vs user key)
         if (keyProviderInfo->dwFlags & CRYPT_MACHINE_KEYSET) {
            ncryptFlags |= NCRYPT_MACHINE_KEY_FLAG;
         }
         NCRYPT_PROV_HANDLE provider;
         SECURITY_STATUS sec_status = NCryptOpenStorageProvider (&provider, keyProviderInfo->pwszProvName, 0);
         ASSERT_CMPUINT32 (sec_status, ==, SEC_E_OK);

         NCRYPT_PROV_HANDLE keyHandle;
         sec_status = NCryptOpenKey (provider, &keyHandle, keyProviderInfo->pwszContainerName, 0, ncryptFlags);
         ASSERT_CMPUINT32 (sec_status, ==, SEC_E_OK);

         sec_status = NCryptDeleteKey (keyHandle, 0); // Also frees handle.
         ASSERT_CMPUINT32 (sec_status, ==, SEC_E_OK);

         sec_status = NCryptFreeObject (provider);
         ASSERT_CMPUINT32 (sec_status, ==, SEC_E_OK);
      }
      ASSERT_CMPSIZE_T (key_count_capi, ==, count_capi_keys ());
      ASSERT_CMPSIZE_T (key_count_cng, ==, count_cng_keys ());

      NCryptFreeObject (hKey);
      NCryptFreeObject (hProv);
   }

   // TODO: try to convert PKCS#8 to PKCS#12, then import. This appears to still require a key name.
   if (0) {
      size_t key_count_capi = count_capi_keys ();
      size_t key_count_cng = count_cng_keys ();
      bson_error_t error;
      bool ok;


      size_t out_len;
      char *pem = read_file_and_null_terminate (CLOUD_DEV_CERT, &out_len);
      ASSERT (pem);
      char* pem_private = strstr (pem, "-----BEGIN PRIVATE KEY-----");
      ASSERT (pem_private);

      // Find the private-key portion of the blob.
      DWORD privateKeyBlobLen;
      LPBYTE privateKeyBlob = decode_pem_base64 (pem_private, &privateKeyBlobLen, "private key", CLOUD_DEV_CERT);

      NCRYPT_PROV_HANDLE hProv;
      // Open the software key storage provider
      SECURITY_STATUS status = NCryptOpenStorageProvider (&hProv, MS_KEY_STORAGE_PROVIDER, 0);
      ASSERT_CMPUINT32 (status, ==, 0);
      
      // Import the private key blob as an ephemeral key
      NCRYPT_KEY_HANDLE hKey;
      ASSERT (mlib_in_range (DWORD, out_len));
      status =
         NCryptImportKey (hProv, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, 0, &hKey, privateKeyBlob, privateKeyBlobLen, 0);
      if (status != SEC_E_OK) {
         test_error ("Failed to import key: %s", mongoc_winerr_to_string (status));
      }

      // Check that imported key is not persisted (because no key name given):
      ASSERT_CMPSIZE_T (key_count_capi, ==, count_capi_keys ());
      ASSERT_CMPSIZE_T (key_count_cng, ==, count_cng_keys ());

      // Create certificate:
      const char *pem_public = strstr (pem, "-----BEGIN CERTIFICATE-----");
      ASSERT (pem_public);
      DWORD encoded_cert_len;
      LPBYTE encoded_cert = decode_pem_base64 (pem_public, &encoded_cert_len, "public key", CLOUD_DEV_CERT);
      ASSERT (encoded_cert);
      PCCERT_CONTEXT cert = CertCreateCertificateContext (X509_ASN_ENCODING, encoded_cert, encoded_cert_len);
      ASSERT (cert);

      // Add certificate to an in-memory certificate store
      HCERTSTORE hStore = CertOpenStore (CERT_STORE_PROV_MEMORY, 0, 0, CERT_STORE_CREATE_NEW_FLAG, NULL);
      ASSERT (hStore);
      ASSERT (CertAddCertificateContextToStore (hStore, cert, CERT_STORE_ADD_REPLACE_EXISTING, NULL));

      // Get the certificate from the store to attach the private key to the cert in the store
      {
         PCCERT_CONTEXT stored_cert = CertEnumCertificatesInStore (hStore, NULL);
         ASSERT (stored_cert);
         // Set private key on cert.
         // Note: setting CERT_NCRYPT_KEY_HANDLE_PROP_ID on the cert context for SChannel results in: W(0x8009030E) No credentials are available in the security package
         // Maybe PKCS#12 export attaches name?
         //ok = CertSetCertificateContextProperty (stored_cert, CERT_NCRYPT_KEY_HANDLE_PROP_ID, 0, &hKey);
         //ASSERT (ok);
      }

      // Export in-memory cert store as PKCS#12, then import and persist:
      {
         CRYPT_DATA_BLOB pfxBlob = {0};
         ok =
            PFXExportCertStoreEx (hStore,
                                  &pfxBlob,
                                  L"",
                                  0,
                                  EXPORT_PRIVATE_KEYS | REPORT_NO_PRIVATE_KEY | REPORT_NOT_ABLE_TO_EXPORT_PRIVATE_KEY);
         if (!ok) {
            test_error ("Failed to export PKCS#12 key: %s", mongoc_winerr_to_string (GetLastError ()));
         }

         pfxBlob.pbData = (BYTE *) bson_malloc (pfxBlob.cbData);

         ok =
            PFXExportCertStoreEx (hStore,
                                  &pfxBlob,
                                  L"",
                                  0,
                                  EXPORT_PRIVATE_KEYS | REPORT_NO_PRIVATE_KEY | REPORT_NOT_ABLE_TO_EXPORT_PRIVATE_KEY);
         if (!ok) {
            test_error ("Failed to export PKCS#12 key: %s", mongoc_winerr_to_string (GetLastError ()));
         }

         HCERTSTORE cert_store = PFXImportCertStore (&pfxBlob, L"", PKCS12_ALWAYS_CNG_KSP);
         ASSERT (cert_store);
      }


      // Key is imported into CNG:
      ASSERT_CMPSIZE_T (key_count_capi, ==, count_capi_keys ());
      ASSERT_CMPSIZE_T (key_count_cng + 1, ==, count_cng_keys ());
   }

   // Given a certificate thumbprint, load from the cert store:
   {
      uint32_t len;
      uint8_t *hash = hex_to_bin ("5820938b9431d5caf375d415bfe3d21ac3d47c07", &len);
      ASSERT (hash);
      CRYPT_HASH_BLOB hashBlob = {.cbData = (DWORD) len, .pbData = (BYTE *) hash};

      DWORD storeType = CERT_SYSTEM_STORE_LOCAL_MACHINE;

      HCERTSTORE store =
         CertOpenStore (CERT_STORE_PROV_SYSTEM,
                        0,
                        0,
                        storeType | CERT_STORE_DEFER_CLOSE_UNTIL_LAST_FREE_FLAG | CERT_STORE_READONLY_FLAG,
                        L"My");
      ASSERT (store);

      
      PCCERT_CONTEXT cert = CertFindCertificateInStore (
         store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_HASH, &hashBlob, NULL);
      ASSERT (cert);

      
      bson_error_t error;
      ASSERT_OR_PRINT (try_connect (CLOUD_PROD_HOST, cert, &error), error);
      
      CertCloseStore (store, 0);
   }
}

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

   TestSuite_Add (suite, "/C5998", test_certs);
}
