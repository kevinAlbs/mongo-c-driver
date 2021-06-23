/*
 * Test examples connecting to a load balanced cluster.
 */
#include "util.h"

#include <mongoc/mongoc.h>

#include <stdio.h>

static void configure_insert_failpoint (mongoc_client_t *client) {
   bson_t *cmd;
   bson_error_t error;

   cmd = BCON_NEW ("configureFailPoint", "failCommand", "mode", "{", "times", BCON_INT32(1), "}", "data", "{", "failCommands", "[", "insert", "]", "closeConnection", BCON_BOOL(true), "}");

   if (!mongoc_client_command_simple (client, "admin", cmd, NULL /* read prefs */, NULL /* reply */, &error)) {
      DIE ("failpoint error: %s", error.message);
   }
}

static void drop (mongoc_client_t *client) {
   mongoc_collection_t *coll = NULL;
   bson_error_t error;
   
   coll = mongoc_client_get_collection (client, "db", "coll");

   if (!mongoc_collection_drop (coll, &error)) {
      if (NULL == strstr(error.message, "ns not found")) {
         DIE ("collection drop error: %s", error.message);
      }
   }

   mongoc_collection_destroy (coll);
}

static void insert_one (mongoc_client_t *client) {
   mongoc_collection_t *coll = NULL;
   bson_t* doc = NULL;
   static int counter = 0;
   bson_error_t error;
   bool ret;

   counter++;
   coll = mongoc_client_get_collection (client, "db", "coll");
   doc = BCON_NEW ("x", BCON_INT32(counter));
   ret = mongoc_collection_insert_one (coll, doc, NULL /* opts */, NULL /* reply */, &error);
   if (!ret) {
      DIE ("error on insert_one: %s", error.message);
   }

   bson_destroy (doc);
   mongoc_collection_destroy (coll);
}

static void scenario_interrupted_find (mongoc_client_t *client) {
   mongoc_collection_t *coll = NULL;
   mongoc_cursor_t *cursor = NULL;
   const bson_t *out = NULL;
   bson_t *filter = bson_new();
   bson_t *opts = BCON_NEW ("batchSize", BCON_INT32(1));
   bson_error_t error;
   const bson_t* reply;

   MONGOC_INFO ("interrupted_find");
   drop (client);

   coll = mongoc_client_get_collection (client, "db", "coll");
   /* insert two documents */
   insert_one (client);
   insert_one (client);

   /* use a batch size of 1 so a getMore will be run. */
   cursor = mongoc_collection_find_with_opts (coll, filter, opts, NULL /* read prefs */);
   if (!mongoc_cursor_next (cursor, &out)) {
      if (mongoc_cursor_error (cursor, &error)) {
         DIE ("next error on first doc: %s", error.message);
      }
      
      DIE ("no documents returned");
   }
   
   configure_insert_failpoint (client);
   insert_one (client);

   if (!mongoc_cursor_next (cursor, &out)) {
      if (mongoc_cursor_error_document (cursor, &error, &reply)) {
         MONGOC_ERROR ("reply=%s", bson_as_canonical_extended_json (reply, NULL));
         DIE ("next error on second doc: %s", error.message);
      }
      
      DIE ("no documents returned");
   }

   bson_destroy (filter);
   bson_destroy (opts);
   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (coll);
}

static void scenario_interrupted_transaction (mongoc_client_t *client) {
   mongoc_collection_t *coll = NULL;
   mongoc_client_session_t *session = NULL;
   bson_error_t error;
   bson_t *txn_opts = NULL;
   bson_t *filter = NULL;
   bson_t *update = NULL;
   bson_t reply;

   session = mongoc_client_start_session (client, NULL /* opts */, &error);
   if (!session) {
      DIE ("Error creating session: %s", error.message);
   }

   txn_opts = bson_new ();
   if (!mongoc_client_session_append (session, txn_opts, &error)) {
      DIE ("Error appending session: %s", error.message);
   }

   if (!mongoc_client_session_start_transaction (session, NULL /* txn_opts */, &error)) {
      DIE ("Error starting transaction: %s", error.message);
   }

   coll = mongoc_client_get_collection (client, "db", "coll");
   filter = bson_new ();
   update = BCON_NEW ("$set", "{", "x", BCON_INT32(1), "}");
   if (!mongoc_collection_update_one (coll, filter, update, txn_opts, NULL /* reply */, &error)) {
      DIE ("Error during update: %s", error.message);
   }
   
   configure_insert_failpoint (client);
   insert_one (client);

   if (!mongoc_collection_update_one (coll, filter, update, txn_opts, &reply, &error)) {
      MONGOC_ERROR ("reply=%s", bson_as_canonical_extended_json (&reply, NULL));
      DIE ("Error during update: %s, %d, %d", error.message, error.code, error.domain);
   }

   if (!mongoc_client_session_commit_transaction (session, NULL /* reply */, &error)) {
      DIE ("Error during commitTransaction: %s", error.message);
   }

   mongoc_collection_destroy (coll);
   mongoc_client_session_destroy (session);
   bson_destroy (filter);
   bson_destroy (update);
   bson_destroy (txn_opts);
}

int
main (int argc, char *argv[])
{
   mongoc_client_t *client;
   mongoc_database_t *db;
   mongoc_collection_t *coll;
   mongoc_apm_callbacks_t *callbacks;
   bson_error_t error;
   mongoc_uri_t *uri;
   bson_t* args;
   char *args_str;

   args = util_args_parse (argc, argv);
   if (!args) {
      DIE ("Could not parse args");
   }
   args_str = bson_as_canonical_extended_json (args, NULL);
   MONGOC_INFO ("args: %s", args_str);
   bson_free (args_str);

   mongoc_init ();

   uri = mongoc_uri_new_with_error (util_args_get (args, "uri"), &error);
   if (!uri) {
      DIE ("Could not construct URI from \"%s\": %s", util_args_get (args, "uri"), error.message);
   }

   client = mongoc_client_new_from_uri (uri);
   if (!client) {
      return EXIT_FAILURE;
   }

   mongoc_client_set_error_api (client, 2);
   callbacks = util_log_callbacks_new ();
   if (util_args_eq (args, "apm", "on")) {
      if (!util_args_eq (args, "apm_show_succeeded", "on")) {
         mongoc_apm_set_command_succeeded_cb (callbacks, NULL);
      }
      mongoc_client_set_apm_callbacks (client, callbacks, NULL /* context */);
   }

   db = mongoc_client_get_database (client, "db");
   coll = mongoc_database_get_collection (db, "coll");
   
   if (util_args_eq (args, "scenario", "interrupted_find")) {
      scenario_interrupted_find (client); 
   } else if (util_args_eq (args, "scenario", "interrupted_transaction")) {
      scenario_interrupted_transaction (client);
   } else {
      DIE ("no scenario to run");
   }
   
   mongoc_apm_callbacks_destroy (callbacks);
   mongoc_uri_destroy (uri);
   mongoc_collection_destroy (coll);
   mongoc_database_destroy (db);
   mongoc_client_destroy (client);

   mongoc_cleanup ();

   return EXIT_SUCCESS;
}
