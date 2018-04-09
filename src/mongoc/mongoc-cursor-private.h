/*
 * Copyright 2013 MongoDB, Inc.
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

#ifndef MONGOC_CURSOR_PRIVATE_H
#define MONGOC_CURSOR_PRIVATE_H

#if !defined(MONGOC_COMPILATION)
#error "Only <mongoc.h> can be included directly."
#endif

#include <bson.h>

#include "mongoc-client.h"
#include "mongoc-buffer-private.h"
#include "mongoc-rpc-private.h"
#include "mongoc-server-stream-private.h"


BSON_BEGIN_DECLS

typedef struct _mongoc_cursor_interface_t mongoc_cursor_interface_t;
typedef struct _mongoc_cursor_context_t mongoc_cursor_context_t;
typedef struct _mongoc_cursor_context_t {
   void (*clone) (mongoc_cursor_context_t *dst, const mongoc_cursor_context_t *src);
   void (*destroy) (mongoc_cursor_context_t *ctx);
   void (*prime) (mongoc_cursor_t *cursor);
   void (*pop_from_batch) (mongoc_cursor_t *cursor, const bson_t **out);
   void (*get_next_batch) (mongoc_cursor_t *cursor);
   /* TODO: this might not be needed, right? */
   void (*get_host) (mongoc_cursor_t *cursor, mongoc_host_list_t *host);
   void *data;
} mongoc_cursor_context_t;

struct _mongoc_cursor_interface_t {
   mongoc_cursor_t *(*clone) (const mongoc_cursor_t *cursor);
   void (*destroy) (mongoc_cursor_t *cursor);
   bool (*more) (mongoc_cursor_t *cursor);
   bool (*next) (mongoc_cursor_t *cursor, const bson_t **bson);
   bool (*error_document) (mongoc_cursor_t *cursor,
                           bson_error_t *error,
                           const bson_t **doc);
   void (*get_host) (mongoc_cursor_t *cursor, mongoc_host_list_t *host);
};

#define MONGOC_CURSOR_ALLOW_PARTIAL_RESULTS "allowPartialResults"
#define MONGOC_CURSOR_ALLOW_PARTIAL_RESULTS_LEN 19
#define MONGOC_CURSOR_AWAIT_DATA "awaitData"
#define MONGOC_CURSOR_AWAIT_DATA_LEN 9
#define MONGOC_CURSOR_BATCH_SIZE "batchSize"
#define MONGOC_CURSOR_BATCH_SIZE_LEN 9
#define MONGOC_CURSOR_COLLATION "collation"
#define MONGOC_CURSOR_COLLATION_LEN 9
#define MONGOC_CURSOR_COMMENT "comment"
#define MONGOC_CURSOR_COMMENT_LEN 7
#define MONGOC_CURSOR_EXHAUST "exhaust"
#define MONGOC_CURSOR_EXHAUST_LEN 7
#define MONGOC_CURSOR_FILTER "filter"
#define MONGOC_CURSOR_FILTER_LEN 6
#define MONGOC_CURSOR_FIND "find"
#define MONGOC_CURSOR_FIND_LEN 4
#define MONGOC_CURSOR_HINT "hint"
#define MONGOC_CURSOR_HINT_LEN 4
#define MONGOC_CURSOR_LIMIT "limit"
#define MONGOC_CURSOR_LIMIT_LEN 5
#define MONGOC_CURSOR_MAX "max"
#define MONGOC_CURSOR_MAX_LEN 3
#define MONGOC_CURSOR_MAX_AWAIT_TIME_MS "maxAwaitTimeMS"
#define MONGOC_CURSOR_MAX_AWAIT_TIME_MS_LEN 14
#define MONGOC_CURSOR_MAX_SCAN "maxScan"
#define MONGOC_CURSOR_MAX_SCAN_LEN 7
#define MONGOC_CURSOR_MAX_TIME_MS "maxTimeMS"
#define MONGOC_CURSOR_MAX_TIME_MS_LEN 9
#define MONGOC_CURSOR_MIN "min"
#define MONGOC_CURSOR_MIN_LEN 3
#define MONGOC_CURSOR_NO_CURSOR_TIMEOUT "noCursorTimeout"
#define MONGOC_CURSOR_NO_CURSOR_TIMEOUT_LEN 15
#define MONGOC_CURSOR_OPLOG_REPLAY "oplogReplay"
#define MONGOC_CURSOR_OPLOG_REPLAY_LEN 11
#define MONGOC_CURSOR_ORDERBY "orderby"
#define MONGOC_CURSOR_ORDERBY_LEN 7
#define MONGOC_CURSOR_PROJECTION "projection"
#define MONGOC_CURSOR_PROJECTION_LEN 10
#define MONGOC_CURSOR_QUERY "query"
#define MONGOC_CURSOR_QUERY_LEN 5
#define MONGOC_CURSOR_READ_CONCERN "readConcern"
#define MONGOC_CURSOR_READ_CONCERN_LEN 11
#define MONGOC_CURSOR_RETURN_KEY "returnKey"
#define MONGOC_CURSOR_RETURN_KEY_LEN 9
#define MONGOC_CURSOR_SHOW_DISK_LOC "showDiskLoc"
#define MONGOC_CURSOR_SHOW_DISK_LOC_LEN 11
#define MONGOC_CURSOR_SHOW_RECORD_ID "showRecordId"
#define MONGOC_CURSOR_SHOW_RECORD_ID_LEN 12
#define MONGOC_CURSOR_SINGLE_BATCH "singleBatch"
#define MONGOC_CURSOR_SINGLE_BATCH_LEN 11
#define MONGOC_CURSOR_SKIP "skip"
#define MONGOC_CURSOR_SKIP_LEN 4
#define MONGOC_CURSOR_SNAPSHOT "snapshot"
#define MONGOC_CURSOR_SNAPSHOT_LEN 8
#define MONGOC_CURSOR_SORT "sort"
#define MONGOC_CURSOR_SORT_LEN 4
#define MONGOC_CURSOR_TAILABLE "tailable"
#define MONGOC_CURSOR_TAILABLE_LEN 8

typedef enum { UNPRIMED, IN_BATCH, END_OF_BATCH, DONE } mongoc_cursor_state_t;

typedef struct _mongoc_cursor_legacy_response {
   mongoc_rpc_t rpc;
   mongoc_buffer_t buffer;
   bson_reader_t *reader;
} mongoc_cursor_legacy_response;

struct _mongoc_cursor_t {
   mongoc_client_t *client;

   uint32_t server_id;
   bool slave_ok;

   mongoc_cursor_state_t state;

   bool in_exhaust : 1;
   bool explicit_session : 1;

   bson_t filter;
   bson_t opts;

   mongoc_read_concern_t *read_concern;
   mongoc_read_prefs_t *read_prefs;
   mongoc_write_concern_t *write_concern;
   mongoc_client_session_t *client_session;

   uint32_t count;

   char ns[140];
   uint32_t nslen;
   uint32_t dblen;

   bson_error_t error;
   bson_t error_doc; /* always initialized, and set with server errors. */

   /* only used by deprecated mongoc_client_command to store the cursor reply.
    * Cursors which batch/stream weirdly used this only for store error docs. */
   bson_t deprecated_reply;

   const bson_t *current;

   mongoc_cursor_legacy_response legacy_response;

   mongoc_cursor_context_t ctx;

   mongoc_cursor_interface_t iface;
   void *iface_data;

   int64_t operation_id;
   int64_t cursor_id;
};


int32_t
_mongoc_n_return (mongoc_cursor_t *cursor);
void
_mongoc_set_cursor_ns (mongoc_cursor_t *cursor, const char *ns, uint32_t nslen);
bool
_mongoc_cursor_get_opt_bool (const mongoc_cursor_t *cursor, const char *option);
void
_mongoc_cursor_flags_to_opts (mongoc_query_flags_t qflags,
                              bson_t *opts,
                              bool *slave_ok);
bool
_mongoc_cursor_translate_dollar_query_opts (const bson_t *query,
                                            bson_t *opts,
                                            bson_t *filter,
                                            bson_error_t *error);
mongoc_cursor_t *
_mongoc_cursor_clone (const mongoc_cursor_t *cursor);
void
_mongoc_cursor_destroy (mongoc_cursor_t *cursor);
bool
_use_find_command (const mongoc_cursor_t *cursor,
                   const mongoc_server_stream_t *server_stream);
mongoc_server_stream_t *
_mongoc_cursor_fetch_stream (mongoc_cursor_t *cursor);
void
_mongoc_cursor_collection (const mongoc_cursor_t *cursor,
                           const char **collection,
                           int *collection_len);
bool
_mongoc_cursor_op_getmore (mongoc_cursor_t *cursor,
                           mongoc_server_stream_t *server_stream);
bool
_mongoc_cursor_run_command (mongoc_cursor_t *cursor,
                            const bson_t *command,
                            const bson_t *opts,
                            bson_t *reply);
bool
_mongoc_cursor_more (mongoc_cursor_t *cursor);
bool
_mongoc_cursor_error_document (mongoc_cursor_t *cursor,
                               bson_error_t *error,
                               const bson_t **doc);
void
_mongoc_cursor_get_host (mongoc_cursor_t *cursor, mongoc_host_list_t *host);

bool
_mongoc_cursor_set_opt_int64 (mongoc_cursor_t *cursor,
                              const char *option,
                              int64_t value);
void
_mongoc_cursor_monitor_failed (mongoc_cursor_t *cursor,
                               int64_t duration,
                               mongoc_server_stream_t *stream,
                               const char *cmd_name);
bool
_mongoc_cursor_monitor_command (mongoc_cursor_t *cursor,
                                mongoc_server_stream_t *server_stream,
                                const bson_t *cmd,
                                const char *cmd_name);
void
_mongoc_cursor_prepare_find_command (mongoc_cursor_t *cursor, bson_t *command);
const bson_t *
_mongoc_cursor_initial_query (mongoc_cursor_t *cursor);
const bson_t *
_mongoc_cursor_get_more (mongoc_cursor_t *cursor);
bool
_mongoc_cursor_flags (mongoc_cursor_t *cursor,
                      mongoc_server_stream_t *stream,
                      mongoc_query_flags_t *flags /* OUT */);
void
_mongoc_cursor_monitor_succeeded (mongoc_cursor_t *cursor,
                                  int64_t duration,
                                  bool first_batch,
                                  mongoc_server_stream_t *stream,
                                  const char *cmd_name);
/* utilities to read a batch document response from commands like "aggregate"
 * or "listCollections" */
typedef struct _mongoc_cursor_batch_reader_t {
   bson_t reply;           /* the entire command reply */
   bson_iter_t batch_iter; /* iterates over the batch array */
   bson_t current_doc;     /* the current doc inside the batch array */
} mongoc_cursor_batch_reader_t;
/* start iterating a reply like
 * {cursor: {id: 1234, ns: "db.collection", firstBatch: [...]}} or
 * {cursor: {id: 1234, ns: "db.collection", nextBatch: [...]}} */
void
_mongoc_cursor_batch_reader_refresh (mongoc_cursor_t *cursor,
                                     const bson_t *command,
                                     const bson_t *opts,
                                     mongoc_cursor_batch_reader_t *reader);
bool
_mongoc_cursor_batch_reader_start (mongoc_cursor_t *cursor,
                                   mongoc_cursor_batch_reader_t *reader);
void
_mongoc_cursor_batch_reader_read (mongoc_cursor_t *cursor,
                                  mongoc_cursor_batch_reader_t *reader,
                                  const bson_t **bson);
bool
_mongoc_cursor_prepare_getmore_command (mongoc_cursor_t *cursor,
                                        bson_t *command);
/* legacy functions. */
bool
_mongoc_cursor_next (mongoc_cursor_t *cursor, const bson_t **bson);

bool
_mongoc_read_from_buffer (mongoc_cursor_t *cursor, const bson_t **bson);

void
_mongoc_cursor_op_query_find (mongoc_cursor_t *cursor);
mongoc_cursor_t *
_mongoc_cursor_new_with_opts (mongoc_client_t *client,
                              const char *db_and_collection,
                              const bson_t *filter,
                              const bson_t *opts,
                              const mongoc_read_prefs_t *read_prefs,
                              const mongoc_read_concern_t *read_concern);
/* cursor constructors. */
mongoc_cursor_t *
_mongoc_cursor_find_new (mongoc_client_t *client, const char* db_and_coll, const bson_t *filter, const bson_t *opts, const mongoc_read_prefs_t *read_prefs, const mongoc_read_concern_t *read_concern);

mongoc_cursor_t *
_mongoc_cursor_cmd_new (mongoc_client_t *client, const char* db_and_coll, const bson_t *cmd, const bson_t *opts, const mongoc_read_prefs_t *read_prefs, const mongoc_read_concern_t *read_concern);

mongoc_cursor_t *
_mongoc_cursor_cmd_new_from_reply (mongoc_client_t *client, const bson_t *reply, const bson_t *opts, uint32_t server_id);

mongoc_cursor_t *
_mongoc_cursor_cmd_deprecated_new (mongoc_client_t *client, const char* db_and_coll, const bson_t *cmd, const mongoc_read_prefs_t *read_prefs);

mongoc_cursor_t *
_mongoc_cursor_array_new (mongoc_client_t* client, const char* db_and_coll, const bson_t *cmd, const bson_t *opts, const char* field_name);

void
_mongoc_cursor_ctx_cmd_init_with_reply (mongoc_cursor_t *cursor,
                                        bson_t *reply,
                                        uint32_t server_id);
void
_mongoc_cursor_ctx_find_init (mongoc_cursor_t *cursor);
void
_mongoc_cursor_ctx_cmd_init (mongoc_cursor_t *cursor);
void
_mongoc_cursor_ctx_cmd_deprecated_init (mongoc_cursor_t *cursor);
void
_mongoc_cursor_ctx_array_init (mongoc_cursor_t *cursor, const char *field_name);

BSON_END_DECLS


#endif /* MONGOC_CURSOR_PRIVATE_H */
