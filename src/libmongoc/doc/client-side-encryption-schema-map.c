#include <mongoc/mongoc.h>
#include <stdio.h>
#include <stdlib.h>

static bool create_json_schema_file(bson_t *kms_providers,
    const char* keyvault_namespace,
    mongoc_client_t* keyvault_client,
    bson_error_t *error) {

    mongoc_client_encryption_t *client_encryption  = NULL;
    mongoc_client_encryption_opts_t *client_encryption_opts  = NULL;
    mongoc_client_encryption_datakey_opts_t *datakey_opts  = NULL;
    bson_value_t datakey_id = {0};
    char *keyaltnames[] = { "mongoc_encryption_example_1" };
    bson_t *schema =  NULL;
    char *json_schema_string = NULL;
    size_t json_schema_string_len;
    FILE* outfile = NULL;
    bool ret = false;

    client_encryption_opts = mongoc_client_encryption_datakey_opts_new ();
    mongoc_client_encryption_opts_set_kms_providers (client_encryption_opts, kms_providers);
    mongoc_client_encryption_opts_set_keyvault_namespace (client_encryption_opts, keyvault_namespace);
    mongoc_client_encryption_opts_set_keyvault_client (client_encryption_opts, keyvault_client);
    
    client_encryption  = mongoc_client_encryption_new (client_encryption_opts, error);
    if (!client_encryption) {
        goto fail;
    }
    
    /* Create a new data key and json schema for the encryptedField.
     * https://dochub.mongodb.org/core/client-side-field-level-encryption-automatic-encryption-rules
     */
    datakey_opts = mongoc_client_encryption_datakey_opts_new ();
    mongoc_client_encryption_datakey_opts_set_keyaltnames (datakey_opts, keyaltnames, 1);
    if (!mongoc_client_encryption_create_datakey (client_encryption, "local", datakey_opts, &datakey_id, error)) {
        goto fail;
    }
   
    schema = BCON_NEW (
        "properties", "{",
            "encryptedField", "{",
                "encrypt", "{",
                    "keyId", "[", BCON_VALUE(datakey_id), "]",
                    "bsonType", "string",
                    "algorithm", AEAD_AES_256_CBC_HMAC_SHA_512_DETERMINISTIC,
                "}",
            "}",
        "}",
        "bsonType", "object"
    );
    /* Use canonical JSON so that other drivers and tools will be
     * able to parse the MongoDB extended JSON file. */
    json_schema_string = bson_as_canonical_extended_json (schema, &json_schema_string_len);
    outfile = fopen ("jsonSchema.json", "w");
    if (0 == fwrite (json_schema_string, sizeof(char), json_schema_string_len, outfile)) {
        fprintf (stderr, "failed to write to file\n");
        goto fail;
    }
    
    ret = true;
fail:
    mongoc_client_encryption_destroy (client_encryption);
    mongoc_client_encryption_datakey_opts_destroy (datakey_opts);
    mongoc_client_encryption_opts_destroy (client_encryption_opts);
    bson_free (json_schema_string);
    bson_destroy (schema);
    if (outfile) {
        fclose (outfile);
    }
    return true;
}

static uint8_t* hex_to_bin (const char* hex, uint32_t* len) {
    int i;
    int hex_len;
    uint8_t *out;

    hex_len  = strlen  (hex);
    if (hex_len % 2 != 0) {
        return NULL;
    }

    out = bson_malloc0 (hex_len / 2);

    for (i = 0; i < hex_len; i+= 2) {
        uint8_t hex_char;

        if (1 != sscanf ("%2x", hex + i, &hex_char)) {
            bson_free (out);
            return NULL;
        }
        out [i / 2] = hex_char;
    }
    return out;
}

static bool print_one (mongoc_collection_t *coll, bson_error_t *error) {
    bool ret = false;
    mongoc_cursor_t *cursor = NULL;
    const bson_t *found;
    bson_t *filter = NULL;
    char *as_string = NULL;
    
    filter = bson_new ();
    cursor = mongoc_collection_find_with_opts (coll, filter, NULL /* opts  */,  NULL /* read prefs */);
    if (!mongoc_cursor_next (cursor, &found)) {
        fprintf (stderr, "error: did not find inserted document\n");
        goto fail;
    }
    if (mongoc_cursor_error (cursor, error)) {
        goto fail;
    }
    as_string = bson_as_canonical_extended_json (found, NULL);
    printf ("%s", as_string);

    ret = true;
fail:
    bson_destroy (filter);
    mongoc_cursor_destroy (cursor);
    bson_free (as_string);
    return ret;
}

int main (int argc, char** argv) {
    /* The MongoDB namespace (db.collection) used to store
     * the encryption data keys. */
    #define KEYVAULT_DB "encryption"
    #define KEYVAULT_COLL "__libmongocTestKeyVault"

    #define ENCRYPTED_DB "encryption"
    #define ENCRYPTED_COLL "__libmongocTestKeyVault"

    int exit_status = EXIT_FAILURE;
    bool ret;

    /* The MongoDB namespace (db.collection) used to store the
     * encrypted documents in this example. */
    const char* encrypted_namespace = "test.coll";
    uint8_t* local_masterkey;
    uint32_t local_masterkey_len;
    bson_t *kms_providers;
    bson_error_t error = {0};

    /* The MongoClient used to access the key vault (keyvault_namespace). */
    mongoc_client_t *keyvault_client;
    mongoc_collection_t *keyvault_coll;

    /* This must be the same master key that was used to create
     * the encryption key. */
    local_masterkey = hex_to_bin (getenv ("LOCAL_MASTERKEY"), &local_masterkey_len);
    if (!local_masterkey || local_masterkey_len != 96) {
      fprintf (stderr,
               "Specify LOCAL_MASTERKEY environment variable as a "
               "secure random 96 byte hex value.\n");
      goto fail;
   }

    kms_providers = BCON_NEW ("local",
                             "{",
                             "key",
                             BCON_BIN (0, local_masterkey, local_masterkey_len),
                             "}");

    keyvault_client = mongoc_client_new ("mongodb://localhost/?appname=client-side-encryption-keyvault");
    keyvault_coll = mongoc_client_get_collection (keyvault_client, KEYVAULT_DB, KEYVAULT_COLL);
    mongoc_collection_drop (keyvault_coll, NULL);

    /* Ensure that two data keys cannot share the same keyAltName. */
    bson_t *index_keys;
    char *index_name;
    bson_t *create_index_cmd;
    index_keys = BCON_NEW ("keyAltNames", BCON_INT32(1));
    index_name = mongoc_collection_keys_to_index_string (index_keys);
    create_index_cmd = BCON_NEW ("createIndexes", KEYVAULT_COLL, "indexes", "[", "{", "key", BCON_DOCUMENT (index_keys), "name", index_name, "unique", BCON_BOOL (true), "partialFilterExpression", "{", "keyAltNames", "{", "$exists", BCON_BOOL(true), "}", "}");
    ret =  mongoc_client_command_simple (keyvault_client, KEYVAULT_DB, create_index_cmd, NULL /* read prefs */, NULL /* reply */, &error);
    
    if (!ret) {
        goto fail;
    }

    ret = create_json_schema_file(
        kms_providers, KEYVAULT_DB "." KEYVAULT_COLL, keyvault_client, &error);
    
    if (!ret) {
        goto fail;
    }

    bson_json_reader_t *reader;
    reader = bson_json_reader_new_from_file ("jsonSchema.json", &error);
    if (!reader)  {
        goto fail;
    }

    bson_t schema;
    bson_json_reader_read (reader, &schema, &error);

    bson_t *schema_map;
    schema_map = BCON_NEW (BCON_UTF8 (encrypted_namespace), BCON_DOCUMENT (&schema));

    mongoc_auto_encryption_opts_t *auto_encryption_opts;
    auto_encryption_opts = mongoc_auto_encryption_opts_new ();
    mongoc_auto_encryption_opts_set_keyvault_client (auto_encryption_opts, keyvault_client);
    mongoc_auto_encryption_opts_set_keyvault_namespace (auto_encryption_opts, KEYVAULT_DB  "." KEYVAULT_COLL);
    mongoc_auto_encryption_opts_set_kms_providers (auto_encryption_opts, kms_providers);

    mongoc_client_t *client = NULL;
    client = mongoc_client_new ("mongodb://localhost/?appname=client-side-encryption");
    ret = mongoc_client_enable_auto_encryption (client, auto_encryption_opts, &error);
    if (!ret) {
        goto fail;
    }

    mongoc_collection_t *coll = NULL;
    coll  = mongoc_client_get_collection (client, ENCRYPTED_DB, ENCRYPTED_COLL);
    /* Clear old data */
    mongoc_collection_drop (coll, NULL);

    bson_t *to_insert;
    to_insert = BCON_NEW ("encryptedField", "123456789");
    ret = mongoc_collection_insert_one (coll, to_insert, NULL /* opts */, NULL /* reply */, &error);
    if (!ret) {
        goto fail;
    }
    printf ("decrypted document: ");
    if (!print_one (coll, &error)) {
        goto fail;
    }
    printf ("\n");

    mongoc_client_t *unencrypted_client = NULL;
    mongoc_collection_t *unencrypted_coll = NULL;

    unencrypted_client = mongoc_client_new ("mongodb://localhost/?appname=client-side-encryption-unencrypted");
    unencrypted_coll = mongoc_client_get_collection (unencrypted_client, ENCRYPTED_DB, ENCRYPTED_COLL);
    printf ("encrypted document: ");
    if (!print_one (unencrypted_coll, &error)) {
        goto fail;
    }
    printf ("\n");

    exit_status = EXIT_SUCCESS;
fail:
    if (error.code)  {
        fprintf ("error: %s\n", error.message);
    }
    return  exit_status;
}