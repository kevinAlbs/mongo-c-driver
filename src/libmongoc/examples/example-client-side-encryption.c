/*
To run, set these environment variables:
AWS_ACCESS_KEY_ID
AWS_SECRET_ACCESS_KEY
*/

#include <mongoc/mongoc.h>
#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char *argv[])
{
   mongoc_client_t *client;
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor = NULL;
   bson_error_t error;
   bson_t schema;
   bson_t *to_insert, *filter;
   const bson_t *to_find;
   bson_t *kms_providers;
   mongoc_auto_encryption_opts_t *auto_encryption_opts;
   char *aws_access_key_id;
   char *aws_secret_access_key;
   char *str;
   const char* schema_str;
   bson_t *extra;

   mongoc_init ();
   client = mongoc_client_new ("mongodb://localhost:27017");
   mongoc_client_set_error_api (client, 2);
   
   /* configure automatic encryption/decryption. */
   auto_encryption_opts = mongoc_auto_encryption_opts_new ();

   /* set key vault namespace to admin.datakeys */
   mongoc_auto_encryption_opts_set_key_vault_namespace (auto_encryption_opts, "admin", "datakeys");

   /* set KMS provider for "aws" */
   aws_access_key_id = getenv ("AWS_ACCESS_KEY_ID");
   aws_secret_access_key = getenv ("AWS_SECRET_ACCESS_KEY");
   kms_providers = BCON_NEW ("aws", "{", "secretAccessKey", BCON_UTF8(aws_secret_access_key), "accessKeyId", BCON_UTF8(aws_access_key_id), "}");
   mongoc_auto_encryption_opts_set_kms_providers (auto_encryption_opts, kms_providers);

   /* set a local JSON schema for "test.test" */
   schema_str = "{ \"test.test\": {"
    "  \"properties\": {"
    "    \"encrypted_string\": {"
    "      \"encrypt\": {"
    "        \"keyId\": ["
    "          {"
    "            \"$binary\": {"
    "              \"base64\": \"AAAAAAAAAAAAAAAAAAAAAA==\","
    "              \"subType\": \"04\""
    "            }"
    "          }"
    "        ],"
    "        \"bsonType\": \"string\","
    "        \"algorithm\": \"AEAD_AES_256_CBC_HMAC_SHA_512-Deterministic\""
    "      }"
    "    }"
    "  },"
    "  \"bsonType\": \"object\""
    "}}";

   if (!bson_init_from_json (&schema, schema_str, strlen (schema_str), &error)) {
       fprintf (stderr, "could not parse JSON: %s\n", error.message);
   }
   
   extra = BCON_NEW ("mongocryptdSpawnArgs", "[", "--logpath", "./logs.txt", "--idleShutdownTimeoutSecs=120", "]");
   mongoc_auto_encryption_opts_set_extra (auto_encryption_opts, extra);
   mongoc_auto_encryption_opts_set_schema_map (auto_encryption_opts, &schema);
   
   if (!mongoc_client_enable_auto_encryption (client, auto_encryption_opts, &error)) {
      fprintf (stderr, "error=%s\n", error.message);
      return EXIT_FAILURE;
   }

   /* Insert should undergo auto encryption */
   to_insert = BCON_NEW ("encrypted_string", BCON_UTF8("hello world"));
   collection = mongoc_client_get_collection (client, "test", "test");

   str = bson_as_json (to_insert, NULL);
   printf ("inserting %s into test.test\n", str);
   bson_free (str);

   if (!mongoc_collection_insert_one (
          collection, to_insert, NULL /* opts */, NULL /* reply */, &error)) {
      fprintf (stderr, "Insert failed: %s\n", error.message);
      return EXIT_FAILURE;
   }

   /* find everything */
   filter = bson_new();
   cursor = mongoc_collection_find_with_opts (
      collection,
      filter,
      NULL /* opts */,
      NULL /* read prefs */);

   printf ("found: \n");
   while (mongoc_cursor_next (cursor, &to_find)) {
      str = bson_as_json (to_find, NULL);
      printf("- %s\n", str);
      bson_free (str);
   }

   if (mongoc_cursor_error (cursor, &error)) {
      fprintf (stderr, "Cursor Failure: %s\n", error.message);
      return EXIT_FAILURE;
   }

   bson_destroy (extra);
   bson_destroy (to_insert);
   bson_destroy (filter);
   bson_destroy (kms_providers);
   bson_destroy (&schema);
   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (collection);
   mongoc_auto_encryption_opts_destroy (auto_encryption_opts);
   mongoc_client_destroy (client);
   mongoc_cleanup ();

   return EXIT_SUCCESS;
}
