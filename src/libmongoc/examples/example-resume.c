/* shows an example implementation of custom resume logic in a change stream. */
#include <mongoc.h>

int
main ()
{
   int exit_code = EXIT_FAILURE;
   const char *uri_string;
   mongoc_uri_t *uri = NULL;
   bson_error_t error;
   mongoc_client_t *client = NULL;
   mongoc_collection_t *coll = NULL;
   bson_t pipeline = BSON_INITIALIZER;
   bson_t opts = BSON_INITIALIZER;
   mongoc_change_stream_t *stream = NULL;
   bson_t cmd = BSON_INITIALIZER;
   bson_t reply;
   bson_iter_t iter;
   const bson_t *doc;
   bson_value_t cached_operation_time = {0}, cached_resume_token = {0};
   int i;

   mongoc_init ();
   uri_string = "mongodb://"
                "localhost:27017,localhost:27018,localhost:27019"
                "/db?replicaSet=rs0";
   uri = mongoc_uri_new_with_error (uri_string, &error);
   if (!uri) {
      fprintf (stderr,
               "failed to parse URI: %s\n"
               "error message:       %s\n",
               uri_string,
               error.message);
      goto cleanup;
   }

   client = mongoc_client_new_from_uri (uri);
   if (!client) {
      goto cleanup;
   }

   /* send a { ping: 1 } command, use the operationTime from the reply. */
   BSON_APPEND_INT64 (&cmd, "ping", 1);
   if (!mongoc_client_command_simple (
          client, "admin", &cmd, NULL, &reply, &error)) {
      fprintf (stderr, "failed to ping: %s\n", error.message);
      goto cleanup;
   }

   if (bson_iter_init_find (&iter, &reply, "operationTime")) {
      bson_value_copy (bson_iter_value (&iter), &cached_operation_time);
   } else {
      fprintf (stderr, "reply does not contain operationTime.");
      goto cleanup;
   }

   /* start a change stream at the returned operationTime. */
   BSON_APPEND_VALUE (&opts, "startAtOperationTime", &cached_operation_time);
   coll = mongoc_client_get_collection (client, "db", "coll");
   stream = mongoc_collection_watch (coll, &pipeline, &opts);

   /* loops and returns changes as they come in. If no changes are found after
    * 10 attempts in a row, then exit the loop. */
   for (i = 0; i < 10; i++) {
      int resume_count = 0;

      printf ("listening for changes on db.coll:\n");
      while (mongoc_change_stream_next (stream, &doc)) {
         char *as_json;

         /* a change was found, reset the outer loop. */
         i = 0;
         as_json = bson_as_canonical_extended_json (doc, NULL);
         printf ("change received: %s\n", as_json);
         bson_free (as_json);
         BSON_ASSERT (bson_iter_init_find (&iter, doc, "_id"));
         if (cached_resume_token.value_type) {
            bson_value_destroy (&cached_resume_token);
         }
         bson_value_copy (bson_iter_value (&iter), &cached_resume_token);
      }

      if (mongoc_change_stream_error_document (stream, &error, NULL)) {
         /* on error, try resuming. if we don't have a resume token yet (i.e.
          * we did not receive a document yet, then use the same operation time
          * that we started with. */
         printf ("attempting to resume due to error: %s\n", error.message);
         for (resume_count = 0; resume_count < 10; ++resume_count) {
            mongoc_change_stream_destroy (stream);
            bson_reinit (&opts);
            if (cached_resume_token.value_type) {
               printf ("resuming with resume token.\n");
               BSON_APPEND_VALUE (&opts, "resumeAfter", &cached_resume_token);
            } else {
               printf ("resuming with operation time.\n");
               BSON_APPEND_VALUE (
                  &opts, "startAtOperationTime", &cached_operation_time);
            }
            stream = mongoc_collection_watch (coll, &pipeline, &opts);
            if (!mongoc_change_stream_error_document (stream, &error, NULL)) {
               break;
            }
         }
      }

      if (resume_count == 10) {
         fprintf (stderr, "exceeded number of resume attempts\n");
         break;
      }
   }

   exit_code = EXIT_SUCCESS;

cleanup:
   mongoc_uri_destroy (uri);
   bson_destroy (&cmd);
   bson_destroy (&pipeline);
   bson_destroy (&opts);
   bson_destroy (&reply);
   if (cached_operation_time.value_type) {
      bson_value_destroy (&cached_operation_time);
   }
   if (cached_resume_token.value_type) {
      bson_value_destroy (&cached_resume_token);
   }
   mongoc_change_stream_destroy (stream);
   mongoc_collection_destroy (coll);
   mongoc_client_destroy (client);
   mongoc_cleanup ();
   return exit_code;
}