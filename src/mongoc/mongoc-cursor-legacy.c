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

/* cursor functions for pre-3.2 MongoDB, including:
 * - OP_QUERY find (superseded by the find command)
 * - OP_GETMORE (superseded by the getMore command)
 * - receiving OP_REPLY documents in a stream (instead of batch)
 */

#include "mongoc-cursor.h"
#include "mongoc-cursor-private.h"
#include "mongoc-client-private.h"
#include "mongoc-counters-private.h"
#include "mongoc-error.h"
#include "mongoc-log.h"
#include "mongoc-trace-private.h"
#include "mongoc-cursor-cursorid-private.h"
#include "mongoc-read-concern-private.h"
#include "mongoc-util-private.h"
#include "mongoc-write-concern-private.h"
#include "mongoc-read-prefs-private.h"
#include "mongoc-rpc-private.h"


static bool
_mongoc_cursor_monitor_legacy_get_more (mongoc_cursor_t *cursor,
                                        mongoc_server_stream_t *server_stream)
{
   bson_t doc;
   char db[MONGOC_NAMESPACE_MAX];
   mongoc_client_t *client;
   mongoc_apm_command_started_t event;

   ENTRY;

   client = cursor->client;
   if (!client->apm_callbacks.started) {
      /* successful */
      RETURN (true);
   }

   bson_init (&doc);
   if (!_mongoc_cursor_prepare_getmore_command (cursor, &doc)) {
      bson_destroy (&doc);
      RETURN (false);
   }

   bson_strncpy (db, cursor->ns, cursor->dblen + 1);
   mongoc_apm_command_started_init (&event,
                                    &doc,
                                    db,
                                    "getMore",
                                    client->cluster.request_id,
                                    cursor->operation_id,
                                    &server_stream->sd->host,
                                    server_stream->sd->id,
                                    client->apm_context);

   client->apm_callbacks.started (&event);
   mongoc_apm_command_started_cleanup (&event);
   bson_destroy (&doc);

   RETURN (true);
}


static bool
_mongoc_cursor_monitor_legacy_query (mongoc_cursor_t *cursor,
                                     mongoc_server_stream_t *server_stream)
{
   bson_t doc;
   mongoc_client_t *client;
   char db[MONGOC_NAMESPACE_MAX];
   bool r;

   ENTRY;

   client = cursor->client;
   if (!client->apm_callbacks.started) {
      /* successful */
      RETURN (true);
   }

   bson_init (&doc);
   bson_strncpy (db, cursor->ns, cursor->dblen + 1);

   /* simulate a MongoDB 3.2+ "find" command */
   _mongoc_cursor_prepare_find_command (cursor, &doc);

   bson_copy_to_excluding_noinit (
      &cursor->opts, &doc, "serverId", "maxAwaitTimeMS", "sessionId", NULL);

   r = _mongoc_cursor_monitor_command (cursor, server_stream, &doc, "find");

   bson_destroy (&doc);

   RETURN (r);
}


bool
_mongoc_read_from_buffer (mongoc_cursor_t *cursor, const bson_t **bson)
{
   bool eof = false;

   BSON_ASSERT (cursor->legacy_response.reader);

   *bson = bson_reader_read (cursor->legacy_response.reader, &eof);
   cursor->state = eof ? END_OF_BATCH : IN_BATCH;

   return *bson ? true : false;
}


bool
_mongoc_cursor_next (mongoc_cursor_t *cursor, const bson_t **bson)
{
   int64_t limit;
   const bson_t *b = NULL;
   bool tailable;

   ENTRY;

   BSON_ASSERT (cursor);

   if (bson) {
      *bson = NULL;
   }

   /*
    * If we reached our limit, make sure we mark this as done and do not try to
    * make further progress.  We also set end_of_event so that
    * mongoc_cursor_more will be false.
    */
   limit = cursor->is_find ? mongoc_cursor_get_limit (cursor) : 1;

   if (limit && cursor->count >= llabs (limit)) {
      cursor->state = DONE;
      RETURN (false);
   }

   /*
    * Try to read the next document from the reader if it exists, we might
    * get NULL back and EOF, in which case we need to submit a getmore.
    */
   if (cursor->legacy_response.reader) {
      _mongoc_read_from_buffer (cursor, &b);
      if (b) {
         GOTO (complete);
      }
   }

   /*
    * Check to see if we need to send a GET_MORE for more results.
    */
   if (cursor->state == UNPRIMED) {
      b = _mongoc_cursor_initial_query (cursor);
   } else if (BSON_UNLIKELY (cursor->state == END_OF_BATCH) &&
              cursor->cursor_id) {
      b = _mongoc_cursor_get_more (cursor);
   }

complete:
   tailable = _mongoc_cursor_get_opt_bool (cursor, "tailable");
   if (cursor->state == END_OF_BATCH && !tailable) {
      if (cursor->in_exhaust && !cursor->cursor_id) {
         /* the exhaust cursor has received all of the documents. */
         cursor->state = DONE;
      } else if (!b) {
         cursor->state = DONE;
      }
   }

   if (bson) {
      *bson = b;
   }

   RETURN (!!b);
}

bool
_mongoc_cursor_op_getmore (mongoc_cursor_t *cursor,
                           mongoc_server_stream_t *server_stream)
{
   int64_t started;
   mongoc_rpc_t rpc;
   uint32_t request_id;
   mongoc_cluster_t *cluster;
   mongoc_query_flags_t flags;
   bool owns_stream = false;
   bool ret = true;

   ENTRY;

   started = bson_get_monotonic_time ();
   cluster = &cursor->client->cluster;

   if (!server_stream) {
      server_stream = _mongoc_cursor_fetch_stream (cursor);
      owns_stream = true;
   }

   if (!_mongoc_cursor_flags (cursor, server_stream, &flags)) {
      GOTO (fail);
   }

   if (cursor->in_exhaust) {
      request_id = (uint32_t) cursor->legacy_response.rpc.header.request_id;
   } else {
      request_id = ++cluster->request_id;

      rpc.get_more.cursor_id = cursor->cursor_id;
      rpc.header.msg_len = 0;
      rpc.header.request_id = request_id;
      rpc.header.response_to = 0;
      rpc.header.opcode = MONGOC_OPCODE_GET_MORE;
      rpc.get_more.zero = 0;
      rpc.get_more.collection = cursor->ns;

      if (flags & MONGOC_QUERY_TAILABLE_CURSOR) {
         rpc.get_more.n_return = 0;
      } else {
         rpc.get_more.n_return = _mongoc_n_return (false, cursor);
      }

      if (!_mongoc_cursor_monitor_legacy_get_more (cursor, server_stream)) {
         GOTO (done);
      }

      if (!mongoc_cluster_legacy_rpc_sendv_to_server (
             cluster, &rpc, server_stream, &cursor->error)) {
         GOTO (done);
      }
   }

   _mongoc_buffer_clear (&cursor->legacy_response.buffer, false);

   /* reset the last known cursor id. */
   cursor->cursor_id = 0;

   if (!_mongoc_client_recv (cursor->client,
                             &cursor->legacy_response.rpc,
                             &cursor->legacy_response.buffer,
                             server_stream,
                             &cursor->error)) {
      GOTO (done);
   }

   if (cursor->legacy_response.rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Invalid opcode. Expected %d, got %d.",
                      MONGOC_OPCODE_REPLY,
                      cursor->legacy_response.rpc.header.opcode);
      GOTO (done);
   }

   if (cursor->legacy_response.rpc.header.response_to != request_id) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Invalid response_to for getmore. Expected %d, got %d.",
                      request_id,
                      cursor->legacy_response.rpc.header.response_to);
      GOTO (done);
   }

   if (!_mongoc_rpc_check_ok (&cursor->legacy_response.rpc,
                              cursor->client->error_api_version,
                              &cursor->error,
                              &cursor->error_doc)) {
      GOTO (done);
   }

   if (cursor->legacy_response.reader) {
      bson_reader_destroy (cursor->legacy_response.reader);
   }

   cursor->cursor_id = cursor->legacy_response.rpc.reply.cursor_id;

   cursor->legacy_response.reader = bson_reader_new_from_data (
      cursor->legacy_response.rpc.reply.documents,
      (size_t) cursor->legacy_response.rpc.reply.documents_len);

   _mongoc_cursor_monitor_succeeded (cursor,
                                     bson_get_monotonic_time () - started,
                                     false, /* not first batch */
                                     server_stream,
                                     "getMore");

   GOTO (done);

fail:
   _mongoc_cursor_monitor_failed (
      cursor, bson_get_monotonic_time () - started, server_stream, "getMore");
   ret = false;
done:
   if (owns_stream) {
      mongoc_server_stream_cleanup (server_stream);
   }
   RETURN (ret);
}

#define OPT_CHECK(_type)                                         \
   do {                                                          \
      if (!BSON_ITER_HOLDS_##_type (&iter)) {                    \
         bson_set_error (&cursor->error,                         \
                         MONGOC_ERROR_COMMAND,                   \
                         MONGOC_ERROR_COMMAND_INVALID_ARG,       \
                         "invalid option %s, should be type %s", \
                         key,                                    \
                         #_type);                                \
         return NULL;                                            \
      }                                                          \
   } while (false)


#define OPT_CHECK_INT()                                          \
   do {                                                          \
      if (!BSON_ITER_HOLDS_INT (&iter)) {                        \
         bson_set_error (&cursor->error,                         \
                         MONGOC_ERROR_COMMAND,                   \
                         MONGOC_ERROR_COMMAND_INVALID_ARG,       \
                         "invalid option %s, should be integer", \
                         key);                                   \
         return NULL;                                            \
      }                                                          \
   } while (false)


#define OPT_ERR(_msg)                                   \
   do {                                                 \
      bson_set_error (&cursor->error,                   \
                      MONGOC_ERROR_COMMAND,             \
                      MONGOC_ERROR_COMMAND_INVALID_ARG, \
                      _msg);                            \
      return NULL;                                      \
   } while (false)


#define OPT_BSON_ERR(_msg)                                                    \
   do {                                                                       \
      bson_set_error (                                                        \
         &cursor->error, MONGOC_ERROR_BSON, MONGOC_ERROR_BSON_INVALID, _msg); \
      return NULL;                                                            \
   } while (false)


#define OPT_FLAG(_flag)                \
   do {                                \
      OPT_CHECK (BOOL);                \
      if (bson_iter_as_bool (&iter)) { \
         *flags |= _flag;              \
      }                                \
   } while (false)


#define PUSH_DOLLAR_QUERY()                                          \
   do {                                                              \
      if (!pushed_dollar_query) {                                    \
         pushed_dollar_query = true;                                 \
         bson_append_document (query, "$query", 6, &cursor->filter); \
      }                                                              \
   } while (false)


#define OPT_SUBDOCUMENT(_opt_name, _legacy_name)                           \
   do {                                                                    \
      OPT_CHECK (DOCUMENT);                                                \
      bson_iter_document (&iter, &len, &data);                             \
      if (!bson_init_static (&subdocument, data, (size_t) len)) {          \
         OPT_BSON_ERR ("Invalid '" #_opt_name "' subdocument in 'opts'."); \
      }                                                                    \
      BSON_APPEND_DOCUMENT (query, "$" #_legacy_name, &subdocument);       \
   } while (false)

static bson_t *
_mongoc_cursor_parse_opts_for_op_query (mongoc_cursor_t *cursor,
                                        mongoc_server_stream_t *stream,
                                        bson_t *query /* OUT */,
                                        bson_t *fields /* OUT */,
                                        mongoc_query_flags_t *flags /* OUT */,
                                        int32_t *skip /* OUT */)
{
   bool pushed_dollar_query;
   bson_iter_t iter;
   uint32_t len;
   const uint8_t *data;
   bson_t subdocument;
   const char *key;
   char *dollar_modifier;

   *flags = MONGOC_QUERY_NONE;
   *skip = 0;

   /* assume we'll send filter straight to server, like "{a: 1}". if we find an
    * opt we must add, like "sort", we push the query like "$query: {a: 1}",
    * then add a query modifier for the option, in this example "$orderby".
    */
   pushed_dollar_query = false;

   if (!bson_iter_init (&iter, &cursor->opts)) {
      OPT_BSON_ERR ("Invalid 'opts' parameter.");
   }

   while (bson_iter_next (&iter)) {
      key = bson_iter_key (&iter);

      /* most common options first */
      if (!strcmp (key, MONGOC_CURSOR_PROJECTION)) {
         OPT_CHECK (DOCUMENT);
         bson_iter_document (&iter, &len, &data);
         if (!bson_init_static (&subdocument, data, (size_t) len)) {
            OPT_BSON_ERR ("Invalid 'projection' subdocument in 'opts'.");
         }
         bson_destroy (fields);
         bson_copy_to (&subdocument, fields);
      } else if (!strcmp (key, MONGOC_CURSOR_SORT)) {
         PUSH_DOLLAR_QUERY ();
         OPT_SUBDOCUMENT (sort, orderby);
      } else if (!strcmp (key, MONGOC_CURSOR_SKIP)) {
         OPT_CHECK_INT ();
         *skip = (int32_t) bson_iter_as_int64 (&iter);
      }
      /* the rest of the options, alphabetically */
      else if (!strcmp (key, MONGOC_CURSOR_ALLOW_PARTIAL_RESULTS)) {
         OPT_FLAG (MONGOC_QUERY_PARTIAL);
      } else if (!strcmp (key, MONGOC_CURSOR_AWAIT_DATA)) {
         OPT_FLAG (MONGOC_QUERY_AWAIT_DATA);
      } else if (!strcmp (key, MONGOC_CURSOR_COMMENT)) {
         OPT_CHECK (UTF8);
         PUSH_DOLLAR_QUERY ();
         BSON_APPEND_UTF8 (query, "$comment", bson_iter_utf8 (&iter, NULL));
      } else if (!strcmp (key, MONGOC_CURSOR_HINT)) {
         if (BSON_ITER_HOLDS_UTF8 (&iter)) {
            PUSH_DOLLAR_QUERY ();
            BSON_APPEND_UTF8 (query, "$hint", bson_iter_utf8 (&iter, NULL));
         } else if (BSON_ITER_HOLDS_DOCUMENT (&iter)) {
            PUSH_DOLLAR_QUERY ();
            OPT_SUBDOCUMENT (hint, hint);
         } else {
            OPT_ERR ("Wrong type for 'hint' field in 'opts'.");
         }
      } else if (!strcmp (key, MONGOC_CURSOR_MAX)) {
         PUSH_DOLLAR_QUERY ();
         OPT_SUBDOCUMENT (max, max);
      } else if (!strcmp (key, MONGOC_CURSOR_MAX_SCAN)) {
         OPT_CHECK_INT ();
         PUSH_DOLLAR_QUERY ();
         BSON_APPEND_INT64 (query, "$maxScan", bson_iter_as_int64 (&iter));
      } else if (!strcmp (key, MONGOC_CURSOR_MAX_TIME_MS)) {
         OPT_CHECK_INT ();
         PUSH_DOLLAR_QUERY ();
         BSON_APPEND_INT64 (query, "$maxTimeMS", bson_iter_as_int64 (&iter));
      } else if (!strcmp (key, MONGOC_CURSOR_MIN)) {
         PUSH_DOLLAR_QUERY ();
         OPT_SUBDOCUMENT (min, min);
      } else if (!strcmp (key, MONGOC_CURSOR_READ_CONCERN)) {
         OPT_ERR ("Set readConcern on client, database, or collection,"
                  " not in a query.");
      } else if (!strcmp (key, MONGOC_CURSOR_RETURN_KEY)) {
         OPT_CHECK (BOOL);
         PUSH_DOLLAR_QUERY ();
         BSON_APPEND_BOOL (query, "$returnKey", bson_iter_as_bool (&iter));
      } else if (!strcmp (key, MONGOC_CURSOR_SHOW_RECORD_ID)) {
         OPT_CHECK (BOOL);
         PUSH_DOLLAR_QUERY ();
         BSON_APPEND_BOOL (query, "$showDiskLoc", bson_iter_as_bool (&iter));
      } else if (!strcmp (key, MONGOC_CURSOR_SNAPSHOT)) {
         OPT_CHECK (BOOL);
         PUSH_DOLLAR_QUERY ();
         BSON_APPEND_BOOL (query, "$snapshot", bson_iter_as_bool (&iter));
      } else if (!strcmp (key, MONGOC_CURSOR_COLLATION)) {
         bson_set_error (&cursor->error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_PROTOCOL_BAD_WIRE_VERSION,
                         "The selected server does not support collation");
         return NULL;
      }
      /* singleBatch limit and batchSize are handled in _mongoc_n_return,
       * exhaust noCursorTimeout oplogReplay tailable in _mongoc_cursor_flags
       * maxAwaitTimeMS is handled in _mongoc_cursor_prepare_getmore_command
       * sessionId is used to retrieve the mongoc_client_session_t
       */
      else if (strcmp (key, MONGOC_CURSOR_SINGLE_BATCH) &&
               strcmp (key, MONGOC_CURSOR_LIMIT) &&
               strcmp (key, MONGOC_CURSOR_BATCH_SIZE) &&
               strcmp (key, MONGOC_CURSOR_EXHAUST) &&
               strcmp (key, MONGOC_CURSOR_NO_CURSOR_TIMEOUT) &&
               strcmp (key, MONGOC_CURSOR_OPLOG_REPLAY) &&
               strcmp (key, MONGOC_CURSOR_TAILABLE) &&
               strcmp (key, MONGOC_CURSOR_MAX_AWAIT_TIME_MS)) {
         /* pass unrecognized options to server, prefixed with $ */
         PUSH_DOLLAR_QUERY ();
         dollar_modifier = bson_strdup_printf ("$%s", key);
         if (!bson_append_iter (query, dollar_modifier, -1, &iter)) {
            bson_set_error (&cursor->error,
                            MONGOC_ERROR_BSON,
                            MONGOC_ERROR_BSON_INVALID,
                            "Error adding \"%s\" to query",
                            dollar_modifier);
            bson_free (dollar_modifier);
            return NULL;
         }
         bson_free (dollar_modifier);
      }
   }

   if (!_mongoc_cursor_flags (cursor, stream, flags)) {
      /* cursor->error is set */
      return NULL;
   }

   return pushed_dollar_query ? query : &cursor->filter;
}


#undef OPT_CHECK
#undef OPT_ERR
#undef OPT_BSON_ERR
#undef OPT_FLAG
#undef OPT_SUBDOCUMENT


const bson_t *
_mongoc_cursor_op_query (mongoc_cursor_t *cursor,
                         mongoc_server_stream_t *server_stream)
{
   int64_t started;
   uint32_t request_id;
   mongoc_rpc_t rpc;
   const bson_t *query_ptr;
   bson_t query = BSON_INITIALIZER;
   bson_t fields = BSON_INITIALIZER;
   mongoc_query_flags_t flags;
   mongoc_assemble_query_result_t result = ASSEMBLE_QUERY_RESULT_INIT;
   const bson_t *ret = NULL;
   bool succeeded = false;
   bool owns_stream = false;

   ENTRY;

   if (!server_stream) {
      server_stream = _mongoc_cursor_fetch_stream (cursor);
      owns_stream = true;
   }

   /* cursors created by mongoc_client_command don't use this function */
   BSON_ASSERT (cursor->is_find);

   started = bson_get_monotonic_time ();

   cursor->operation_id = ++cursor->client->cluster.operation_id;

   request_id = ++cursor->client->cluster.request_id;

   rpc.header.msg_len = 0;
   rpc.header.request_id = request_id;
   rpc.header.response_to = 0;
   rpc.header.opcode = MONGOC_OPCODE_QUERY;
   rpc.query.flags = MONGOC_QUERY_NONE;
   rpc.query.collection = cursor->ns;
   rpc.query.skip = 0;
   rpc.query.n_return = 0;
   rpc.query.fields = NULL;

   query_ptr = _mongoc_cursor_parse_opts_for_op_query (
      cursor, server_stream, &query, &fields, &flags, &rpc.query.skip);

   if (!query_ptr) {
      /* invalid opts. cursor->error is set */
      GOTO (done);
   }

   assemble_query (
      cursor->read_prefs, server_stream, query_ptr, flags, &result);

   rpc.query.query = bson_get_data (result.assembled_query);
   rpc.query.flags = result.flags;
   rpc.query.n_return = _mongoc_n_return (true, cursor);
   if (!bson_empty (&fields)) {
      rpc.query.fields = bson_get_data (&fields);
   }

   /* cursor from mongoc_collection_find[_with_opts] is about to send its
    * initial OP_QUERY to pre-3.2 MongoDB */
   if (!_mongoc_cursor_monitor_legacy_query (cursor, server_stream)) {
      GOTO (done);
   }

   if (!mongoc_cluster_legacy_rpc_sendv_to_server (
          &cursor->client->cluster, &rpc, server_stream, &cursor->error)) {
      GOTO (done);
   }

   _mongoc_buffer_clear (&cursor->legacy_response.buffer, false);

   if (!_mongoc_client_recv (cursor->client,
                             &cursor->legacy_response.rpc,
                             &cursor->legacy_response.buffer,
                             server_stream,
                             &cursor->error)) {
      GOTO (done);
   }

   if (cursor->legacy_response.rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Invalid opcode. Expected %d, got %d.",
                      MONGOC_OPCODE_REPLY,
                      cursor->legacy_response.rpc.header.opcode);
      GOTO (done);
   }

   if (cursor->legacy_response.rpc.header.response_to != request_id) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Invalid response_to for query. Expected %d, got %d.",
                      request_id,
                      cursor->legacy_response.rpc.header.response_to);
      GOTO (done);
   }

   if (!_mongoc_rpc_check_ok (&cursor->legacy_response.rpc,
                              cursor->client->error_api_version,
                              &cursor->error,
                              &cursor->error_doc)) {
      GOTO (done);
   }

   if (cursor->legacy_response.reader) {
      bson_reader_destroy (cursor->legacy_response.reader);
   }

   cursor->cursor_id = cursor->legacy_response.rpc.reply.cursor_id;

   cursor->legacy_response.reader = bson_reader_new_from_data (
      cursor->legacy_response.rpc.reply.documents,
      (size_t) cursor->legacy_response.rpc.reply.documents_len);

   if (_mongoc_cursor_get_opt_bool (cursor, MONGOC_CURSOR_EXHAUST)) {
      cursor->in_exhaust = true;
      cursor->client->in_exhaust = true;
   }

   _mongoc_cursor_monitor_succeeded (cursor,
                                     bson_get_monotonic_time () - started,
                                     true, /* first_batch */
                                     server_stream,
                                     "find");

   cursor->state = IN_BATCH;
   succeeded = true;

   /* TODO: I'm using owns_stream to distinguish whether or not the caller is
    * the new cursor.
    * The real change here should be *not* to return the reply immediately (I
    * think) */
   if (!owns_stream) {
      _mongoc_read_from_buffer (cursor, &ret);
   }

done:
   if (!succeeded) {
      _mongoc_cursor_monitor_failed (
         cursor, bson_get_monotonic_time () - started, server_stream, "find");
      /* TODO: maybe set cursor->state to DONE here? */
   }

   if (owns_stream) {
      mongoc_server_stream_cleanup (server_stream);
   }

   assemble_query_result_cleanup (&result);
   bson_destroy (&query);
   bson_destroy (&fields);

   /* TODO: ditto above */
   if (owns_stream) {
      return NULL;
   }
   if (!ret) {
      cursor->state = DONE;
   }

   RETURN (ret);
}