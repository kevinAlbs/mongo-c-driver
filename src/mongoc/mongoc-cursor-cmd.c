/*
 * Copyright 2018-present MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "mongoc.h"
#include "mongoc-cursor-private.h"
#include "mongoc-client-private.h"

typedef enum { NONE, DOC, STREAM } reading_from_t;
typedef enum { UNKNOWN, GETMORE_CMD, OP_GETMORE } getmore_type_t;

typedef struct _data_cmd_t {
   /* Two paths:
    * - Mongo 3.2+, sent "getMore" cmd, we're reading reply's "nextBatch" array
    * - Mongo 2.6 to 3, after "aggregate" or similar command we sent OP_GETMORE,
    *   we're reading the raw reply from a stream
    */
   mongoc_cursor_batch_reader_t reader;
   reading_from_t reading_from;
   getmore_type_t getmore_type; /* cache after first getmore. */
} data_cmd_t;


static bool
_use_getmore_cmd (mongoc_cursor_t *cursor)
{
   mongoc_server_stream_t *server_stream;
   bool ret;
   data_cmd_t *data = (data_cmd_t *) cursor->ctx.data;
   if (data->getmore_type != UNKNOWN) {
      return data->getmore_type == GETMORE_CMD;
   }
   server_stream = _mongoc_cursor_fetch_stream (cursor);
   ret = server_stream->sd->max_wire_version >= WIRE_VERSION_FIND_CMD &&
         !_mongoc_cursor_get_opt_bool (cursor, MONGOC_CURSOR_EXHAUST);
   data->getmore_type = ret ? GETMORE_CMD : OP_GETMORE;
   return ret;
}

static void
_destroy (mongoc_cursor_context_t *ctx)
{
   data_cmd_t *data = (data_cmd_t *) ctx->data;
   bson_destroy (&data->reader.reply);
}


static void
_prime (mongoc_cursor_t *cursor)
{
   data_cmd_t *data = (data_cmd_t *) cursor->ctx.data;
   bson_t copied_opts;
   bson_init (&copied_opts);

   cursor->operation_id = ++cursor->client->cluster.operation_id;
   /* commands have a cursor field, so copy opts without "batchSize" */
   bson_copy_to_excluding_noinit (
      &cursor->opts, &copied_opts, "batchSize", NULL);

   /* TODO: if filter is placed in context, rename to command. */
   /* server replies to aggregate/listIndexes/listCollections with:
    * {cursor: {id: N, firstBatch: []}} */
   _mongoc_cursor_batch_reader_refresh (
      cursor, &cursor->filter, &copied_opts, &data->reader);
   data->reading_from = DOC;
   bson_destroy (&copied_opts);
}


static void
_pop_from_batch (mongoc_cursor_t *cursor, const bson_t **out)
{
   data_cmd_t *data = (data_cmd_t *) cursor->ctx.data;
   const bson_t *bson;
   bool eof;

   switch (data->reading_from) {
   case DOC:
      _mongoc_cursor_batch_reader_read (cursor, &data->reader, out);
      return;
   case STREAM:
      bson = bson_reader_read (cursor->legacy_response.reader, &eof);
      if (out) {
         *out = bson;
      }

      if (eof) {
         cursor->state = cursor->cursor_id ? END_OF_BATCH : DONE;
      } else if (!bson) {
         cursor->state = DONE;
      }
      return;
   case NONE:
      fprintf (stderr, "trying to pop from an uninitialized cursor reader.\n");
      BSON_ASSERT (false);
   }
}


static void
_get_next_batch (mongoc_cursor_t *cursor)
{
   data_cmd_t *data = (data_cmd_t *) cursor->ctx.data;
   bson_t getmore_cmd;

   if (_use_getmore_cmd (cursor)) {
      _mongoc_cursor_prepare_getmore_command (cursor, &getmore_cmd);
      _mongoc_cursor_batch_reader_refresh (
         cursor, &getmore_cmd, NULL /* opts */, &data->reader);
      bson_destroy (&getmore_cmd);
      data->reading_from = DOC;
   } else {
      if (_mongoc_cursor_op_getmore (cursor, NULL)) {
         cursor->state = IN_BATCH;
      } else {
         cursor->state = DONE;
      }
      data->reading_from = STREAM;
   }
}


static void
_clone (mongoc_cursor_context_t *dst, const mongoc_cursor_context_t *src)
{
   data_cmd_t *data = bson_malloc0 (sizeof (*data));
   bson_init (&data->reader.reply);
   dst->data = data;
}


mongoc_cursor_t *
_mongoc_cursor_cmd_new (mongoc_client_t *client,
                        const char *db_and_coll,
                        const bson_t *cmd,
                        const bson_t *opts,
                        const mongoc_read_prefs_t *read_prefs,
                        const mongoc_read_concern_t *read_concern)
{
   mongoc_cursor_t *cursor;
   data_cmd_t *data = bson_malloc0 (sizeof (*data));

   cursor = _mongoc_cursor_new_with_opts (
      client, db_and_coll, cmd, opts, read_prefs, read_concern);
   bson_init (&data->reader.reply);
   cursor->ctx.prime = _prime;
   cursor->ctx.pop_from_batch = _pop_from_batch;
   cursor->ctx.get_next_batch = _get_next_batch;
   cursor->ctx.destroy = _destroy;
   cursor->ctx.clone = _clone;
   cursor->ctx.data = (void *) data;
   return cursor;
}

mongoc_cursor_t *
_mongoc_cursor_cmd_new_from_reply (mongoc_client_t *client,
                                   const bson_t *cmd,
                                   const bson_t *opts,
                                   bson_t *reply,
                                   uint32_t server_id)
{
   mongoc_cursor_t *cursor =
      _mongoc_cursor_cmd_new (client, NULL, cmd, opts, NULL, NULL);
   data_cmd_t *data = (data_cmd_t *) cursor->ctx.data;
   cursor->state = IN_BATCH;
   cursor->server_id = server_id;
   data->reading_from = DOC;
   bson_destroy (&data->reader.reply);
   if (!bson_steal (&data->reader.reply, reply)) {
      bson_destroy (&data->reader.reply);
      BSON_ASSERT (bson_steal (&data->reader.reply, bson_copy (reply)));
   }

   if (!_mongoc_cursor_batch_reader_start (cursor, &data->reader)) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                      "Couldn't parse cursor document");
   }
   return cursor;
}