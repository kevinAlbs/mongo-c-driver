#include <mongoc/mongoc-rust-private.h>

#include <mongoc/mongoc.h>

#include <bson/bson.h>

#include <common-thread-private.h>
#include <TestSuite.h>
#include <test-conveniences.h>
#include <test-libmongoc.h>

#include <stdint.h>
#include <string.h>

static void
test_sanity_check(void)
{
   ASSERT_CMPINT32(mongoc_async_sanity_check(1), ==, 1);
   ASSERT_CMPINT32(mongoc_async_sanity_check(2), ==, 2);
   ASSERT_CMPINT32(mongoc_async_sanity_check(3), ==, 3);
}

static void
test_client_new_valid(void)
{
   capture_logs(true);

   mongoc_async_client_t *const client = mongoc_async_client_new("mongodb://localhost:27017");

   ASSERT(client);
   ASSERT_NO_CAPTURED_LOGS("client");

   mongoc_async_client_destroy(client);

   capture_logs(false);
}

static void
test_client_new_invalid(void)
{
   capture_logs(true);

   mongoc_async_client_t *const client = mongoc_async_client_new("invalid");

   ASSERT(!client);
   ASSERT_CAPTURED_LOG("client", MONGOC_LOG_LEVEL_ERROR, "connection string contains no scheme");

   mongoc_async_client_destroy(client);

   capture_logs(false);
}

static void
test_client_get_database(void)
{
   mongoc_async_client_t *const client = mongoc_async_client_new("mongodb://localhost:27017");
   ASSERT(client);

   {
      mongoc_async_database_t *const db = mongoc_async_client_get_database(client, "admin");
      ASSERT(db);
      ASSERT_CMPSTR(mongoc_async_database_get_name(db), "admin");
      mongoc_async_database_destroy(db);
   }

   mongoc_async_client_destroy(client);
}

static void
test_database_get_name(void)
{
   mongoc_async_client_t *const client = mongoc_async_client_new("mongodb://localhost:27017");
   ASSERT(client);

   char const *names[] = {"", "db", "test", NULL};

   for (char const **iter = names; *iter; ++iter) {
      char const *const name = *iter;

      mongoc_async_database_t *const db = mongoc_async_client_get_database(client, name);

      ASSERT(db);
      ASSERT_CMPSTR(mongoc_async_database_get_name(db), name);

      mongoc_async_database_destroy(db);
   }

   mongoc_async_client_destroy(client);
}

static void
test_database_drop(void)
{
   mongoc_async_client_t *const client = mongoc_async_client_new("mongodb://localhost:27017");
   ASSERT(client);
   mongoc_async_database_t *const db = mongoc_async_client_get_database(client, "test");
   ASSERT(db);

   capture_logs(true);
   ASSERT(mongoc_async_database_drop_await(db));
   ASSERT(mongoc_async_database_drop_await(db));
   ASSERT(mongoc_async_database_drop_await(db));
   ASSERT_NO_CAPTURED_LOGS("database");
   capture_logs(false);

   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_database_get_collection(void)
{
   mongoc_async_client_t *const client = mongoc_async_client_new("mongodb://localhost:27017");
   ASSERT(client);

   mongoc_async_database_t *const db = mongoc_async_client_get_database(client, "db");
   ASSERT(db);

   {
      mongoc_async_collection_t *const coll = mongoc_async_database_get_collection(db, "collection");
      ASSERT(coll);
      mongoc_async_collection_destroy(coll);
   }

   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_database_create_collection(void)
{
   mongoc_async_client_t *const client = mongoc_async_client_new("mongodb://localhost:27017");
   ASSERT(client);

   mongoc_async_database_t *const db = mongoc_async_client_get_database(client, "db");
   ASSERT(db);

   mongoc_async_database_drop_await(db);

   {
      capture_logs(true);

      char **const names = mongoc_async_database_get_collection_names_with_opts_await(db);
      ASSERT(names);
      ASSERT_CMPSTR(names[0], NULL);

      bson_strfreev(names);

      ASSERT_NO_CAPTURED_LOGS("database");

      capture_logs(false);
   }

   {
      mongoc_async_collection_t *const coll = mongoc_async_database_create_collection_await(db, "coll");
      ASSERT(coll);
      mongoc_async_collection_destroy(coll);
   }

   {
      capture_logs(true);

      char **const names = mongoc_async_database_get_collection_names_with_opts_await(db);
      ASSERT(names);
      ASSERT_CMPSTR(names[0], "coll");
      ASSERT_CMPSTR(names[1], NULL);

      bson_strfreev(names);

      ASSERT_NO_CAPTURED_LOGS("database");

      capture_logs(false);
   }

   {
      mongoc_async_collection_t *const coll = mongoc_async_database_create_collection_await(db, "test");
      ASSERT(coll);
      mongoc_async_collection_destroy(coll);
   }

   {
      capture_logs(true);

      char **const names = mongoc_async_database_get_collection_names_with_opts_await(db);
      ASSERT(names);
      ASSERT_CMPSTR(names[0], "coll");
      ASSERT_CMPSTR(names[1], "test");
      ASSERT_CMPSTR(names[2], NULL);

      bson_strfreev(names);

      ASSERT_NO_CAPTURED_LOGS("database");

      capture_logs(false);
   }

   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_get_name(void)
{
   mongoc_async_client_t *const client = mongoc_async_client_new("mongodb://localhost:27017");
   ASSERT(client);

   mongoc_async_database_t *const db = mongoc_async_client_get_database(client, "db");
   ASSERT(db);

   char const *names[] = {"", "coll", "test", NULL};

   for (char const **iter = names; *iter; ++iter) {
      char const *const name = *iter;

      mongoc_async_collection_t *const coll = mongoc_async_database_get_collection(db, name);
      ASSERT(coll);
      ASSERT_CMPSTR(mongoc_async_collection_get_name(coll), name);

      mongoc_async_collection_destroy(coll);
   }

   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_drop(void)
{
   mongoc_async_client_t *const client = mongoc_async_client_new("mongodb://localhost:27017");
   ASSERT(client);
   mongoc_async_database_t *const db = mongoc_async_client_get_database(client, "test");
   ASSERT(db);
   mongoc_async_collection_t *const coll = mongoc_async_database_get_collection(db, "test");
   ASSERT(coll);

   capture_logs(true);
   ASSERT(mongoc_async_collection_drop_await(coll));
   ASSERT(mongoc_async_collection_drop_await(coll));
   ASSERT(mongoc_async_collection_drop_await(coll));
   ASSERT_NO_CAPTURED_LOGS("collection");
   capture_logs(false);

   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_count_documents(void)
{
   mongoc_async_client_t *const client = mongoc_async_client_new("mongodb://localhost:27017");
   ASSERT(client);
   mongoc_async_database_t *const db = mongoc_async_client_get_database(client, "test");
   ASSERT(db);
   mongoc_async_collection_t *const coll = mongoc_async_database_get_collection(db, "test");
   ASSERT(coll);

   capture_logs(true);
   int64_t const count = mongoc_async_collection_count_documents_await(coll, NULL);
   ASSERT(count >= 0);
   ASSERT_NO_CAPTURED_LOGS("collection");
   capture_logs(false);

   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_database_drop_async(void)
{
   mongoc_async_client_t *const client = mongoc_async_client_new("mongodb://localhost:27017");
   ASSERT(client);

   mongoc_async_database_t *const db = mongoc_async_client_get_database(client, "db");
   ASSERT(db);

   capture_logs(true);
   mongoc_async_collection_destroy(mongoc_async_database_create_collection_await(db, "coll1"));
   mongoc_async_collection_destroy(mongoc_async_database_create_collection_await(db, "coll2"));
   mongoc_async_collection_destroy(mongoc_async_database_create_collection_await(db, "coll3"));
   ASSERT_NO_CAPTURED_LOGS("mongoc_async_database_create_collection_await");
   capture_logs(false);

   {
      mongoc_async_future_t *const future = mongoc_async_database_drop(db);
      ASSERT(future);

      ASSERT(!mongoc_async_future_get_void(future));

      // `mongoc_async_future_wait(future)`
      while (!mongoc_async_future_poll(future)) {
         ASSERT(!mongoc_async_future_get_void(future));

         mongoc_async_future_make_progress(future);
      }

      ASSERT(mongoc_async_future_get_void(future));

      mongoc_async_future_destroy(future);
   }

   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_count_documents_async(void)
{
   mongoc_async_client_t *const client = mongoc_async_client_new("mongodb://localhost:27017");
   ASSERT(client);
   mongoc_async_database_t *const db = mongoc_async_client_get_database(client, "test");
   ASSERT(db);
   mongoc_async_collection_t *const coll = mongoc_async_database_get_collection(db, "test");
   ASSERT(coll);

   {
      mongoc_async_future_t *const future = mongoc_async_collection_count_documents(coll);
      ASSERT(future);

      ASSERT(!mongoc_async_future_get_void(future));

      uint64_t count;

      // `mongoc_async_future_wait(future)`
      while (!mongoc_async_future_poll(future)) {
         ASSERT(!mongoc_async_future_get_uint64(future, &count));

         mongoc_async_future_make_progress(future);
      }

      ASSERT(mongoc_async_future_get_uint64(future, &count));
      ASSERT_CMPUINT64(count, >=, 0);

      mongoc_async_future_destroy(future);
   }

   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_insert_one(void)
{
   // Set-up with Sync client:
   {
      mongoc_client_t *client = test_framework_new_default_client();
      mongoc_collection_t *coll = mongoc_client_get_collection(client, "db", "coll");
      mongoc_collection_destroy(coll);
      mongoc_client_destroy(client);
   }


   // Create BSON view:
   bson_t *doc = tmp_bson(BSON_STR({"from_rust" : "hello"}));
   bson_unowned_t bson_view = {.data = bson_get_data(doc), .len = doc->len};

   // Insert with Async client:
   {
      mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
      mongoc_async_database_t *db = mongoc_async_client_get_database(client, "db");
      mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "coll");

      mongoc_async_future_t *future = mongoc_async_collection_insert_one(coll, bson_view, NULL);
      while (!mongoc_async_future_poll_with_timeout(future, 123)) {
         printf("polling...\n");
         mongoc_async_future_make_progress(future);
      }
      ASSERT(mongoc_async_future_get_void(future));

      mongoc_async_collection_destroy(coll);
      mongoc_async_database_destroy(db);
      mongoc_async_client_destroy(client);
   }

   // Verify with Sync client:
   {
      mongoc_client_t *client = test_framework_new_default_client();
      mongoc_collection_t *coll = mongoc_client_get_collection(client, "db", "coll");
      mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(coll, tmp_bson("{}"), NULL, NULL);
      const bson_t *got = NULL;
      ASSERT(mongoc_cursor_next(cursor, &got));
      ASSERT_MATCH(got, BSON_STR({"from_rust" : "hello"}));
      mongoc_collection_destroy(coll);
      mongoc_client_destroy(client);
   }
}

static void
test_collection_insert_one_error(void)
{
   // Drop collection for a clean state.
   {
      mongoc_client_t *client = test_framework_new_default_client();
      mongoc_collection_t *coll = mongoc_client_get_collection(client, "db", "coll_insert_one_error");
      mongoc_collection_drop(coll, NULL);
      mongoc_collection_destroy(coll);
      mongoc_client_destroy(client);
   }

   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "db");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "coll_insert_one_error");

   bson_t *doc = tmp_bson(BSON_STR({"_id" : "test-id", "from_rust" : "hello"}));
   bson_unowned_t bson_view = {.data = bson_get_data(doc), .len = doc->len};

   // First insert succeeds; no error is set.
   {
      capture_logs(true);
      mongoc_async_error_t *error = NULL;
      ASSERT(mongoc_async_collection_insert_one_await(coll, bson_view, NULL, &error));
      ASSERT(!error);
      ASSERT_NO_CAPTURED_LOGS("collection");
      capture_logs(false);
   }

   // Second insert with the same _id fails with a Write (duplicate key) error.
   {
      capture_logs(true);
      mongoc_async_error_t *error = NULL;
      ASSERT(!mongoc_async_collection_insert_one_await(coll, bson_view, NULL, &error));
      ASSERT(error);
      ASSERT_CMPINT32((int32_t)mongoc_async_error_get_code(error), ==, (int32_t)MONGOC_ASYNC_ERROR_CODE_WRITE);
      mongoc_async_error_destroy(error);
      ASSERT_CAPTURED_LOG("collection", MONGOC_LOG_LEVEL_ERROR, "");
      capture_logs(false);
   }

   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_insert_many(void)
{
   {
      mongoc_client_t *client = test_framework_new_default_client();
      mongoc_collection_t *coll = mongoc_client_get_collection(client, "db", "coll_insert_many");
      mongoc_collection_drop(coll, NULL);
      mongoc_collection_destroy(coll);
      mongoc_client_destroy(client);
   }

   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "db");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "coll_insert_many");

   bson_t *doc1 = tmp_bson(BSON_STR({"x" : 1}));
   bson_t *doc2 = tmp_bson(BSON_STR({"x" : 2}));
   bson_t *doc3 = tmp_bson(BSON_STR({"x" : 3}));
   bson_unowned_t views[3] = {
      {.data = bson_get_data(doc1), .len = doc1->len},
      {.data = bson_get_data(doc2), .len = doc2->len},
      {.data = bson_get_data(doc3), .len = doc3->len},
   };

   uint64_t inserted_count = 0;
   mongoc_async_error_t *error = NULL;
   ASSERT(mongoc_async_collection_insert_many_await(coll, views, 3, NULL, &inserted_count, &error));
   ASSERT(!error);
   ASSERT_CMPUINT64(inserted_count, ==, 3);

   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_find(void)
{
   {
      mongoc_client_t *client = test_framework_new_default_client();
      mongoc_collection_t *coll = mongoc_client_get_collection(client, "db", "coll_find");
      mongoc_collection_drop(coll, NULL);
      mongoc_collection_destroy(coll);
      mongoc_client_destroy(client);
   }

   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "db");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "coll_find");

   bson_t *doc = tmp_bson(BSON_STR({"key" : "find-value"}));
   bson_unowned_t doc_view = {.data = bson_get_data(doc), .len = doc->len};
   ASSERT(mongoc_async_collection_insert_one_await(coll, doc_view, NULL, NULL));

   bson_t *filter = tmp_bson("{}");
   bson_unowned_t filter_view = {.data = bson_get_data(filter), .len = filter->len};
   mongoc_async_error_t *error = NULL;
   mongoc_async_cursor_t *cursor = mongoc_async_collection_find_await(coll, filter_view, NULL, &error);
   ASSERT(cursor);
   ASSERT(!error);

   ASSERT(mongoc_async_cursor_next_await(cursor, &error));
   ASSERT(!error);

   bson_unowned_t current = mongoc_async_cursor_current(cursor);
   bson_t current_bson;
   bson_init_static(&current_bson, (uint8_t const *)current.data, current.len);
   ASSERT_MATCH(&current_bson, BSON_STR({"key" : "find-value"}));

   ASSERT(!mongoc_async_cursor_next_await(cursor, &error));
   ASSERT(!error);

   mongoc_async_cursor_destroy(cursor);
   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_find_one(void)
{
   {
      mongoc_client_t *client = test_framework_new_default_client();
      mongoc_collection_t *coll = mongoc_client_get_collection(client, "db", "coll_find_one");
      mongoc_collection_drop(coll, NULL);
      mongoc_collection_destroy(coll);
      mongoc_client_destroy(client);
   }

   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "db");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "coll_find_one");

   bson_t *doc = tmp_bson(BSON_STR({"_id" : "find-one-id", "val" : 42}));
   bson_unowned_t doc_view = {.data = bson_get_data(doc), .len = doc->len};
   ASSERT(mongoc_async_collection_insert_one_await(coll, doc_view, NULL, NULL));

   // Existing document is found.
   {
      bson_t *filter = tmp_bson(BSON_STR({"_id" : "find-one-id"}));
      bson_unowned_t filter_view = {.data = bson_get_data(filter), .len = filter->len};
      mongoc_async_error_t *error = NULL;
      bson_owned_t *found = mongoc_async_collection_find_one_await(coll, filter_view, NULL, &error);
      ASSERT(found);
      ASSERT(!error);
      bson_unowned_t view = bson_owned_as_view(found);
      bson_t bson;
      bson_init_static(&bson, (uint8_t const *)view.data, view.len);
      ASSERT_MATCH(&bson, BSON_STR({"val" : 42}));
      bson_owned_destroy(found);
   }

   // Non-existent document returns null without error.
   {
      bson_t *filter = tmp_bson(BSON_STR({"_id" : "no-such-id"}));
      bson_unowned_t filter_view = {.data = bson_get_data(filter), .len = filter->len};
      mongoc_async_error_t *error = NULL;
      bson_owned_t *found = mongoc_async_collection_find_one_await(coll, filter_view, NULL, &error);
      ASSERT(!found);
      ASSERT(!error);
   }

   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_update_one(void)
{
   {
      mongoc_client_t *client = test_framework_new_default_client();
      mongoc_collection_t *coll = mongoc_client_get_collection(client, "db", "coll_update_one");
      mongoc_collection_drop(coll, NULL);
      mongoc_collection_destroy(coll);
      mongoc_client_destroy(client);
   }

   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "db");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "coll_update_one");

   bson_t *doc = tmp_bson(BSON_STR({"_id" : "update-id", "x" : 1}));
   bson_unowned_t doc_view = {.data = bson_get_data(doc), .len = doc->len};
   ASSERT(mongoc_async_collection_insert_one_await(coll, doc_view, NULL, NULL));

   bson_t *filter = tmp_bson(BSON_STR({"_id" : "update-id"}));
   bson_t *update = tmp_bson(BSON_STR({"$set" : {"x" : 2}}));
   bson_unowned_t filter_view = {.data = bson_get_data(filter), .len = filter->len};
   bson_unowned_t update_view = {.data = bson_get_data(update), .len = update->len};

   uint64_t matched = 0, modified = 0;
   mongoc_async_error_t *error = NULL;
   ASSERT(mongoc_async_collection_update_one_await(coll, filter_view, update_view, NULL, &matched, &modified, &error));
   ASSERT(!error);
   ASSERT_CMPUINT64(matched, ==, 1);
   ASSERT_CMPUINT64(modified, ==, 1);

   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_replace_one(void)
{
   {
      mongoc_client_t *client = test_framework_new_default_client();
      mongoc_collection_t *coll = mongoc_client_get_collection(client, "db", "coll_replace_one");
      mongoc_collection_drop(coll, NULL);
      mongoc_collection_destroy(coll);
      mongoc_client_destroy(client);
   }

   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "db");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "coll_replace_one");

   bson_t *doc = tmp_bson(BSON_STR({"_id" : "replace-id", "x" : 1}));
   bson_unowned_t doc_view = {.data = bson_get_data(doc), .len = doc->len};
   ASSERT(mongoc_async_collection_insert_one_await(coll, doc_view, NULL, NULL));

   bson_t *filter = tmp_bson(BSON_STR({"_id" : "replace-id"}));
   bson_t *replacement = tmp_bson(BSON_STR({"_id" : "replace-id", "x" : 99}));
   bson_unowned_t filter_view = {.data = bson_get_data(filter), .len = filter->len};
   bson_unowned_t replacement_view = {.data = bson_get_data(replacement), .len = replacement->len};

   uint64_t matched = 0, modified = 0;
   mongoc_async_error_t *error = NULL;
   ASSERT(mongoc_async_collection_replace_one_await(coll, filter_view, replacement_view, NULL, &matched, &modified, &error));
   ASSERT(!error);
   ASSERT_CMPUINT64(matched, ==, 1);
   ASSERT_CMPUINT64(modified, ==, 1);

   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_delete_one(void)
{
   {
      mongoc_client_t *client = test_framework_new_default_client();
      mongoc_collection_t *coll = mongoc_client_get_collection(client, "db", "coll_delete_one");
      mongoc_collection_drop(coll, NULL);
      mongoc_collection_destroy(coll);
      mongoc_client_destroy(client);
   }

   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "db");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "coll_delete_one");

   bson_t *doc = tmp_bson(BSON_STR({"_id" : "delete-id"}));
   bson_unowned_t doc_view = {.data = bson_get_data(doc), .len = doc->len};
   ASSERT(mongoc_async_collection_insert_one_await(coll, doc_view, NULL, NULL));

   bson_t *filter = tmp_bson(BSON_STR({"_id" : "delete-id"}));
   bson_unowned_t filter_view = {.data = bson_get_data(filter), .len = filter->len};

   uint64_t deleted = 0;
   mongoc_async_error_t *error = NULL;
   ASSERT(mongoc_async_collection_delete_one_await(coll, filter_view, NULL, &deleted, &error));
   ASSERT(!error);
   ASSERT_CMPUINT64(deleted, ==, 1);

   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_delete_many(void)
{
   {
      mongoc_client_t *client = test_framework_new_default_client();
      mongoc_collection_t *coll = mongoc_client_get_collection(client, "db", "coll_delete_many");
      mongoc_collection_drop(coll, NULL);
      mongoc_collection_destroy(coll);
      mongoc_client_destroy(client);
   }

   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "db");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "coll_delete_many");

   bson_t *doc1 = tmp_bson(BSON_STR({"tag" : "dm"}));
   bson_t *doc2 = tmp_bson(BSON_STR({"tag" : "dm"}));
   bson_t *doc3 = tmp_bson(BSON_STR({"tag" : "dm"}));
   bson_unowned_t views[3] = {
      {.data = bson_get_data(doc1), .len = doc1->len},
      {.data = bson_get_data(doc2), .len = doc2->len},
      {.data = bson_get_data(doc3), .len = doc3->len},
   };
   ASSERT(mongoc_async_collection_insert_many_await(coll, views, 3, NULL, NULL, NULL));

   bson_t *filter = tmp_bson(BSON_STR({"tag" : "dm"}));
   bson_unowned_t filter_view = {.data = bson_get_data(filter), .len = filter->len};

   uint64_t deleted = 0;
   mongoc_async_error_t *error = NULL;
   ASSERT(mongoc_async_collection_delete_many_await(coll, filter_view, NULL, &deleted, &error));
   ASSERT(!error);
   ASSERT_CMPUINT64(deleted, ==, 3);

   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_distinct(void)
{
   {
      mongoc_client_t *client = test_framework_new_default_client();
      mongoc_collection_t *coll = mongoc_client_get_collection(client, "db", "coll_distinct");
      mongoc_collection_drop(coll, NULL);
      mongoc_collection_destroy(coll);
      mongoc_client_destroy(client);
   }

   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "db");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "coll_distinct");

   // Insert three docs: two with "red", one with "blue".
   bson_t *doc1 = tmp_bson(BSON_STR({"color" : "red"}));
   bson_t *doc2 = tmp_bson(BSON_STR({"color" : "blue"}));
   bson_t *doc3 = tmp_bson(BSON_STR({"color" : "red"}));
   bson_unowned_t views[3] = {
      {.data = bson_get_data(doc1), .len = doc1->len},
      {.data = bson_get_data(doc2), .len = doc2->len},
      {.data = bson_get_data(doc3), .len = doc3->len},
   };
   ASSERT(mongoc_async_collection_insert_many_await(coll, views, 3, NULL, NULL, NULL));

   bson_t *filter = tmp_bson("{}");
   bson_unowned_t filter_view = {.data = bson_get_data(filter), .len = filter->len};

   mongoc_async_error_t *error = NULL;
   bson_owned_t *result = mongoc_async_collection_distinct_await(coll, "color", filter_view, &error);
   ASSERT(result);
   ASSERT(!error);

   bson_unowned_t view = bson_owned_as_view(result);
   bson_t bson;
   bson_init_static(&bson, (uint8_t const *)view.data, view.len);

   // Verify the "values" array has exactly 2 distinct values.
   bson_iter_t iter;
   ASSERT(bson_iter_init_find(&iter, &bson, "values"));
   bson_iter_t arr_iter;
   ASSERT(bson_iter_recurse(&iter, &arr_iter));
   int count = 0;
   while (bson_iter_next(&arr_iter)) {
      count++;
   }
   ASSERT_CMPINT32(count, ==, 2);

   bson_owned_destroy(result);
   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_database_run_command(void)
{
   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "admin");

   bson_t *cmd = tmp_bson(BSON_STR({"ping" : 1}));
   bson_unowned_t cmd_view = {.data = bson_get_data(cmd), .len = cmd->len};

   mongoc_async_error_t *error = NULL;
   bson_owned_t *reply = mongoc_async_database_run_command_await(db, cmd_view, &error);
   ASSERT(reply);
   ASSERT(!error);

   // A successful ping reply always contains "ok".
   bson_unowned_t view = bson_owned_as_view(reply);
   bson_t bson;
   bson_init_static(&bson, (uint8_t const *)view.data, view.len);
   bson_iter_t iter;
   ASSERT(bson_iter_init_find(&iter, &bson, "ok"));

   bson_owned_destroy(reply);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

/* Poll a future to completion (5 s timeout per make_progress call). */
#define AWAIT(future)                              \
   do {                                            \
      while (!mongoc_async_future_poll(future)) {   \
         mongoc_async_future_make_progress(future); \
      }                                            \
   } while (0)

static void
test_collection_insert_many_async(void)
{
   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "test_async");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "insert_many_async");

   bson_t *d0 = tmp_bson(BSON_STR({"x" : 1}));
   bson_t *d1 = tmp_bson(BSON_STR({"x" : 2}));
   bson_t *d2 = tmp_bson(BSON_STR({"x" : 3}));
   bson_unowned_t views[3] = {
      {.data = bson_get_data(d0), .len = d0->len},
      {.data = bson_get_data(d1), .len = d1->len},
      {.data = bson_get_data(d2), .len = d2->len},
   };

   mongoc_async_future_t *f = mongoc_async_collection_insert_many(coll, views, 3, NULL);
   ASSERT(f);
   AWAIT(f);

   uint64_t inserted = 0;
   ASSERT(mongoc_async_future_get_uint64(f, &inserted));
   ASSERT_CMPUINT64(inserted, ==, 3);

   mongoc_async_future_destroy(f);
   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_find_one_async(void)
{
   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "test_async");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "find_one_async");

   mongoc_async_collection_drop_await(coll);

   /* Insert a known doc first using sync API. */
   mongoc_async_error_t *err = NULL;
   bson_t *seed = tmp_bson(BSON_STR({"_id" : 42, "val" : "hello"}));
   bson_unowned_t seed_view = {.data = bson_get_data(seed), .len = seed->len};
   ASSERT(mongoc_async_collection_insert_one_await(coll, seed_view, NULL, &err));

   /* find_one with match */
   bson_t *filter = tmp_bson(BSON_STR({"_id" : 42}));
   bson_unowned_t fv = {.data = bson_get_data(filter), .len = filter->len};
   mongoc_async_future_t *f = mongoc_async_collection_find_one(coll, fv, NULL);
   ASSERT(f);
   AWAIT(f);

   bson_owned_t *doc = mongoc_async_future_get_bson(f);
   ASSERT(doc);
   bson_owned_destroy(doc);
   mongoc_async_future_destroy(f);

   /* find_one with no match → returns null, no error */
   bson_t *nomatch = tmp_bson(BSON_STR({"_id" : 999}));
   bson_unowned_t nv = {.data = bson_get_data(nomatch), .len = nomatch->len};
   f = mongoc_async_collection_find_one(coll, nv, NULL);
   ASSERT(f);
   AWAIT(f);

   ASSERT(!mongoc_async_future_get_bson(f));
   ASSERT(!mongoc_async_future_get_error(f));
   mongoc_async_future_destroy(f);

   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_update_one_async(void)
{
   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "test_async");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "update_one_async");

   mongoc_async_collection_drop_await(coll);

   mongoc_async_error_t *err = NULL;
   bson_t *seed = tmp_bson(BSON_STR({"_id" : 1, "v" : 0}));
   bson_unowned_t sv = {.data = bson_get_data(seed), .len = seed->len};
   ASSERT(mongoc_async_collection_insert_one_await(coll, sv, NULL, &err));

   bson_t *filter = tmp_bson(BSON_STR({"_id" : 1}));
   bson_t *update = tmp_bson(BSON_STR({"$set" : {"v" : 1}}));
   bson_unowned_t fv = {.data = bson_get_data(filter), .len = filter->len};
   bson_unowned_t uv = {.data = bson_get_data(update), .len = update->len};

   mongoc_async_future_t *f = mongoc_async_collection_update_one(coll, fv, uv, NULL);
   ASSERT(f);
   AWAIT(f);

   uint64_t matched = 0, modified = 0;
   ASSERT(mongoc_async_future_get_update_result(f, &matched, &modified));
   ASSERT_CMPUINT64(matched, ==, 1);
   ASSERT_CMPUINT64(modified, ==, 1);

   mongoc_async_future_destroy(f);
   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_update_many_async(void)
{
   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "test_async");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "update_many_async");

   mongoc_async_error_t *err = NULL;
   bson_t *d0 = tmp_bson(BSON_STR({"tag" : "a"}));
   bson_t *d1 = tmp_bson(BSON_STR({"tag" : "a"}));
   bson_unowned_t v0 = {.data = bson_get_data(d0), .len = d0->len};
   bson_unowned_t v1 = {.data = bson_get_data(d1), .len = d1->len};
   ASSERT(mongoc_async_collection_insert_one_await(coll, v0, NULL, &err));
   ASSERT(mongoc_async_collection_insert_one_await(coll, v1, NULL, &err));

   bson_t *filter = tmp_bson(BSON_STR({"tag" : "a"}));
   bson_t *update = tmp_bson(BSON_STR({"$set" : {"tag" : "b"}}));
   bson_unowned_t fv = {.data = bson_get_data(filter), .len = filter->len};
   bson_unowned_t uv = {.data = bson_get_data(update), .len = update->len};

   mongoc_async_future_t *f = mongoc_async_collection_update_many(coll, fv, uv, NULL);
   ASSERT(f);
   AWAIT(f);

   uint64_t matched = 0, modified = 0;
   ASSERT(mongoc_async_future_get_update_result(f, &matched, &modified));
   ASSERT_CMPUINT64(matched, ==, 2);
   ASSERT_CMPUINT64(modified, ==, 2);

   mongoc_async_future_destroy(f);
   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_replace_one_async(void)
{
   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "test_async");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "replace_one_async");

   mongoc_async_collection_drop_await(coll);

   mongoc_async_error_t *err = NULL;
   bson_t *seed = tmp_bson(BSON_STR({"_id" : 1, "old" : true}));
   bson_unowned_t sv = {.data = bson_get_data(seed), .len = seed->len};
   ASSERT(mongoc_async_collection_insert_one_await(coll, sv, NULL, &err));

   bson_t *filter = tmp_bson(BSON_STR({"_id" : 1}));
   bson_t *replacement = tmp_bson(BSON_STR({"_id" : 1, "new" : true}));
   bson_unowned_t fv = {.data = bson_get_data(filter), .len = filter->len};
   bson_unowned_t rv = {.data = bson_get_data(replacement), .len = replacement->len};

   mongoc_async_future_t *f = mongoc_async_collection_replace_one(coll, fv, rv, NULL);
   ASSERT(f);
   AWAIT(f);

   uint64_t matched = 0, modified = 0;
   ASSERT(mongoc_async_future_get_update_result(f, &matched, &modified));
   ASSERT_CMPUINT64(matched, ==, 1);
   ASSERT_CMPUINT64(modified, ==, 1);

   mongoc_async_future_destroy(f);
   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_delete_one_async(void)
{
   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "test_async");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "delete_one_async");

   mongoc_async_error_t *err = NULL;
   bson_t *seed = tmp_bson(BSON_STR({"x" : 1}));
   bson_unowned_t sv = {.data = bson_get_data(seed), .len = seed->len};
   ASSERT(mongoc_async_collection_insert_one_await(coll, sv, NULL, &err));

   bson_t *filter = tmp_bson(BSON_STR({"x" : 1}));
   bson_unowned_t fv = {.data = bson_get_data(filter), .len = filter->len};

   mongoc_async_future_t *f = mongoc_async_collection_delete_one(coll, fv, NULL);
   ASSERT(f);
   AWAIT(f);

   uint64_t deleted = 0;
   ASSERT(mongoc_async_future_get_uint64(f, &deleted));
   ASSERT_CMPUINT64(deleted, ==, 1);

   mongoc_async_future_destroy(f);
   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_delete_many_async(void)
{
   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "test_async");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "delete_many_async");

   mongoc_async_error_t *err = NULL;
   bson_t *d0 = tmp_bson(BSON_STR({"tag" : "del"}));
   bson_t *d1 = tmp_bson(BSON_STR({"tag" : "del"}));
   bson_t *d2 = tmp_bson(BSON_STR({"tag" : "del"}));
   bson_unowned_t v0 = {.data = bson_get_data(d0), .len = d0->len};
   bson_unowned_t v1 = {.data = bson_get_data(d1), .len = d1->len};
   bson_unowned_t v2 = {.data = bson_get_data(d2), .len = d2->len};
   ASSERT(mongoc_async_collection_insert_one_await(coll, v0, NULL, &err));
   ASSERT(mongoc_async_collection_insert_one_await(coll, v1, NULL, &err));
   ASSERT(mongoc_async_collection_insert_one_await(coll, v2, NULL, &err));

   bson_t *filter = tmp_bson(BSON_STR({"tag" : "del"}));
   bson_unowned_t fv = {.data = bson_get_data(filter), .len = filter->len};

   mongoc_async_future_t *f = mongoc_async_collection_delete_many(coll, fv, NULL);
   ASSERT(f);
   AWAIT(f);

   uint64_t deleted = 0;
   ASSERT(mongoc_async_future_get_uint64(f, &deleted));
   ASSERT_CMPUINT64(deleted, ==, 3);

   mongoc_async_future_destroy(f);
   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_collection_distinct_async(void)
{
   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "test_async");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "distinct_async");

   mongoc_async_error_t *err = NULL;
   bson_t *d0 = tmp_bson(BSON_STR({"color" : "red"}));
   bson_t *d1 = tmp_bson(BSON_STR({"color" : "red"}));
   bson_t *d2 = tmp_bson(BSON_STR({"color" : "blue"}));
   bson_unowned_t v0 = {.data = bson_get_data(d0), .len = d0->len};
   bson_unowned_t v1 = {.data = bson_get_data(d1), .len = d1->len};
   bson_unowned_t v2 = {.data = bson_get_data(d2), .len = d2->len};
   ASSERT(mongoc_async_collection_insert_one_await(coll, v0, NULL, &err));
   ASSERT(mongoc_async_collection_insert_one_await(coll, v1, NULL, &err));
   ASSERT(mongoc_async_collection_insert_one_await(coll, v2, NULL, &err));

   bson_t *filter = tmp_bson(BSON_STR({}));
   bson_unowned_t fv = {.data = bson_get_data(filter), .len = filter->len};

   mongoc_async_future_t *f = mongoc_async_collection_distinct(coll, "color", fv);
   ASSERT(f);
   AWAIT(f);

   bson_owned_t *result = mongoc_async_future_get_bson(f);
   ASSERT(result);

   bson_unowned_t rv = bson_owned_as_view(result);
   bson_t bson;
   bson_init_static(&bson, (uint8_t const *)rv.data, rv.len);
   bson_iter_t iter, child;
   ASSERT(bson_iter_init_find(&iter, &bson, "values"));
   ASSERT(bson_iter_recurse(&iter, &child));
   int count = 0;
   while (bson_iter_next(&child))
      count++;
   ASSERT_CMPINT(count, ==, 2);

   bson_owned_destroy(result);
   mongoc_async_future_destroy(f);
   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_database_run_command_async(void)
{
   mongoc_async_client_t *client = mongoc_async_client_new("mongodb://localhost:27017");
   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "admin");

   bson_t *cmd = tmp_bson(BSON_STR({"ping" : 1}));
   bson_unowned_t cv = {.data = bson_get_data(cmd), .len = cmd->len};

   mongoc_async_future_t *f = mongoc_async_database_run_command(db, cv);
   ASSERT(f);
   AWAIT(f);

   bson_owned_t *reply = mongoc_async_future_get_bson(f);
   ASSERT(reply);

   bson_unowned_t rv = bson_owned_as_view(reply);
   bson_t bson;
   bson_init_static(&bson, (uint8_t const *)rv.data, rv.len);
   bson_iter_t iter;
   ASSERT(bson_iter_init_find(&iter, &bson, "ok"));

   bson_owned_destroy(reply);
   mongoc_async_future_destroy(f);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

/* -----------------------------------------------------------------------
 * APM callback test
 * ----------------------------------------------------------------------- */

typedef struct {
   int started_count;
   int succeeded_count;
   int failed_count;
   char last_started_cmd[64];
   char last_succeeded_cmd[64];
} apm_ctx_t;

static void
apm_on_started(const mongoc_async_command_started_event_t *e, void *ud)
{
   apm_ctx_t *ctx = (apm_ctx_t *)ud;
   ctx->started_count++;
   strncpy(ctx->last_started_cmd, e->command_name, sizeof(ctx->last_started_cmd) - 1);
}

static void
apm_on_succeeded(const mongoc_async_command_succeeded_event_t *e, void *ud)
{
   apm_ctx_t *ctx = (apm_ctx_t *)ud;
   ctx->succeeded_count++;
   strncpy(ctx->last_succeeded_cmd, e->command_name, sizeof(ctx->last_succeeded_cmd) - 1);
}

static void
apm_on_failed(const mongoc_async_command_failed_event_t *e, void *ud)
{
   apm_ctx_t *ctx = (apm_ctx_t *)ud;
   ctx->failed_count++;
   (void)e;
}

static void
test_apm_callbacks(void)
{
   apm_ctx_t ctx = {0};

   mongoc_async_apm_callbacks_t *apm = mongoc_async_apm_callbacks_new();
   mongoc_async_apm_callbacks_set_started(apm, apm_on_started, &ctx);
   mongoc_async_apm_callbacks_set_succeeded(apm, apm_on_succeeded);
   mongoc_async_apm_callbacks_set_failed(apm, apm_on_failed);

   mongoc_async_client_t *client = mongoc_async_client_new_with_apm("mongodb://localhost:27017", apm);
   mongoc_async_apm_callbacks_destroy(apm);
   ASSERT(client);

   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "apm_test");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "apm_coll");
   mongoc_async_collection_drop_await(coll);

   bson_t *doc = tmp_bson(BSON_STR({"x" : 1}));
   bson_unowned_t dv = {.data = bson_get_data(doc), .len = doc->len};

   mongoc_async_error_t *err = NULL;
   bool ok = mongoc_async_collection_insert_one_await(coll, dv, NULL, &err);
   ASSERT(ok);
   ASSERT(!err);

   /* The started/succeeded callbacks should have been called for "insert". */
   ASSERT_CMPINT(ctx.started_count, >=, 1);
   ASSERT_CMPINT(ctx.succeeded_count, >=, 1);
   ASSERT_CMPINT(ctx.failed_count, ==, 0);
   ASSERT_CMPSTR(ctx.last_started_cmd, "insert");
   ASSERT_CMPSTR(ctx.last_succeeded_cmd, "insert");

   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

/* -----------------------------------------------------------------------
 * insert_one comment test: verifies the comment opt reaches the server
 * ----------------------------------------------------------------------- */

typedef struct {
   bson_t *last_insert_cmd; /* owned copy of the most recent insert command, or NULL */
} comment_apm_ctx_t;

static void
comment_apm_on_started(const mongoc_async_command_started_event_t *e, void *ud)
{
   comment_apm_ctx_t *ctx = (comment_apm_ctx_t *)ud;
   if (strcmp(e->command_name, "insert") != 0) {
      return;
   }
   bson_destroy(ctx->last_insert_cmd);
   ctx->last_insert_cmd = bson_new_from_data((const uint8_t *)e->command.data, e->command.len);
}

static void
test_collection_insert_one_comment(void)
{
   comment_apm_ctx_t ctx = {NULL};

   mongoc_async_apm_callbacks_t *apm = mongoc_async_apm_callbacks_new();
   mongoc_async_apm_callbacks_set_started(apm, comment_apm_on_started, &ctx);

   mongoc_async_client_t *client = mongoc_async_client_new_with_apm("mongodb://localhost:27017", apm);
   mongoc_async_apm_callbacks_destroy(apm);
   ASSERT(client);

   mongoc_async_database_t *db = mongoc_async_client_get_database(client, "db");
   mongoc_async_collection_t *coll = mongoc_async_database_get_collection(db, "coll_insert_one_comment");
   mongoc_async_collection_drop_await(coll);

   bson_t *doc = tmp_bson(BSON_STR({"x" : 1}));
   bson_unowned_t dv = {.data = bson_get_data(doc), .len = doc->len};

   /* Test 1: string comment via mongoc_async_insert_one_opts_set_comment */
   {
      mongoc_async_insert_one_opts_t *opts = mongoc_async_insert_one_opts_new();
      mongoc_async_insert_one_opts_set_comment(opts, "my comment");

      mongoc_async_error_t *err = NULL;
      bool ok = mongoc_async_collection_insert_one_await(coll, dv, opts, &err);
      mongoc_async_insert_one_opts_destroy(opts);
      ASSERT(ok);
      ASSERT(!err);
      ASSERT(ctx.last_insert_cmd);
      ASSERT_MATCH(ctx.last_insert_cmd, BSON_STR({"comment" : "my comment"}));
      bson_destroy(ctx.last_insert_cmd);
      ctx.last_insert_cmd = NULL;
   }

   /* Test 2: int32 BSON value comment (42) via mongoc_async_insert_one_opts_set_comment_value */
   {
      mongoc_async_insert_one_opts_t *opts = mongoc_async_insert_one_opts_new();
      bson_value_t cv = {.value_type = BSON_TYPE_INT32, .value.v_int32 = 42};
      mongoc_async_insert_one_opts_set_comment_value(opts, &cv);

      mongoc_async_error_t *err = NULL;
      bool ok = mongoc_async_collection_insert_one_await(coll, dv, opts, &err);
      mongoc_async_insert_one_opts_destroy(opts);
      ASSERT(ok);
      ASSERT(!err);
      ASSERT(ctx.last_insert_cmd);
      ASSERT_MATCH(ctx.last_insert_cmd, BSON_STR({"comment" : 42}));
      bson_destroy(ctx.last_insert_cmd);
      ctx.last_insert_cmd = NULL;
   }

   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

static void
test_client_get_database_names(void)
{
   mongoc_async_client_t *const client = mongoc_async_client_new("mongodb://localhost:27017");
   ASSERT(client);

   /* Create two databases so they are guaranteed to appear in the list. */
   mongoc_async_database_t *const db_a = mongoc_async_client_get_database(client, "rust_test_list_a");
   ASSERT(db_a);
   mongoc_async_database_t *const db_b = mongoc_async_client_get_database(client, "rust_test_list_b");
   ASSERT(db_b);

   mongoc_async_collection_t *const coll_a = mongoc_async_database_create_collection_await(db_a, "sentinel");
   ASSERT(coll_a);
   mongoc_async_collection_t *const coll_b = mongoc_async_database_create_collection_await(db_b, "sentinel");
   ASSERT(coll_b);

   char **const names = mongoc_async_client_get_database_names_await(client);
   ASSERT(names);

   /* Verify both database names are present somewhere in the sorted list. */
   bool found_a = false, found_b = false;
   for (char **p = names; *p; ++p) {
      if (strcmp(*p, "rust_test_list_a") == 0) found_a = true;
      if (strcmp(*p, "rust_test_list_b") == 0) found_b = true;
   }
   ASSERT(found_a);
   ASSERT(found_b);

   bson_strfreev(names);

   mongoc_async_database_drop_await(db_a);
   mongoc_async_database_drop_await(db_b);

   mongoc_async_collection_destroy(coll_a);
   mongoc_async_collection_destroy(coll_b);
   mongoc_async_database_destroy(db_a);
   mongoc_async_database_destroy(db_b);
   mongoc_async_client_destroy(client);
}

static void
test_collection_rename(void)
{
   mongoc_async_client_t *const client = mongoc_async_client_new("mongodb://localhost:27017");
   ASSERT(client);

   mongoc_async_database_t *const db = mongoc_async_client_get_database(client, "rust_test_rename");
   ASSERT(db);

   /* Start with a clean slate. */
   mongoc_async_database_drop_await(db);

   /* Create source collection and insert a document so the collection exists. */
   mongoc_async_collection_t *const src = mongoc_async_database_create_collection_await(db, "before");
   ASSERT(src);
   bson_t doc;
   bson_init(&doc);
   BSON_APPEND_UTF8(&doc, "hello", "world");
   bson_unowned_t view = {bson_get_data(&doc), doc.len};
   ASSERT(mongoc_async_collection_insert_one_await(src, view, NULL, NULL));
   bson_destroy(&doc);

   /* Rename within the same database. */
   mongoc_async_error_t *err = NULL;
   bool ok = mongoc_async_collection_rename_await(src, "rust_test_rename", "after", false, &err);
   if (!ok) {
      fprintf(stderr, "rename failed: %s\n", err ? mongoc_async_error_get_message(err) : "(no error)");
      if (err) mongoc_async_error_destroy(err);
   }
   ASSERT(ok);

   /* The renamed collection should have 1 document. */
   mongoc_async_collection_t *const after = mongoc_async_database_get_collection(db, "after");
   ASSERT(after);
   ASSERT_CMPINT64(mongoc_async_collection_count_documents_await(after, NULL), ==, (int64_t) 1);

   /* Only "after" should exist — "before" was renamed away. */
   char **const coll_names = mongoc_async_database_get_collection_names_with_opts_await(db);
   ASSERT(coll_names);
   ASSERT_CMPSTR(coll_names[0], "after");
   ASSERT_CMPSTR(coll_names[1], NULL);
   bson_strfreev(coll_names);

   mongoc_async_database_drop_await(db);

   mongoc_async_collection_destroy(after);
   mongoc_async_collection_destroy(src);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

/* Helper: insert one BSON doc into a collection using a session. */
static bool
insert_with_session(mongoc_async_collection_t *coll,
                    mongoc_async_session_t *session,
                    const char *key,
                    const char *val)
{
   bson_t doc;
   bson_init(&doc);
   BSON_APPEND_UTF8(&doc, key, val);
   bson_unowned_t view = {bson_get_data(&doc), doc.len};
   bool ok = mongoc_async_collection_insert_one_with_session_await(coll, view, NULL, session, NULL);
   bson_destroy(&doc);
   return ok;
}

/*
 * Start a session, begin a transaction, insert a document, commit, then verify
 * the document persists.  Silently skips on standalone mongod (no txn support).
 */
static void
test_session_commit(void)
{
   mongoc_async_client_t *const client = mongoc_async_client_new("mongodb://localhost:27017");
   ASSERT(client);

   mongoc_async_database_t *const db =
      mongoc_async_client_get_database(client, "rust_test_txn_commit");
   mongoc_async_database_drop_await(db);
   mongoc_async_collection_t *const coll =
      mongoc_async_database_create_collection_await(db, "items");
   ASSERT(coll);

   mongoc_async_error_t *err = NULL;
   mongoc_async_session_t *const session = mongoc_async_client_start_session_await(client, &err);
   ASSERT(session);
   ASSERT(!err);

   if (!mongoc_async_session_start_transaction_await(session, &err)) {
      /* Standalone mongod does not support transactions — skip. */
      fprintf(stderr,
              "[rust] test_session_commit: skipping (no transaction support): %s\n",
              err ? mongoc_async_error_get_message(err) : "");
      if (err) mongoc_async_error_destroy(err);
      mongoc_async_session_destroy(session);
      mongoc_async_collection_destroy(coll);
      mongoc_async_database_destroy(db);
      mongoc_async_client_destroy(client);
      return;
   }

   ASSERT(insert_with_session(coll, session, "step", "commit"));

   err = NULL;
   ASSERT(mongoc_async_session_commit_transaction_await(session, &err));
   ASSERT(!err);

   /* Document must be visible after commit. */
   ASSERT_CMPINT64(mongoc_async_collection_count_documents_await(coll, NULL), ==, (int64_t) 1);

   mongoc_async_session_destroy(session);
   mongoc_async_database_drop_await(db);
   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

/*
 * Start a session, begin a transaction, insert a document, abort, then verify
 * the document was rolled back.  Silently skips on standalone mongod.
 */
static void
test_session_abort(void)
{
   mongoc_async_client_t *const client = mongoc_async_client_new("mongodb://localhost:27017");
   ASSERT(client);

   mongoc_async_database_t *const db =
      mongoc_async_client_get_database(client, "rust_test_txn_abort");
   mongoc_async_database_drop_await(db);
   mongoc_async_collection_t *const coll =
      mongoc_async_database_create_collection_await(db, "items");
   ASSERT(coll);

   mongoc_async_error_t *err = NULL;
   mongoc_async_session_t *const session = mongoc_async_client_start_session_await(client, &err);
   ASSERT(session);
   ASSERT(!err);

   if (!mongoc_async_session_start_transaction_await(session, &err)) {
      fprintf(stderr,
              "[rust] test_session_abort: skipping (no transaction support): %s\n",
              err ? mongoc_async_error_get_message(err) : "");
      if (err) mongoc_async_error_destroy(err);
      mongoc_async_session_destroy(session);
      mongoc_async_collection_destroy(coll);
      mongoc_async_database_destroy(db);
      mongoc_async_client_destroy(client);
      return;
   }

   ASSERT(insert_with_session(coll, session, "step", "abort"));

   err = NULL;
   ASSERT(mongoc_async_session_abort_transaction_await(session, &err));
   ASSERT(!err);

   /* Insert was rolled back — collection must be empty. */
   ASSERT_CMPINT64(mongoc_async_collection_count_documents_await(coll, NULL), ==, (int64_t) 0);

   mongoc_async_session_destroy(session);
   mongoc_async_database_drop_await(db);
   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

/*
 * Helper: return true if `name` appears in the null-terminated `names` array.
 */
static bool
names_contains(char **names, const char *name)
{
   for (; *names; ++names) {
      if (strcmp(*names, name) == 0) return true;
   }
   return false;
}

/*
 * Exercise the Standard index management API:
 *   - create_index (two indexes)
 *   - list_index_names  (verify both + _id present)
 *   - drop_index by name
 *   - list_index_names  (verify dropped index gone)
 *   - drop_indexes (drop all non-_id)
 *   - list_index_names  (verify only _id remains)
 */
static void
test_collection_index_management(void)
{
   mongoc_async_client_t *const client = mongoc_async_client_new("mongodb://localhost:27017");
   ASSERT(client);

   mongoc_async_database_t *const db =
      mongoc_async_client_get_database(client, "rust_test_indexes");
   mongoc_async_database_drop_await(db);

   mongoc_async_collection_t *const coll =
      mongoc_async_database_create_collection_await(db, "items");
   ASSERT(coll);

   /* Create index on field "a" (ascending). */
   bson_t keys_a;
   bson_init(&keys_a);
   BSON_APPEND_INT32(&keys_a, "a", 1);
   bson_unowned_t view_a = {bson_get_data(&keys_a), keys_a.len};
   ASSERT(mongoc_async_collection_create_index_await(coll, view_a, false, NULL));
   bson_destroy(&keys_a);

   /* Create unique index on field "b" (descending). */
   bson_t keys_b;
   bson_init(&keys_b);
   BSON_APPEND_INT32(&keys_b, "b", -1);
   bson_unowned_t view_b = {bson_get_data(&keys_b), keys_b.len};
   ASSERT(mongoc_async_collection_create_index_await(coll, view_b, true, NULL));
   bson_destroy(&keys_b);

   /* List index names — expect _id, a_1, b_-1 (sorted). */
   char **names = mongoc_async_collection_list_index_names_await(coll);
   ASSERT(names);
   ASSERT(names_contains(names, "_id_"));
   ASSERT(names_contains(names, "a_1"));
   ASSERT(names_contains(names, "b_-1"));
   bson_strfreev(names);

   /* Drop the "a_1" index by name. */
   ASSERT(mongoc_async_collection_drop_index_await(coll, "a_1", NULL));

   /* Verify "a_1" is gone; "b_-1" and "_id_" remain. */
   names = mongoc_async_collection_list_index_names_await(coll);
   ASSERT(names);
   ASSERT(!names_contains(names, "a_1"));
   ASSERT(names_contains(names, "b_-1"));
   ASSERT(names_contains(names, "_id_"));
   bson_strfreev(names);

   /* Drop all non-_id indexes. */
   ASSERT(mongoc_async_collection_drop_indexes_await(coll, NULL));

   /* Only _id_ should remain. */
   names = mongoc_async_collection_list_index_names_await(coll);
   ASSERT(names);
   ASSERT_CMPSTR(names[0], "_id_");
   ASSERT(names[1] == NULL);
   bson_strfreev(names);

   mongoc_async_database_drop_await(db);
   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

/*
 * Open a change stream, insert a document, and verify the insert event is
 * received with operationType "insert".
 * Silently skips when not running against a replica set.
 */
static void
test_change_stream(void)
{
   mongoc_async_client_t *const client = mongoc_async_client_new("mongodb://localhost:27017");
   ASSERT(client);

   mongoc_async_database_t *const db = mongoc_async_client_get_database(client, "rust_test_change_stream");
   mongoc_async_database_drop_await(db);
   mongoc_async_collection_t *const coll = mongoc_async_database_create_collection_await(db, "items");
   ASSERT(coll);

   /* Open change stream before inserting so the event is captured. */
   mongoc_async_error_t *err = NULL;
   mongoc_async_change_stream_t *const stream = mongoc_async_collection_watch_await(coll, NULL, 0, &err);
   if (!stream) {
      /* Not a replica set — skip. */
      fprintf(stderr,
              "[rust] test_change_stream: skipping (watch failed): %s\n",
              err ? mongoc_async_error_get_message(err) : "");
      if (err)
         mongoc_async_error_destroy(err);
      mongoc_async_collection_destroy(coll);
      mongoc_async_database_destroy(db);
      mongoc_async_client_destroy(client);
      return;
   }

   /* Insert a document to generate an insert event. */
   bson_t *const doc = BCON_NEW("x", BCON_INT32(42));
   bson_unowned_t const doc_view = {.data = bson_get_data(doc), .len = doc->len};
   ASSERT(mongoc_async_collection_insert_one_await(coll, doc_view, NULL, NULL));
   bson_destroy(doc);

   /* Poll until we get the event (each call makes one server round-trip). */
   bool got_event = false;
   for (int i = 0; i < 20 && !got_event; i++) {
      err = NULL;
      if (mongoc_async_change_stream_next_await(stream, &err)) {
         got_event = true;
      }
      ASSERT(!err);
   }
   ASSERT(got_event);

   /* Verify operationType == "insert". */
   bson_unowned_t const view = mongoc_async_change_stream_current(stream);
   bson_t event_bson;
   ASSERT(bson_init_static(&event_bson, view.data, view.len));
   bson_iter_t iter;
   ASSERT(bson_iter_init_find(&iter, &event_bson, "operationType"));
   ASSERT_CMPSTR(bson_iter_utf8(&iter, NULL), "insert");

   mongoc_async_change_stream_destroy(stream);
   mongoc_async_database_drop_await(db);
   mongoc_async_collection_destroy(coll);
   mongoc_async_database_destroy(db);
   mongoc_async_client_destroy(client);
}

/* Argument passed to each progress thread. */
typedef struct {
   mongoc_async_runtime_handle_t *handle;
   volatile int *stop; /* set to 1 by main thread when done */
} progress_thread_arg_t;

static BSON_THREAD_FUN (progress_thread_fn, arg)
{
   progress_thread_arg_t *a = (progress_thread_arg_t *) arg;
   while (!*a->stop) {
      mongoc_async_runtime_handle_make_progress (a->handle, 5 /* ms */);
   }
   BSON_THREAD_RETURN;
}

/*
 * Test that mongoc_async_runtime_handle_make_progress can be called concurrently
 * from multiple threads while the main thread polls an async future.
 *
 * Design:
 *  - Obtain a RuntimeHandle (Arc clone of the client's Tokio runtime).
 *  - Spawn N_THREADS threads, each calling make_progress in a tight loop.
 *  - Main thread issues database_drop_async and polls the future without
 *    calling make_progress itself — progress is entirely driven by the
 *    background threads.
 *  - Verify the future completes and returns a success result.
 */
static void
test_runtime_handle_make_progress_multithreaded (void)
{
#define N_THREADS 3

   mongoc_async_client_t *const client = mongoc_async_client_new ("mongodb://localhost:27017");
   ASSERT (client);

   mongoc_async_runtime_handle_t *const handle = mongoc_async_client_get_runtime_handle (client);
   ASSERT (handle);

   mongoc_async_database_t *const db = mongoc_async_client_get_database (client, "rust_test_make_progress");
   ASSERT (db);

   /* Start an async drop — this will need IO to complete. */
   mongoc_async_future_t *const future = mongoc_async_database_drop (db);
   ASSERT (future);

   volatile int stop = 0;
   progress_thread_arg_t arg = {handle, &stop};

   /* Spawn threads that drive the runtime. */
   bson_thread_t threads[N_THREADS];
   for (int i = 0; i < N_THREADS; i++) {
      ASSERT_CMPINT (0, ==, mcommon_thread_create (&threads[i], progress_thread_fn, &arg));
   }

   /* Poll the future until complete.  Threads supply all the progress. */
   while (!mongoc_async_future_poll (future)) {
      /* nothing — background threads drive IO */
   }

   stop = 1;
   for (int i = 0; i < N_THREADS; i++) {
      mcommon_thread_join (threads[i]);
   }

   /* Verify the drop succeeded. */
   ASSERT (mongoc_async_future_get_void (future));

   mongoc_async_future_destroy (future);
   mongoc_async_runtime_handle_destroy (handle);
   mongoc_async_database_destroy (db);
   mongoc_async_client_destroy (client);

#undef N_THREADS
}

void
test_mongoc_async_install(TestSuite *suite)
{
   TestSuite_Add(suite, "/rust/sanity_check", test_sanity_check);

   TestSuite_AddLive(suite, "/rust/client/new/valid", test_client_new_valid);
   TestSuite_AddLive(suite, "/rust/client/new/invalid", test_client_new_invalid);
   TestSuite_AddLive(suite, "/rust/client/get_database", test_client_get_database);
   TestSuite_AddLive(suite, "/rust/client/get_database_names", test_client_get_database_names);

   TestSuite_AddLive(suite, "/rust/database/get_name", test_database_get_name);
   TestSuite_AddLive(suite, "/rust/database/drop", test_database_drop);
   TestSuite_AddLive(suite, "/rust/database/get_collection", test_database_get_collection);
   TestSuite_AddLive(suite, "/rust/database/create_collection", test_database_create_collection);

   TestSuite_AddLive(suite, "/rust/collection/get_name", test_collection_get_name);
   TestSuite_AddLive(suite, "/rust/collection/drop", test_collection_drop);
   TestSuite_AddLive(suite, "/rust/collection/rename", test_collection_rename);
   TestSuite_AddLive(suite, "/rust/collection/count_documents", test_collection_count_documents);
   TestSuite_AddLive(suite, "/rust/collection/index_management", test_collection_index_management);

   TestSuite_AddLive(suite, "/rust/database/drop_async", test_database_drop_async);
   TestSuite_AddLive(suite, "/rust/collection/count_documents_async", test_collection_count_documents_async);

   TestSuite_AddLive(suite, "/rust/collection/insert_one", test_collection_insert_one);
   TestSuite_AddLive(suite, "/rust/collection/insert_one_error", test_collection_insert_one_error);
   TestSuite_AddLive(suite, "/rust/collection/insert_many", test_collection_insert_many);
   TestSuite_AddLive(suite, "/rust/collection/find", test_collection_find);
   TestSuite_AddLive(suite, "/rust/collection/find_one", test_collection_find_one);
   TestSuite_AddLive(suite, "/rust/collection/update_one", test_collection_update_one);
   TestSuite_AddLive(suite, "/rust/collection/replace_one", test_collection_replace_one);
   TestSuite_AddLive(suite, "/rust/collection/delete_one", test_collection_delete_one);
   TestSuite_AddLive(suite, "/rust/collection/delete_many", test_collection_delete_many);
   TestSuite_AddLive(suite, "/rust/collection/distinct", test_collection_distinct);
   TestSuite_AddLive(suite, "/rust/database/run_command", test_database_run_command);

   TestSuite_AddLive(suite, "/rust/collection/insert_many_async", test_collection_insert_many_async);
   TestSuite_AddLive(suite, "/rust/collection/find_one_async", test_collection_find_one_async);
   TestSuite_AddLive(suite, "/rust/collection/update_one_async", test_collection_update_one_async);
   TestSuite_AddLive(suite, "/rust/collection/update_many_async", test_collection_update_many_async);
   TestSuite_AddLive(suite, "/rust/collection/replace_one_async", test_collection_replace_one_async);
   TestSuite_AddLive(suite, "/rust/collection/delete_one_async", test_collection_delete_one_async);
   TestSuite_AddLive(suite, "/rust/collection/delete_many_async", test_collection_delete_many_async);
   TestSuite_AddLive(suite, "/rust/collection/distinct_async", test_collection_distinct_async);
   TestSuite_AddLive(suite, "/rust/database/run_command_async", test_database_run_command_async);

   TestSuite_AddLive(suite, "/rust/session/commit", test_session_commit);
   TestSuite_AddLive(suite, "/rust/session/abort", test_session_abort);

   TestSuite_AddLive(suite, "/rust/apm/command_events", test_apm_callbacks);
   TestSuite_AddLive(suite, "/rust/collection/insert_one_comment", test_collection_insert_one_comment);

   TestSuite_AddLive(suite, "/rust/collection/change_stream", test_change_stream);

   TestSuite_AddLive(suite, "/rust/runtime_handle/make_progress_multithreaded",
                     test_runtime_handle_make_progress_multithreaded);
}
