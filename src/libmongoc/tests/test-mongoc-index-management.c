#include <mongoc/mongoc.h>
#include <TestSuite.h>
#include <test-libmongoc.h>
#include <test-conveniences.h>
#include <bson/bson-dsl.h>
#include <mcd-time.h>

static int
skip_if_no_atlas (void)
{
   char *got = test_framework_getenv ("ATLAS_SEARCH_INDEXES_URI");
   if (!got) {
      MONGOC_DEBUG ("Skipping test. Requires `ATLAS_SEARCH_INDEXES_URI` "
                    "environment variable");
      return 0; // Do not proceed.
   }
   bson_free (got);
   return 1; // Proceed.
}

static void
test_index_management_prose_case1 (void *unused)
{
   bson_error_t error;

   BSON_UNUSED (unused);

   mongoc_client_t *client;
   // Create client.
   {
      char *uristr =
         test_framework_getenv_required ("ATLAS_SEARCH_INDEXES_URI");
      mongoc_uri_t *uri = mongoc_uri_new_with_error (uristr, &error);
      ASSERT_OR_PRINT (uri, error);

      client = mongoc_client_new_from_uri_with_error (uri, &error);
      ASSERT_OR_PRINT (client, error);

      mongoc_uri_destroy (uri);
      bson_free (uristr);
   }

   // Case 1: Driver can successfully create and list search indexes
   {
      // Create a collection with a randomly generated name.
      char *coll0_name = gen_collection_name (BSON_FUNC);
      mongoc_collection_t *coll0;
      {
         mongoc_database_t *db = mongoc_client_get_database (client, "test");
         coll0 = mongoc_database_create_collection (
            db, coll0_name, NULL /* options */, &error);
         ASSERT_OR_PRINT (coll0, error);
         mongoc_database_destroy (db);
      }

      // Create a new search index
      bson_t reply;
      {
         bsonBuildDecl (
            cmd,
            kv ("createSearchIndexes", cstr (coll0_name)),
            kv ("indexes",
                array (
                   doc (kv ("name", cstr ("test-search-index")),
                        kv ("definition",
                            doc (kv ("mappings",
                                     doc (kv ("dynamic", bool (false))))))))));
         bool ok = mongoc_collection_command_simple (
            coll0, &cmd, NULL /* read_prefs */, &reply, &error);
         ASSERT_OR_PRINT (ok, error);
         bson_destroy (&cmd);
      }

      // Assert that the command returns the name of the index
      ASSERT_MATCH (&reply,
                    "{'indexesCreated': [ { 'name': 'test-search-index' }]}");

      // Run ``coll0.listSearchIndexes()`` repeatedly every 5 seconds.
      {
         mcd_timer timer = mcd_timer_expire_after (mcd_minutes (5));

         while (true) {
            bson_t *pipeline = tmp_bson (
               BSON_STR ({"pipeline" : [ {"$listSearchIndexes" : {}} ]}));
            mongoc_cursor_t *cursor =
               mongoc_collection_aggregate (coll0,
                                            MONGOC_QUERY_NONE,
                                            pipeline,
                                            NULL /* opts */,
                                            NULL /* read_prefs */);
            printf ("Listing indexes:\n");

            bool condition_is_met = false;
            const bson_t *got;
            while (mongoc_cursor_next (cursor, &got)) {
               char *got_str = bson_as_canonical_extended_json (got, NULL);
               printf ("  %s\n", got_str);
               bson_free (got_str);
               bson_iter_t iter;

               // Example document:
               // { "id" : "64c15003a199d3199e27ab7a",
               //   "name" : "test-search-index",
               //   "status" : "PENDING",
               //   "queryable" : false,
               //   "latestDefinition" : { "mappings" : { "dynamic" : false } }
               //   }
               if (!bson_iter_init_find (&iter, got, "name")) {
                  // Skip. Condition not yet met.
                  continue;
               }
               ASSERT_WITH_MSG (BSON_ITER_HOLDS_UTF8 (&iter),
                                "expected 'name' to be type UTF8, got: %s",
                                tmp_json (got));
               if (0 !=
                   strcmp (bson_iter_utf8 (&iter, NULL), "test-search-index")) {
                  // Skip. Condition not yet met.
                  continue;
               }
               if (!bson_iter_init_find (&iter, got, "queryable")) {
                  // Skip. Condition not yet met.
                  continue;
               }
               ASSERT_WITH_MSG (BSON_ITER_HOLDS_BOOL (&iter),
                                "expected 'queryable' to be type bool, got: %s",
                                tmp_json (got));
               if (!bson_iter_bool (&iter)) {
                  // Skip. Condition not yet met.
                  continue;
               }

               // Condition met.
               condition_is_met = true;

               // Assert that ``index`` has a property ``mappings`` whose value
               // is ``{ dynamic: false }``
               ASSERT_MATCH (got, "{ 'mappings': {'dynamic': false }}");
               break;
            }
            ASSERT_OR_PRINT (!mongoc_cursor_error (cursor, &error), error);
            mongoc_cursor_destroy (cursor);

            if (condition_is_met) {
               break;
            }

            if (mcd_get_milliseconds (mcd_timer_remaining (timer)) == 0) {
               test_error ("Condition not met. Timer has expired");
            }

            // Sleep for five seconds.
            MONGOC_DEBUG ("Condition not yet met. Sleeping for 5 seconds");
            _mongoc_usleep (5000 * 1000);
         }
      }

      bson_destroy (&reply);
      mongoc_collection_destroy (coll0);
      bson_free (coll0_name);
   }

   mongoc_client_destroy (client);
}

static void
test_index_management_prose_case2 (void *unused)
{
   bson_error_t error;

   BSON_UNUSED (unused);

   mongoc_client_t *client;
   // Create client.
   {
      char *uristr =
         test_framework_getenv_required ("ATLAS_SEARCH_INDEXES_URI");
      mongoc_uri_t *uri = mongoc_uri_new_with_error (uristr, &error);
      ASSERT_OR_PRINT (uri, error);

      client = mongoc_client_new_from_uri_with_error (uri, &error);
      ASSERT_OR_PRINT (client, error);

      mongoc_uri_destroy (uri);
      bson_free (uristr);
   }

   {
      // Create a collection with a randomly generated name.
      char *coll0_name = gen_collection_name (BSON_FUNC);
      mongoc_collection_t *coll0;
      {
         mongoc_database_t *db = mongoc_client_get_database (client, "test");
         coll0 = mongoc_database_create_collection (
            db, coll0_name, NULL /* options */, &error);
         ASSERT_OR_PRINT (coll0, error);
         mongoc_database_destroy (db);
      }

      // Create a new search index
      bson_t reply;
      {
         bsonBuildDecl (
            cmd,
            kv ("createSearchIndexes", cstr (coll0_name)),
            kv ("indexes",
                array (
                   doc (kv ("name", cstr ("test-search-index-1")),
                        kv ("definition",
                            doc (kv ("mappings",
                                     doc (kv ("dynamic", bool (false))))))),

                   doc (kv ("name", cstr ("test-search-index-2")),
                        kv ("definition",
                            doc (kv ("mappings",
                                     doc (kv ("dynamic", bool (false))))))))));
         bool ok = mongoc_collection_command_simple (
            coll0, &cmd, NULL /* read_prefs */, &reply, &error);
         ASSERT_OR_PRINT (ok, error);
         bson_destroy (&cmd);
      }

      // Assert that the command returns the name of the index
      ASSERT_MATCH (&reply,
                    "{'indexesCreated': [ { 'name': 'test-search-index-1' }, { "
                    "'name': 'test-search-index-2' }]}");

      // Run ``coll0.listSearchIndexes()`` repeatedly every 5 seconds.
      {
         mcd_timer timer = mcd_timer_expire_after (mcd_minutes (5));

         while (true) {
            bson_t *pipeline = tmp_bson (
               BSON_STR ({"pipeline" : [ {"$listSearchIndexes" : {}} ]}));
            mongoc_cursor_t *cursor =
               mongoc_collection_aggregate (coll0,
                                            MONGOC_QUERY_NONE,
                                            pipeline,
                                            NULL /* opts */,
                                            NULL /* read_prefs */);
            printf ("Listing indexes:\n");

            bson_t *index1 = NULL;
            bson_t *index2 = NULL;
            const bson_t *got;
            while (mongoc_cursor_next (cursor, &got)) {
               char *got_str = bson_as_canonical_extended_json (got, NULL);
               printf ("  %s\n", got_str);
               bson_free (got_str);
               bson_iter_t iter;

               // Example document:
               // { "id" : "64c15003a199d3199e27ab7a",
               //   "name" : "test-search-index",
               //   "status" : "PENDING",
               //   "queryable" : false,
               //   "latestDefinition" : { "mappings" : { "dynamic" : false } }
               //   }
               if (!bson_iter_init_find (&iter, got, "name")) {
                  // Skip. Condition not yet met.
                  continue;
               }
               ASSERT_WITH_MSG (BSON_ITER_HOLDS_UTF8 (&iter),
                                "expected 'name' to be type UTF8, got: %s",
                                tmp_json (got));
               if (0 == strcmp (bson_iter_utf8 (&iter, NULL),
                                "test-search-index-1")) {
                  if (!bson_iter_init_find (&iter, got, "queryable")) {
                     // Skip. Condition not yet met.
                     continue;
                  }
                  ASSERT_WITH_MSG (
                     BSON_ITER_HOLDS_BOOL (&iter),
                     "expected 'queryable' to be type bool, got: %s",
                     tmp_json (got));
                  if (!bson_iter_bool (&iter)) {
                     // Skip. Condition not yet met.
                     continue;
                  }
                  if (!index1) {
                     index1 = bson_copy (got);
                  }
               } else if (0 == strcmp (bson_iter_utf8 (&iter, NULL),
                                       "test-search-index-2")) {
                  if (!bson_iter_init_find (&iter, got, "queryable")) {
                     // Skip. Condition not yet met.
                     continue;
                  }
                  ASSERT_WITH_MSG (
                     BSON_ITER_HOLDS_BOOL (&iter),
                     "expected 'queryable' to be type bool, got: %s",
                     tmp_json (got));
                  if (!bson_iter_bool (&iter)) {
                     // Skip. Condition not yet met.
                     continue;
                  }
                  if (!index2) {
                     index2 = bson_copy (got);
                  }
               } else {
                  // Skip. Condition not yet met.
                  continue;
               }
            }

            ASSERT_OR_PRINT (!mongoc_cursor_error (cursor, &error), error);
            mongoc_cursor_destroy (cursor);

            if (index1 && index2) {
               // Condition met.
               ASSERT_MATCH (
                  index1,
                  "{'latestDefinition': { 'mappings': {'dynamic': false }}}");
               ASSERT_MATCH (
                  index2,
                  "{'latestDefinition': { 'mappings': {'dynamic': false }}}");
               bson_destroy (index1);
               bson_destroy (index2);
               break;
            }
            bson_destroy (index1);
            bson_destroy (index2);

            if (mcd_get_milliseconds (mcd_timer_remaining (timer)) == 0) {
               test_error ("Condition not met. Timer has expired");
            }

            // Sleep for five seconds.
            MONGOC_DEBUG ("Condition not yet met. Sleeping for 5 seconds");
            _mongoc_usleep (5000 * 1000);
         }
      }

      bson_destroy (&reply);
      mongoc_collection_destroy (coll0);
      bson_free (coll0_name);
   }

   mongoc_client_destroy (client);
}

static void
test_index_management_prose_case3 (void *unused)
{
   bson_error_t error;

   BSON_UNUSED (unused);

   mongoc_client_t *client;
   // Create client.
   {
      char *uristr =
         test_framework_getenv_required ("ATLAS_SEARCH_INDEXES_URI");
      mongoc_uri_t *uri = mongoc_uri_new_with_error (uristr, &error);
      ASSERT_OR_PRINT (uri, error);

      client = mongoc_client_new_from_uri_with_error (uri, &error);
      ASSERT_OR_PRINT (client, error);

      mongoc_uri_destroy (uri);
      bson_free (uristr);
   }

   {
      // Create a collection with a randomly generated name.
      char *coll0_name = gen_collection_name (BSON_FUNC);
      mongoc_collection_t *coll0;
      {
         mongoc_database_t *db = mongoc_client_get_database (client, "test");
         coll0 = mongoc_database_create_collection (
            db, coll0_name, NULL /* options */, &error);
         ASSERT_OR_PRINT (coll0, error);
         mongoc_database_destroy (db);
      }

      // Create a new search index
      bson_t reply;
      {
         bsonBuildDecl (
            cmd,
            kv ("createSearchIndexes", cstr (coll0_name)),
            kv ("indexes",
                array (
                   doc (kv ("name", cstr ("test-search-index")),
                        kv ("definition",
                            doc (kv ("mappings",
                                     doc (kv ("dynamic", bool (false))))))))));
         bool ok = mongoc_collection_command_simple (
            coll0, &cmd, NULL /* read_prefs */, &reply, &error);
         ASSERT_OR_PRINT (ok, error);
         bson_destroy (&cmd);
      }

      // Assert that the command returns the name of the index
      ASSERT_MATCH (&reply,
                    "{'indexesCreated': [ { 'name': 'test-search-index' }]}");
      bson_destroy (&reply);

      // Run ``coll0.listSearchIndexes()`` repeatedly every 5 seconds.
      {
         mcd_timer timer = mcd_timer_expire_after (mcd_minutes (5));

         while (true) {
            bson_t *pipeline = tmp_bson (
               BSON_STR ({"pipeline" : [ {"$listSearchIndexes" : {}} ]}));
            mongoc_cursor_t *cursor =
               mongoc_collection_aggregate (coll0,
                                            MONGOC_QUERY_NONE,
                                            pipeline,
                                            NULL /* opts */,
                                            NULL /* read_prefs */);
            printf ("Listing indexes:\n");

            bool condition_is_met = false;
            const bson_t *got;
            while (mongoc_cursor_next (cursor, &got)) {
               char *got_str = bson_as_canonical_extended_json (got, NULL);
               printf ("  %s\n", got_str);
               bson_free (got_str);
               bson_iter_t iter;

               // Example document:
               // { "id" : "64c15003a199d3199e27ab7a",
               //   "name" : "test-search-index",
               //   "status" : "PENDING",
               //   "queryable" : false,
               //   "latestDefinition" : { "mappings" : { "dynamic" : false } }
               //   }
               if (!bson_iter_init_find (&iter, got, "name")) {
                  // Skip. Condition not yet met.
                  continue;
               }
               ASSERT_WITH_MSG (BSON_ITER_HOLDS_UTF8 (&iter),
                                "expected 'name' to be type UTF8, got: %s",
                                tmp_json (got));
               if (0 !=
                   strcmp (bson_iter_utf8 (&iter, NULL), "test-search-index")) {
                  // Skip. Condition not yet met.
                  continue;
               }
               if (!bson_iter_init_find (&iter, got, "queryable")) {
                  // Skip. Condition not yet met.
                  continue;
               }
               ASSERT_WITH_MSG (BSON_ITER_HOLDS_BOOL (&iter),
                                "expected 'queryable' to be type bool, got: %s",
                                tmp_json (got));
               if (!bson_iter_bool (&iter)) {
                  // Skip. Condition not yet met.
                  continue;
               }

               // Condition met.
               condition_is_met = true;

               break;
            }
            ASSERT_OR_PRINT (!mongoc_cursor_error (cursor, &error), error);
            mongoc_cursor_destroy (cursor);

            if (condition_is_met) {
               break;
            }

            if (mcd_get_milliseconds (mcd_timer_remaining (timer)) == 0) {
               test_error ("Condition not met. Timer has expired");
            }

            // Sleep for five seconds.
            MONGOC_DEBUG ("Condition not yet met. Sleeping for 5 seconds");
            _mongoc_usleep (5000 * 1000);
         }
      }

      // Run a ``dropSearchIndexes`` on ``coll0``
      {
         bsonBuildDecl (cmd,
                        kv ("dropSearchIndex", cstr (coll0_name)),
                        kv ("name", cstr ("test-search-index")));
         bool ok = mongoc_collection_command_simple (
            coll0, &cmd, NULL /* read_prefs */, NULL, &error);
         ASSERT_OR_PRINT (ok, error);
         bson_destroy (&cmd);
      }

      // Run ``coll0.listSearchIndexes()`` repeatedly every 5 seconds.
      {
         mcd_timer timer = mcd_timer_expire_after (mcd_minutes (5));

         while (true) {
            bson_t *pipeline = tmp_bson (
               BSON_STR ({"pipeline" : [ {"$listSearchIndexes" : {}} ]}));
            mongoc_cursor_t *cursor =
               mongoc_collection_aggregate (coll0,
                                            MONGOC_QUERY_NONE,
                                            pipeline,
                                            NULL /* opts */,
                                            NULL /* read_prefs */);
            printf ("Listing indexes:\n");

            bool condition_is_met = true;
            const bson_t *got;
            while (mongoc_cursor_next (cursor, &got)) {
               char *got_str = bson_as_canonical_extended_json (got, NULL);
               printf ("  %s\n", got_str);
               bson_free (got_str);

               condition_is_met = false;
            }
            ASSERT_OR_PRINT (!mongoc_cursor_error (cursor, &error), error);
            mongoc_cursor_destroy (cursor);

            if (condition_is_met) {
               break;
            }

            if (mcd_get_milliseconds (mcd_timer_remaining (timer)) == 0) {
               test_error ("Condition not met. Timer has expired");
            }

            // Sleep for five seconds.
            MONGOC_DEBUG ("Condition not yet met. Sleeping for 5 seconds");
            _mongoc_usleep (5000 * 1000);
         }
      }

      mongoc_collection_destroy (coll0);
      bson_free (coll0_name);
   }

   mongoc_client_destroy (client);
}

static void
test_index_management_prose_case4 (void *unused)
{
   bson_error_t error;

   BSON_UNUSED (unused);

   mongoc_client_t *client;
   // Create client.
   {
      char *uristr =
         test_framework_getenv_required ("ATLAS_SEARCH_INDEXES_URI");
      mongoc_uri_t *uri = mongoc_uri_new_with_error (uristr, &error);
      ASSERT_OR_PRINT (uri, error);

      client = mongoc_client_new_from_uri_with_error (uri, &error);
      ASSERT_OR_PRINT (client, error);

      mongoc_uri_destroy (uri);
      bson_free (uristr);
   }

   {
      // Create a collection with a randomly generated name.
      char *coll0_name = gen_collection_name (BSON_FUNC);
      mongoc_collection_t *coll0;
      {
         mongoc_database_t *db = mongoc_client_get_database (client, "test");
         coll0 = mongoc_database_create_collection (
            db, coll0_name, NULL /* options */, &error);
         ASSERT_OR_PRINT (coll0, error);
         mongoc_database_destroy (db);
      }

      // Create a new search index
      bson_t reply;
      {
         bsonBuildDecl (
            cmd,
            kv ("createSearchIndexes", cstr (coll0_name)),
            kv ("indexes",
                array (
                   doc (kv ("name", cstr ("test-search-index")),
                        kv ("definition",
                            doc (kv ("mappings",
                                     doc (kv ("dynamic", bool (false))))))))));
         bool ok = mongoc_collection_command_simple (
            coll0, &cmd, NULL /* read_prefs */, &reply, &error);
         ASSERT_OR_PRINT (ok, error);
         bson_destroy (&cmd);
      }

      // Assert that the command returns the name of the index
      ASSERT_MATCH (&reply,
                    "{'indexesCreated': [ { 'name': 'test-search-index' }]}");
      bson_destroy (&reply);

      // Run ``coll0.listSearchIndexes()`` repeatedly every 5 seconds.
      {
         mcd_timer timer = mcd_timer_expire_after (mcd_minutes (5));

         while (true) {
            bson_t *pipeline = tmp_bson (
               BSON_STR ({"pipeline" : [ {"$listSearchIndexes" : {}} ]}));
            mongoc_cursor_t *cursor =
               mongoc_collection_aggregate (coll0,
                                            MONGOC_QUERY_NONE,
                                            pipeline,
                                            NULL /* opts */,
                                            NULL /* read_prefs */);
            printf ("Listing indexes:\n");

            bool condition_is_met = false;
            const bson_t *got;
            while (mongoc_cursor_next (cursor, &got)) {
               char *got_str = bson_as_canonical_extended_json (got, NULL);
               printf ("  %s\n", got_str);
               bson_free (got_str);
               bson_iter_t iter;

               // Example document:
               // { "id" : "64c15003a199d3199e27ab7a",
               //   "name" : "test-search-index",
               //   "status" : "PENDING",
               //   "queryable" : false,
               //   "latestDefinition" : { "mappings" : { "dynamic" : false } }
               //   }
               if (!bson_iter_init_find (&iter, got, "name")) {
                  // Skip. Condition not yet met.
                  continue;
               }
               ASSERT_WITH_MSG (BSON_ITER_HOLDS_UTF8 (&iter),
                                "expected 'name' to be type UTF8, got: %s",
                                tmp_json (got));
               if (0 !=
                   strcmp (bson_iter_utf8 (&iter, NULL), "test-search-index")) {
                  // Skip. Condition not yet met.
                  continue;
               }
               if (!bson_iter_init_find (&iter, got, "queryable")) {
                  // Skip. Condition not yet met.
                  continue;
               }
               ASSERT_WITH_MSG (BSON_ITER_HOLDS_BOOL (&iter),
                                "expected 'queryable' to be type bool, got: %s",
                                tmp_json (got));
               if (!bson_iter_bool (&iter)) {
                  // Skip. Condition not yet met.
                  continue;
               }

               // Condition met.
               condition_is_met = true;

               break;
            }
            ASSERT_OR_PRINT (!mongoc_cursor_error (cursor, &error), error);
            mongoc_cursor_destroy (cursor);

            if (condition_is_met) {
               break;
            }

            if (mcd_get_milliseconds (mcd_timer_remaining (timer)) == 0) {
               test_error ("Condition not met. Timer has expired");
            }

            // Sleep for five seconds.
            MONGOC_DEBUG ("Condition not yet met. Sleeping for 5 seconds");
            _mongoc_usleep (5000 * 1000);
         }
      }

      // Run a ``updateSearchIndex`` on ``coll0``
      {
         bsonBuildDecl (
            cmd,
            kv ("updateSearchIndex", cstr (coll0_name)),
            kv ("name", cstr ("test-search-index")),
            kv ("definition",
                doc (kv ("mappings", doc (kv ("dynamic", bool (true)))))));
         bool ok = mongoc_collection_command_simple (
            coll0, &cmd, NULL /* read_prefs */, NULL, &error);
         ASSERT_OR_PRINT (ok, error);
         bson_destroy (&cmd);
      }

      // Run ``coll0.listSearchIndexes()`` repeatedly every 5 seconds.
      {
         mcd_timer timer = mcd_timer_expire_after (mcd_minutes (5));
         while (true) {
            bson_t *pipeline = tmp_bson (
               BSON_STR ({"pipeline" : [ {"$listSearchIndexes" : {}} ]}));
            mongoc_cursor_t *cursor =
               mongoc_collection_aggregate (coll0,
                                            MONGOC_QUERY_NONE,
                                            pipeline,
                                            NULL /* opts */,
                                            NULL /* read_prefs */);
            printf ("Listing indexes:\n");

            bool condition_is_met = false;
            const bson_t *got;
            while (mongoc_cursor_next (cursor, &got)) {
               char *got_str = bson_as_canonical_extended_json (got, NULL);
               printf ("  %s\n", got_str);
               bson_free (got_str);
               bson_iter_t iter;

               // Example document:
               // { "id" : "64c15003a199d3199e27ab7a",
               //   "name" : "test-search-index",
               //   "status" : "PENDING",
               //   "queryable" : false,
               //   "latestDefinition" : { "mappings" : { "dynamic" : false } }
               //   }
               if (!bson_iter_init_find (&iter, got, "name")) {
                  // Skip. Condition not yet met.
                  continue;
               }
               ASSERT_WITH_MSG (BSON_ITER_HOLDS_UTF8 (&iter),
                                "expected 'name' to be type UTF8, got: %s",
                                tmp_json (got));
               if (0 !=
                   strcmp (bson_iter_utf8 (&iter, NULL), "test-search-index")) {
                  // Skip. Condition not yet met.
                  continue;
               }
               if (!bson_iter_init_find (&iter, got, "queryable")) {
                  // Skip. Condition not yet met.
                  continue;
               }
               ASSERT_WITH_MSG (BSON_ITER_HOLDS_BOOL (&iter),
                                "expected 'queryable' to be type bool, got: %s",
                                tmp_json (got));
               if (!bson_iter_bool (&iter)) {
                  // Skip. Condition not yet met.
                  continue;
               }

               if (!bson_iter_init_find (&iter, got, "status")) {
                  // Skip. Condition not yet met.
                  continue;
               }
               ASSERT_WITH_MSG (BSON_ITER_HOLDS_UTF8 (&iter),
                                "expected 'status' to be type utf8, got: %s",
                                tmp_json (got));
               if (0 != strcmp ("READY", bson_iter_utf8 (&iter, NULL))) {
                  // Skip. Condition not yet met.
                  continue;
               }

               // Condition met.
               condition_is_met = true;
               ASSERT_MATCH (got,
                             "{'latestDefinition' : { 'mappings' : { 'dynamic' "
                             ": true } }");

               break;
            }
            ASSERT_OR_PRINT (!mongoc_cursor_error (cursor, &error), error);
            mongoc_cursor_destroy (cursor);

            if (condition_is_met) {
               break;
            }

            if (mcd_get_milliseconds (mcd_timer_remaining (timer)) == 0) {
               test_error ("Condition not met. Timer has expired");
            }

            // Sleep for five seconds.
            MONGOC_DEBUG ("Condition not yet met. Sleeping for 5 seconds");
            _mongoc_usleep (5000 * 1000);
         }
      }

      mongoc_collection_destroy (coll0);
      bson_free (coll0_name);
   }

   mongoc_client_destroy (client);
}

static void
test_index_management_prose_case5 (void *unused)
{
   bson_error_t error;

   BSON_UNUSED (unused);

   mongoc_client_t *client;
   // Create client.
   {
      char *uristr =
         test_framework_getenv_required ("ATLAS_SEARCH_INDEXES_URI");
      mongoc_uri_t *uri = mongoc_uri_new_with_error (uristr, &error);
      ASSERT_OR_PRINT (uri, error);

      client = mongoc_client_new_from_uri_with_error (uri, &error);
      ASSERT_OR_PRINT (client, error);

      mongoc_uri_destroy (uri);
      bson_free (uristr);
   }

   // Case 1: Driver can successfully create and list search indexes
   {
      // Create a driver-side collection with a randomly generated name.
      char *coll0_name = gen_collection_name (BSON_FUNC);
      mongoc_collection_t *coll0;
      {
         mongoc_database_t *db = mongoc_client_get_database (client, "test");
         coll0 = mongoc_database_get_collection (db, coll0_name);
         ASSERT_OR_PRINT (coll0, error);
         mongoc_database_destroy (db);
      }

      // Run a ``dropSearchIndex`` on ``coll0``
      {
         bsonBuildDecl (cmd,
                        kv ("dropSearchIndex", cstr (coll0_name)),
                        kv ("name", cstr ("test-search-index")));
         bool ok = mongoc_collection_command_simple (
            coll0, &cmd, NULL /* read_prefs */, NULL, &error);
         ASSERT_OR_PRINT (ok, error);
         bson_destroy (&cmd);
      }


      mongoc_collection_destroy (coll0);
      bson_free (coll0_name);
   }

   mongoc_client_destroy (client);
}


void
test_index_management_install (TestSuite *suite)
{
   TestSuite_AddFull (suite,
                      "/index_management/prose/case1",
                      test_index_management_prose_case1,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      skip_if_no_atlas);
   TestSuite_AddFull (suite,
                      "/index_management/prose/case2",
                      test_index_management_prose_case2,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      skip_if_no_atlas);
   TestSuite_AddFull (suite,
                      "/index_management/prose/case3",
                      test_index_management_prose_case3,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      skip_if_no_atlas);
   TestSuite_AddFull (suite,
                      "/index_management/prose/case4",
                      test_index_management_prose_case4,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      skip_if_no_atlas);
   TestSuite_AddFull (suite,
                      "/index_management/prose/case5",
                      test_index_management_prose_case5,
                      NULL /* dtor */,
                      NULL /* ctx */,
                      skip_if_no_atlas);
}
