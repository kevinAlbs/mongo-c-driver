/*
To compile and run these tests:

# Check out this branch:
git clone git@github.com:kevinAlbs/mongo-c-driver.git \
   --branch W13533 \
   mongo-c-driver-W13533

# Configure with `cmake`. This may require `brew install cmake` on macOS:
cd mongo-c-driver-W13533
cmake -Bcmake-build -S.

# Build with `cmake`:
cmake --build cmake-build --target test-libmongoc

# Run test:
./cmake-build/src/libmongoc/test-libmongoc \
   -d --no-fork \
   --match "/bulk_write/old/works"
# `-d` prints debug logs.
# `--no-fork` runs the test in the same process to help attaching a debugger.

Set the environment variable `PRINT_COMMAND_STARTED=on` to print command started
events.

*/

#include <mongoc/mongoc-collection.h>

#include "test-conveniences.h"
#include "TestSuite.h"
#include "test-libmongoc.h" // test_framework_getenv

static void
command_started (const mongoc_apm_command_started_t *event)
{
   char *s;

   s = bson_as_relaxed_extended_json (
      mongoc_apm_command_started_get_command (event), NULL);
   MONGOC_DEBUG ("Command %s started on %s: %s",
                 mongoc_apm_command_started_get_command_name (event),
                 mongoc_apm_command_started_get_host (event)->host,
                 s);

   bson_free (s);
}

static void
print_started_events (mongoc_client_t *client)
{
   mongoc_apm_callbacks_t *cbs = mongoc_apm_callbacks_new ();

   mongoc_apm_set_command_started_cb (cbs, command_started);
   mongoc_client_set_apm_callbacks (client, cbs, NULL /* context */);
   mongoc_apm_callbacks_destroy (cbs);
}

#define ASSERT_COLLECTION_CONTAINS(coll, ...)                                \
   if (1) {                                                                  \
      bson_t *_expect[] = {__VA_ARGS__};                                     \
      mongoc_cursor_t *_cur = mongoc_collection_find_with_opts (             \
         coll, tmp_bson ("{}"), NULL /* opts */, NULL /* read prefs */);     \
      const bson_t *_got;                                                    \
      for (size_t i = 0; i < sizeof (_expect) / sizeof (_expect[0]); i++) {  \
         if (!mongoc_cursor_next (_cur, &_got)) {                            \
            test_error ("expected document in collection for `%s`, but got " \
                        "end of cursor",                                     \
                        tmp_json (_expect[i]));                              \
         }                                                                   \
         assert_match_bson (_got, _expect[i], false /* is_command */);       \
      }                                                                      \
      bson_error_t _error;                                                   \
      ASSERT_OR_PRINT (!mongoc_cursor_error (_cur, &_error), _error);        \
      if (mongoc_cursor_next (_cur, &_got)) {                                \
         test_error ("unexpected extra document in collection: `%s`",        \
                     tmp_json (_got));                                       \
      }                                                                      \
   } else                                                                    \
      (void) 0

// Print command started events.

static void
test_bulk_write_old_works (void)
{
   mongoc_client_t *client = test_framework_new_default_client ();

   if (test_framework_getenv_bool ("PRINT_COMMAND_STARTED")) {
      print_started_events (client);
   }

   mongoc_collection_t *coll =
      mongoc_client_get_collection (client, "db", "coll");

   bson_error_t error;

   // Test a single insert.
   {
      // Drop prior data.
      mongoc_collection_drop (coll, NULL /* ignore error */);

      mongoc_bulk_operation_t *bulk =
         mongoc_collection_create_bulk_operation_with_opts (coll,
                                                            NULL /* opts */);

      mongoc_bulk_operation_insert (bulk, tmp_bson ("{'x': 1}"));
      uint32_t got =
         mongoc_bulk_operation_execute (bulk, NULL /* reply */, &error);
      ASSERT_OR_PRINT (got, error);

      ASSERT_COLLECTION_CONTAINS (coll, tmp_bson ("{'x': 1}"));

      mongoc_bulk_operation_destroy (bulk);
   }

   // Test a mix of operations.
   {
      // Drop prior data.
      mongoc_collection_drop (coll, NULL /* ignore error */);

      mongoc_bulk_operation_t *bulk =
         mongoc_collection_create_bulk_operation_with_opts (coll,
                                                            NULL /* opts */);

      mongoc_bulk_operation_insert (bulk, tmp_bson ("{'x': 1}"));
      mongoc_bulk_operation_insert (bulk, tmp_bson ("{'y': 1}"));
      mongoc_bulk_operation_update_one (bulk,
                                        tmp_bson ("{'x': 1}"),
                                        tmp_bson ("{'$set': {'x': 2}}"),
                                        false /* upsert */);
      mongoc_bulk_operation_remove_one (bulk, tmp_bson ("{'y': 1}"));
      mongoc_bulk_operation_insert (bulk, tmp_bson ("{'x': 1}"));

      uint32_t got =
         mongoc_bulk_operation_execute (bulk, NULL /* reply */, &error);
      ASSERT_OR_PRINT (got, error);

      ASSERT_COLLECTION_CONTAINS (coll, tmp_bson ("{'x': 2}"));

      mongoc_bulk_operation_destroy (bulk);
   }

   mongoc_collection_destroy (coll);
   mongoc_client_destroy (client);
}

void
test_bulk_write_install (TestSuite *suite)
{
   TestSuite_AddLive (
      suite, "/bulk_write/old/works", test_bulk_write_old_works);
}
