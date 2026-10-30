// Usage:
//   MONGODB_URI='mongodb://<user>:<password>@<host>:<port>/' ./hello-mongoac
//
// Sends a "ping" command using libmongoac.

#include <mongoac/client.h>
#include <mongoac/database.h>
#include <mongoac/error.h>
#include <mongoac/future.h>
#include <mongoac/runtime.h>

#include <bson/bson.h> // libbson is used to build the command document.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Construct a `mongoac_string_view_t` from a NUL-terminated string.
#define MONGOAC_STRING_VIEW(str) ((mongoac_string_view_t){(str), strlen(str)})

int main(void) {
    char const* uri = getenv("MONGODB_URI");
    if (!uri || !*uri) {
        uri = "mongodb://localhost:27017";
    }
    int exit_code = EXIT_FAILURE;

    bson_t* ping = NULL;
    mongoac_error_t* error = NULL;
    mongoac_client_t* client = NULL;
    mongoac_database_t* db = NULL;
    mongoac_runtime_t* runtime = NULL;
    mongoac_future_t* future = NULL;

    error = mongoac_error_new();

    printf("Sending ping from libmongoac ... ");
    fflush(stdout);

    // Connect using only a connection string.
    client = mongoac_client_new(MONGOAC_STRING_VIEW(uri), error);
    if (!client) {
        goto fail;
    }

    db = mongoac_client_get_database(client, MONGOAC_STRING_VIEW("admin"), NULL);
    runtime = mongoac_database_get_runtime(db);

    // Build the `{ping: 1}` command with libbson.
    ping = bson_new();
    BSON_APPEND_INT32(ping, "ping", 1);

    // Begin the operation. Returns NULL on invalid arguments.
    future = mongoac_database_run_command_async(db, NULL, (mongoac_bson_view_t){bson_get_data(ping), ping->len}, NULL);
    if (!future) {
        goto fail;
    }

    // Block until the operation completes or fails.
    mongoac_runtime_block_on(runtime, future, error);
    if (mongoac_error_code(error) != MONGOAC_ERROR_CODE_OK) {
        goto fail;
    }

    // Fetch the reply to surface any server error. The reply view remains
    // valid until the future is destroyed.
    mongoac_bson_view_t const reply = mongoac_future_get_bson(future, error);
    if (mongoac_error_code(error) != MONGOAC_ERROR_CODE_OK) {
        goto fail;
    }

    printf("OK!\n");
    exit_code = EXIT_SUCCESS;
    goto cleanup;

fail:
    printf("ERROR\n");
    fflush(stdout);
    if (mongoac_error_code(error) != MONGOAC_ERROR_CODE_OK) {
        mongoac_string_t const message = mongoac_error_message(error);
        fprintf(
            stderr,
            "error: %.*s (category=%d, code=%d)\n",
            (int)message.len,
            message.ptr,
            mongoac_error_category(error),
            mongoac_error_code(error));
        mongoac_string_destroy(message);
    }

cleanup:

    bson_destroy(ping);
    mongoac_future_destroy(future);
    mongoac_runtime_destroy(runtime);
    mongoac_database_destroy(db);
    mongoac_client_destroy(client);
    mongoac_error_destroy(error);
    return exit_code;
}
