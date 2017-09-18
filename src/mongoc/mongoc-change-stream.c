#include <bson.h>
#include "mongoc-change-stream.h"
#include "mongoc-error.h"
#include "mongoc-cursor.h"
#include "mongoc-cursor-private.h"

#define SET_BSON_ERR(_str)                 \
   bson_set_error (&stream->err,           \
                   MONGOC_ERROR_CURSOR,    \
                   MONGOC_ERROR_BSON,      \
                   "Could not set " _str); \
   return false;


#define SET_BSON_OR_ERR(_dst, _str)                                   \
   do {                                                               \
      if (!BSON_APPEND_VALUE (_dst, _str, bson_iter_value (&iter))) { \
         SET_BSON_ERR (_str);                                         \
      }                                                               \
   } while (0);


/* TODO: move this to private */
struct _mongoc_change_stream_t {
   /* lightly parsed user options */
   bson_t appended_pipeline;
   bson_t change_stream_stage_opts;
   bson_t agg_opts;
   bson_t resume_token; /* empty, or has resumeAfter: doc */

   bool err_occurred; /* set if any error occurs */
   bson_error_t err;
   bson_t err_doc;

   mongoc_cursor_t *cursor; /* will not be null */
   mongoc_collection_t *coll;
   int64_t maxAwaitTimeMS;
};

static void
_mongoc_change_stream_make_cursor (mongoc_change_stream_t *stream)
{
   bson_t *change_stream_stage; /* { $changeStream: <change_stream_doc> } */
   bson_t change_stream_doc;   /* { pipeline: <pipeline_array> */
   bson_t pipeline_array;
   bson_t pipeline;
   bson_iter_t iter;

   /* We construct the pipeline here, since we need to reconstruct when
    * we retry to add the updated resume token
    */
   if (!bson_empty (&stream->change_stream_stage_opts)) {
      bson_copy_to (&stream->change_stream_stage_opts, &change_stream_doc);
   } else {
      bson_init (&change_stream_doc);
   }

   if (!bson_empty (&stream->resume_token)) {
      bson_concat (&change_stream_doc, &stream->resume_token);
   }

   bson_init (&pipeline);
   bson_append_array_begin (&pipeline, "pipeline", -1, &pipeline_array);

   change_stream_stage = BCON_NEW("$changeStream", BCON_DOCUMENT(&change_stream_doc));
   BSON_APPEND_DOCUMENT(&pipeline_array, "0", change_stream_stage);
   bson_destroy (&change_stream_doc);
   bson_destroy (change_stream_stage);

   /* Append user pipeline if it exists */
   if (bson_iter_init_find(&iter, &stream->appended_pipeline, "pipeline") && BSON_ITER_HOLDS_ARRAY(&iter)) {
      bson_iter_t child_iter;
      uint32_t key_int = 1;
      char buf[16];
      const char *key_str;

      bson_iter_recurse(&iter, &child_iter);
      while (bson_iter_next(&child_iter)) {
         if (BSON_ITER_HOLDS_DOCUMENT(&child_iter)) {
            size_t keyLen = bson_uint32_to_string(key_int, &key_str, buf, sizeof(buf));
            bson_append_value(&pipeline_array, key_str, keyLen, bson_iter_value(&child_iter));
            ++key_int;
         }
      }
   }

   bson_append_array_end (&pipeline, &pipeline_array);

   stream->cursor = mongoc_collection_aggregate (stream->coll,
                                                 MONGOC_QUERY_TAILABLE_CURSOR |
                                                    MONGOC_QUERY_AWAIT_DATA,
                                                 &pipeline,
                                                 &stream->agg_opts,
                                                 NULL);
   if (stream->maxAwaitTimeMS >= 0) {
      _mongoc_cursor_set_opt_int64 (stream->cursor,
                                    MONGOC_CURSOR_MAX_AWAIT_TIME_MS,
                                    stream->maxAwaitTimeMS);
   }
}

bool
mongoc_change_stream_next (mongoc_change_stream_t *stream, const bson_t **bson)
{
   BSON_ASSERT (stream);
   BSON_ASSERT (bson);

   if (stream->err_occurred) {
      return false;
   }

   if (!mongoc_cursor_next (stream->cursor, bson)) {
      const bson_t *err_doc;
      bson_error_t err;
      bool resumable = false;

      if (!mongoc_cursor_error_document (stream->cursor, &err, &err_doc)) {
         /* No error occurred, just no documents left */
         return false;
      }

      printf ("Got error\n");

      /* An error is resumable if it is not a server error, or if it has error
       * code 43 (cursor not found) or is "not master" */
      if (!bson_empty (err_doc)) {
         /* This is a server error */
         bson_iter_t iter;
         if (bson_iter_init_find (&iter, err_doc, "errmsg") &&
             BSON_ITER_HOLDS_UTF8 (&iter)) {
            uint32_t len;
            const char *errmsg = bson_iter_utf8 (&iter, &len);
            if (bson_utf8_validate (errmsg, len, false) &&
                strncmp (errmsg, "not master", len) == 0) {
               resumable = true;
            }
         }

         if (bson_iter_init_find (&iter, err_doc, "code") &&
             BSON_ITER_HOLDS_INT (&iter)) {
            if (bson_iter_int64 (&iter) == 43) {
               resumable = true;
            }
         }
      } else {
         /* This is a client error */
         resumable = true;
      }

      if (resumable) {
         printf ("Trying to resume\n");
         mongoc_cursor_destroy (stream->cursor);
         _mongoc_change_stream_make_cursor (stream);
         if (!mongoc_cursor_next (stream->cursor, bson)) {
            resumable =
               !mongoc_cursor_error_document (stream->cursor, &err, &err_doc);
            if (resumable) {
               // Empty batch.
               return false;
            }
         }
      }

      if (!resumable) {
         printf("Unrecoverable error\n");
         stream->err_occurred = true;
         stream->err = err;
         bson_destroy (&stream->err_doc);
         bson_copy_to (err_doc, &stream->err_doc);
         return false;
      }
   }

   /* We have received documents, either from the first call to next
    * or after a resume. */
   bson_iter_t iter;
   if (!bson_iter_init_find (&iter, *bson, "_id")) {
      stream->err_occurred = true;
      bson_set_error (&stream->err,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_CHANGE_STREAM_NO_RESUME_TOKEN,
                      "Cannot provide resume functionality when the resume "
                      "token is missing");
      return false;
   }

   /* We need to copy the resume token, as the next call to mongoc_cursor_next
    * invalidates */

   bson_destroy (&stream->resume_token);
   bson_init (&stream->resume_token);
   BSON_APPEND_VALUE (
      &stream->resume_token, "resumeAfter", bson_iter_value (&iter));
   return true;
}

bool
mongoc_change_stream_error_document (const mongoc_change_stream_t *stream,
                                     bson_error_t *err,
                                     const bson_t **bson)
{
   /* if we have change stream specific errors, return them first */
   if (stream->err_occurred) {
      if (err) {
         *err = stream->err;
      }
      if (bson) {
         *bson = &stream->err_doc;
      }
      return true;
   }
   return false;
}

void
mongoc_change_stream_destroy (mongoc_change_stream_t *stream)
{
   bson_destroy (&stream->appended_pipeline);
   bson_destroy (&stream->change_stream_stage_opts);
   bson_destroy (&stream->agg_opts);
   bson_destroy (&stream->resume_token);
   bson_destroy (&stream->err_doc);
   mongoc_cursor_destroy (stream->cursor);
   mongoc_collection_destroy(stream->coll);
   bson_free (stream);
}

mongoc_change_stream_t *
_mongoc_change_stream_new (const mongoc_collection_t *coll,
                           const bson_t *pipeline,
                           const bson_t *opts)
{
   BSON_ASSERT (coll);
   BSON_ASSERT (pipeline);

   mongoc_change_stream_t *stream =
      (mongoc_change_stream_t *) bson_malloc (sizeof (mongoc_change_stream_t));
   stream->maxAwaitTimeMS = -1;
   stream->coll = mongoc_collection_copy (coll);
   bson_init (&stream->appended_pipeline);
   bson_init (&stream->change_stream_stage_opts);
   bson_init (&stream->agg_opts);
   bson_init (&stream->resume_token);
   bson_init (&stream->err_doc);
   memset (&stream->err, 0, sizeof (bson_error_t));
   stream->cursor = NULL;
   stream->err_occurred = false;

   /*
    * The passed options may consist of:
    * fullDocument: 'default'|'updateLookup', passed to $changeStream stage
    * resumeAfter: optional<Doc>, passed to $changeStream stage
    * maxAwaitTimeMS: Optional<Int64>, passed to cursor
    * batchSize: Optional<Int32>, passed as agg option, {cursor: { batchSize: }}
    * collation: Optional<Document>, passed as agg option
    */

   if (opts) {
      bson_iter_t iter;
      bson_iter_init (&iter, opts);

      if (bson_iter_init_find (&iter, opts, "fullDocument")) {
         SET_BSON_OR_ERR (&stream->change_stream_stage_opts, "fullDocument");
      } else {
         if (!BSON_APPEND_UTF8 (
                &stream->change_stream_stage_opts, "fullDocument", "default")) {
            SET_BSON_ERR ("fullDocument");
         }
      }

      if (bson_iter_init_find (&iter, opts, "resumeAfter")) {
         SET_BSON_OR_ERR (&stream->resume_token, "resumeAfter");
      }

      if (bson_iter_init_find (&iter, opts, "batchSize")) {
         /* TODO: mongoc_collection_aggregate appends the cursor subdoc,
          * document? */
         SET_BSON_OR_ERR (&stream->agg_opts, "batchSize");
      }

      if (bson_iter_init_find (&iter, opts, "collation")) {
         SET_BSON_OR_ERR (&stream->agg_opts, "collation");
      }

      if (bson_iter_init_find (&iter, opts, "maxAwaitTimeMS")) {
         /* Accept either int32 or int64 */
         if (BSON_ITER_HOLDS_INT32 (&iter)) {
            stream->maxAwaitTimeMS = bson_iter_int32 (&iter);
         } else if (BSON_ITER_HOLDS_INT64 (&iter)) {
            stream->maxAwaitTimeMS = bson_iter_int64 (&iter);
         }
      }
   }

   if (!bson_empty(pipeline)) {
      bson_iter_t iter;
      if (bson_iter_init_find (&iter, pipeline, "pipeline")) {
         SET_BSON_OR_ERR (&stream->appended_pipeline, "pipeline");
      }
   }

   if (!stream->err_occurred) {
      _mongoc_change_stream_make_cursor (stream);
   }

   return stream;
}

#undef SET_BSON_OR_ERR