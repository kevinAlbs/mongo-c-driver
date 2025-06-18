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
   ASSERT (stream);

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

static void test_certs (void) {
   char *CLOUD_PROD_HOST = test_framework_getenv ("CLOUD_PROD_HOST");
   char *CLOUD_PROD_CERT = test_framework_getenv ("CLOUD_PROD_CERT");

   char *CLOUD_DEV_HOST = test_framework_getenv ("CLOUD_DEV_HOST");
   char *CLOUD_DEV_CERT = test_framework_getenv ("CLOUD_DEV_CERT");
   char *CLOUD_DEV_CERT_PKCS12 = test_framework_getenv ("CLOUD_DEV_CERT_PKCS12");

   
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
      extern char *read_file_and_null_terminate (const char *filename, size_t *out_len);

      size_t out_len;
      char* blob = read_file_and_null_terminate (CLOUD_DEV_CERT_PKCS12, &out_len);
      ASSERT (blob);
      ASSERT (mlib_in_range (DWORD, out_len));
      CRYPT_DATA_BLOB datablob = {.cbData = out_len, .pbData = blob};
      // Assume empty password for now.
      LPWSTR pwd = L"";
      HCERTSTORE cert_store = PFXImportCertStore (&datablob, pwd, 0);

      // Imports as a CAPI key:
      ASSERT_CMPSIZE_T (key_count_capi + 1, ==, count_capi_keys ());
      ASSERT_CMPSIZE_T (key_count_cng, ==, count_cng_keys ());

      PCCERT_CONTEXT cert = CertFindCertificateInStore (
         cert_store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_HAS_PRIVATE_KEY, NULL, NULL);

      ASSERT (cert);      
      ASSERT_OR_PRINT (try_connect (CLOUD_PROD_HOST, CertDuplicateCertificateContext (cert), &error), error);


      // Delete imported key:
      {
         DWORD count = 0; // size contained in pcbData often exceeds the size of the base structure.
         ASSERT (CertGetCertificateContextProperty (cert, CERT_KEY_PROV_INFO_PROP_ID, NULL, &count));
         uint8_t *bytes = bson_malloc0 (count);
         ASSERT (CertGetCertificateContextProperty (cert, CERT_KEY_PROV_INFO_PROP_ID, bytes, &count));
         PCRYPT_KEY_PROV_INFO keyProviderInfo = bytes;

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

   // TODO: try to import PEM in CAPI and persist. Check if this fixes.

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
