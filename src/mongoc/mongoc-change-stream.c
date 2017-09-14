#include <bson.h>
#include "mongoc-change-stream.h"
#include "mongoc-error.h"
#include "mongoc-cursor.h"
#include "mongoc-cursor-private.h"


struct _mongoc_change_stream_t {
   /* lightly parsed user options */
   bson_t appended_pipeline;
   bson_t change_stream_stage_opts;
   bson_t agg_opts;
   bson_t resume_token; /* current resume token, or user supplied */

   mongoc_error_code_t err; /* defer creating error strings until user needs to retrieve it */
   mongoc_cursor_t* cursor;
   mongoc_collection_t* coll;
   mongoc_read_prefs_t* read_prefs;
   int64_t maxAwaitTimeMS;
};

static void _mongoc_change_stream_make_cursor(mongoc_change_stream_t* stream) {
   bson_t change_stream_stage;
   bson_t pipeline;

   /* We construct the pipeline here, since we need to reconstruct when
    * we retry to add the updated resume token
    */
   if (!bson_empty(&stream->change_stream_stage_opts)) {
      bson_copy_to(&stream->change_stream_stage_opts, &change_stream_stage);
   }
   else {
      bson_init(&change_stream_stage);
   }

   if (!bson_empty(&stream->resume_token)) {
      bson_iter_t iter;
      BSON_ASSERT(bson_iter_init_find(&iter, &stream->resume_token, "_id"));
      BSON_APPEND_VALUE(&change_stream_stage, "resumeAfter", bson_iter_value(&iter));
   }

   bson_init(&pipeline);
   BCON_APPEND (&pipeline, "pipeline", "[", "{", "$changeStream", BCON_DOCUMENT (&change_stream_stage), "}", "]");
   bson_destroy(&change_stream_stage);

   /* Append user pipeline if it exists */
   if (!bson_empty(&stream->appended_pipeline)) {
      BSON_APPEND_ARRAY(&pipeline, "pipeline", &stream->appended_pipeline);
   }

   /* use the collection read preferences */
   printf("Creating a cursor\n");
   /* Setting MONGOC_QUERY_TAILABLE_CURSOR|MONGOC_QUERY_AWAIT_DATA doesn't really matter, except the cursor
    * will not send the maxAwaitTimeMS value on GetMores unless these are set. TODO: should we pass just MONGOC_QUERY_NONE and have maxAwaitTimeMS sent by default?
    */
   /* For agg, maxTimeMS is max processing time, it will not await data. */
   stream->cursor = mongoc_collection_aggregate(stream->coll, MONGOC_QUERY_TAILABLE_CURSOR|MONGOC_QUERY_AWAIT_DATA, &pipeline, &stream->agg_opts, NULL);
   if (stream->maxAwaitTimeMS >= 0) {
      /* mongoc_cursor_set_max_await_time_ms takes an int32, so avoid it. */
      _mongoc_cursor_set_opt_int64 (stream->cursor, MONGOC_CURSOR_MAX_AWAIT_TIME_MS, stream->maxAwaitTimeMS);
   }
}

bool mongoc_change_stream_next(mongoc_change_stream_t* stream, const bson_t **bson) {
   BSON_ASSERT(bson);

   if (!mongoc_cursor_next(stream->cursor, bson)) {
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
      if (/* TODO: is resumable error */ err.code) {
         printf("Trying to resume");
         mongoc_cursor_destroy (stream->cursor);
         _mongoc_change_stream_make_cursor (stream);
         if (!mongoc_cursor_next(stream->cursor, bson)) {
            /* will not retry again */
            if (mongoc_cursor_error(stream->cursor, &err)) {
               return false;
            }
         }

      }
      return false;
   }

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
   BSON_APPEND_VALUE(&stream->resume_token, "_id", bson_iter_value(&iter));
   return true;
}

bool mongoc_change_stream_error(const mongoc_change_stream_t* stream, bson_error_t* err) {
   /* if we have change stream specific errors, return them first */
   if (stream->err) {
      switch (stream->err) {
      case MONGOC_ERROR_CHANGE_STREAM_NO_RESUME_TOKEN:
         bson_set_error (err, MONGOC_ERROR_CURSOR, stream->err, "Cannot provide resume functionality when the resume token is missing");
         return true;
      default:
         BSON_ASSERT(false); /* oops... */
      }
   }

   return mongoc_cursor_error(stream->cursor, err);
}

void mongoc_change_stream_destroy(mongoc_change_stream_t* stream) {
   bson_destroy(&stream->resume_token);
   bson_destroy(&stream->appended_pipeline);
   bson_destroy(&stream->agg_opts);
   mongoc_cursor_destroy(stream->cursor);
   bson_free(stream);
}

/* TODO: Should I avoid copying pipeline and opts? */
/* TODO: spec says Collection::Watch should be as similar to Collection::Aggregate as possible
 * but being required to pass an empty BSON for the pipeline is annoying.
 */
mongoc_change_stream_t* _mongoc_change_stream_new(const mongoc_collection_t* coll,
                                                  const bson_t* pipeline,
                                                  const bson_t* opts)
{
   BSON_ASSERT(coll);
   mongoc_change_stream_t* stream = (mongoc_change_stream_t*)bson_malloc(sizeof(mongoc_change_stream_t));
   stream->maxAwaitTimeMS = -1;
   stream->coll = mongoc_collection_copy(coll);
   bson_init(&stream->change_stream_stage_opts);
   bson_init(&stream->agg_opts);
   if (pipeline) {
      bson_copy_to(pipeline, &stream->appended_pipeline);
   } else {
      bson_init(&stream->appended_pipeline);
   }
   bson_init(&stream->resume_token); /* gets destroyed on first iteration */
   stream->read_prefs = mongoc_read_prefs_copy(mongoc_collection_get_read_prefs(coll));

   /*
    * opts consists of:
    * fullDocument = 'default'|'updateLookup', passed to $changeStream stage
    * resumeAfter = optional<Doc>, passed to $changeStream stage
    * maxAwaitTimeMS: Optional<Int64>, passed to cursor
    * batchSize: Optional<Int32>, passed as agg option, {cursor: { batchSize: }}
    * collation: Optional<Document>, passed as agg option
    *
    * validation is done on the server.
    */

   if (opts) {
      bson_iter_t iter;
      bson_iter_init(&iter, opts);

      /* Add fullDocument and/or resumeAfter to $changeStream if they exist */
      if (bson_iter_init_find (&iter, opts, "fullDocument")) {
         /* Append regardless of the BSON type. The server will handle bad input. */
         BSON_APPEND_VALUE (&stream->change_stream_stage_opts, "fullDocument",
                            bson_iter_value (&iter));
      } else {
         BSON_APPEND_UTF8 (&stream->change_stream_stage_opts, "fullDocument", "default");
      }

      if (bson_iter_init_find (&iter, opts, "resumeAfter")) {
         /* TODO: check if this returns false. Return null then? */
         BSON_APPEND_VALUE (&stream->resume_token, "_id", bson_iter_value (&iter));
      }

      if (bson_iter_init_find (&iter, opts, "batchSize")) {
         /* TODO: mongoc_collection_aggregate will append a cursor subdoc, we should document this unless this is expected. */
         BSON_APPEND_VALUE(&stream->agg_opts, "batchSize", bson_iter_value(&iter));
//         bson_t cursor_doc;
//         bson_init(&cursor_doc);
//         bson_append_value(&cursor_doc, "batchSize", -1, bson_iter_value (&iter));
//         bson_append_document (&stream->agg_opts, "cursor", -1, &cursor_doc);
//         bson_destroy(&cursor_doc);
      }

      if (bson_iter_init_find (&iter, opts, "collation")) {
         BSON_APPEND_VALUE (&stream->agg_opts, "collation",
                            bson_iter_value (&iter));
      }

      if (bson_iter_init_find (&iter, opts, "maxAwaitTimeMS")) {
         /* Accept either int32 or int64 */
         if (BSON_ITER_HOLDS_INT32(&iter)) {
            stream->maxAwaitTimeMS = bson_iter_int32(&iter);
         }
         else if (BSON_ITER_HOLDS_INT64(&iter)) {
            stream->maxAwaitTimeMS = bson_iter_int64(&iter);
         }
      }
   }

   _mongoc_change_stream_make_cursor(stream);

   return stream;
}