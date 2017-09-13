#include <bson.h>
#include "mongoc-change-stream.h"
#include "mongoc-error.h"



struct _mongoc_change_stream_t {
   bson_t pipeline;
   bson_t resume_token;
   bson_t agg_opts;
   mongoc_error_code_t err; /* defer creating error strings until user needs to retrieve it */
   mongoc_cursor_t* cursor;
   mongoc_collection_t* coll;
   mongoc_read_prefs_t* read_prefs;
   int32_t maxAwaitTimeMS; /* TODO: this should be int64_t */
};

bool mongoc_change_stream_next(mongoc_change_stream_t* stream, const bson_t **bson) {
   BSON_ASSERT(bson);
   bool succeeded = mongoc_cursor_next(stream->cursor, bson);
   if (!succeeded) {
      bson_error_t err;
      mongoc_cursor_error(stream->cursor, &err);
      printf("Got error code %d with message %s\n", err.code, err.code ? err.message : "");
      /* TODO: Check if this is a resumable error, don't just blindly retry once. */

      /* TODO: should kill cursor, retry establishing the cursor */
      /*
       * Any error encountered which is not a server error, with the exception
       * of server responses with the message “not master” or error code 43 (cursor not found).
       * An example might be a timeout error, or network error.
       */
      /*
       * Once a ChangeStream has encountered a resumable error, it MUST attempt to resume one time. The process for resuming MUST follow these steps:
         - Perform server selection
         - Connect to selected server
         - Execute the known aggregation command, specifying a resumeAfter with the last known resumeToken
         A driver SHOULD attempt to kill the cursor on the server on which the cursor is opened during the resume process, and MUST NOT attempt to kill the cursor on any other server.
       */
      if (bson_empty(&stream->resume_token)) {
         // Pass the resume token if we have one.
      }
   } else {
      bson_iter_t iter, resume_token_iter; /* I bet commas are c99 */
      if (!bson_iter_init_find(&iter, *bson, "_id"))
      {
         stream->err = MONGOC_ERROR_CHANGE_STREAM_NO_RESUME_TOKEN;
         return false;
      }

      /* We need to copy the resume token, as the next call to mongoc_cursor_next invalidates */

      bson_destroy(&stream->resume_token);
      bson_init(&stream->resume_token);

      /* This has the unneeded _id key, but it seemed more straightforward */
      bson_append_value(bson_iter_value(&iter), "_id", -1, &stream->resume_token);
   }
   return succeeded;
}

bool mongoc_change_stream_error(const mongoc_change_stream_t* stream, bson_error_t* err) {
   /* if we have change stream specific errors, return them first */
   if (stream->err) {
      switch (stream->err) {
      case MONGOC_ERROR_CHANGE_STREAM_NO_RESUME_TOKEN:
         bson_set_error (&stream->err, MONGOC_ERROR_CURSOR, stream->err, "Cannot provide resume functionality when the resume token is missing");
         return true;
      default:
         BSON_ASSERT(false); /* oops... */
      }
   }

   return mongoc_cursor_error(stream->cursor, err);
}

void mongoc_change_stream_destroy(mongoc_change_stream_t* stream) {
   bson_destroy(&stream->resume_token);
   bson_destroy(&stream->pipeline);
   bson_destroy(&stream->agg_opts);
   mongoc_cursor_destroy(stream->cursor);
   bson_free(stream);
}

static void _mongoc_change_stream_make_cursor(mongoc_change_stream_t* stream) {
   /* use the collection read preferences */
   stream->cursor = mongoc_collection_aggregate(stream->coll, MONGOC_QUERY_TAILABLE_CURSOR | MONGOC_QUERY_AWAIT_DATA, &stream->pipeline, &stream->agg_opts, NULL);
   if (stream->maxAwaitTimeMS >= 0) {
      mongoc_cursor_set_max_await_time_ms (stream->cursor,
                                           stream->maxAwaitTimeMS);
   }
}

/* TODO: Should I avoid copying? */
/* TODO: spec says Collection::Watch should be as similar to Collection::Aggregate as possible
 * but being required to pass an empty BSON for the pipeline is annoying.
 */
mongoc_change_stream_t* _mongoc_change_stream_new(const mongoc_collection_t* coll,
                                                  const bson_t* pipeline,
                                                  const bson_t* opts)
{
   bson_iter_t iter;
   bson_t change_stream_stage;

   BSON_ASSERT(coll);

   mongoc_change_stream_t* stream = (mongoc_change_stream_t*)bson_malloc(sizeof(mongoc_change_stream_t));
   stream->maxAwaitTimeMS = -1;
   stream->coll = mongoc_collection_copy(coll);
   bson_init(&stream->agg_opts);
   bson_init(&stream->pipeline);
   bson_init(&stream->resume_token); /* gets destroyed on first iteration */
   stream->read_prefs = mongoc_read_prefs_copy(mongoc_collection_get_read_prefs(coll));

   /*
    * opts consists of:
    * fullDocument = 'default'|'updateLookup', passed to $changeStream stage
    * resumeAfter = optional<Doc>, passed to $changeStream stage
    * maxAwaitTimeMS: Optional<Int64>, passed to cursor
    * batchSize: Optional<Int32>, passed as agg option
    * collation: Optional<Document>, passed as agg option
    *
    * validation is done on the server.
    */

   bson_init(&change_stream_stage);
   BCON_APPEND (&change_stream_stage, "$changeStream", "{", "}");

   if (opts) {
      bson_iter_init(&iter, opts);

      /* Add fullDocument and/or resumeAfter to $changeStream if they exist */
      if (bson_iter_init_find (&iter, opts, "fullDocument")) {
         /* Append regardless of the BSON type. The server will handle bad input. */
         bson_append_value (&change_stream_stage, "fullDocument", -1,
                            bson_iter_value (&iter));
      }

      if (bson_iter_init_find (&iter, opts, "resumeAfter")) {
         /* TODO: check if this returns false. Return null then? */
         bson_append_value (&change_stream_stage, "resumeAfter", -1,
                            bson_iter_value (&iter));
      }

      if (bson_iter_init_find (&iter, opts, "batchSize")) {
         bson_append_value (&stream->agg_opts, "batchSize", -1,
                            bson_iter_value (&iter));
      }

      if (bson_iter_init_find (&iter, opts, "collation")) {
         bson_append_value (&stream->agg_opts, "collation", -1,
                            bson_iter_value (&iter));
      }

      if (bson_iter_init_find (&iter, opts, "maxAwaitTimeMS")) {
         stream->maxAwaitTimeMS = bson_iter_int32(&iter);
      }
   }


   BCON_APPEND (&stream->pipeline, "pipeline", "[", BCON_DOCUMENT (&change_stream_stage), "]");
   bson_destroy(&change_stream_stage);

   /* Append user pipeline if it exists */
   if (pipeline) {
      BSON_APPEND_ARRAY(&stream->pipeline, "pipeline", pipeline);
   }

   _mongoc_change_stream_make_cursor(stream);

   return stream;
}