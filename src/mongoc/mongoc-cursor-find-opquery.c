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
#include "mongoc-rpc-private.h"
#include "mongoc-cursor-private.h"
#include "mongoc-client-private.h"

typedef struct _data_find_opquery_t {
} data_find_opquery_t;

static bool
_hit_limit (mongoc_cursor_t *cursor)
{
   int64_t limit;
   limit = mongoc_cursor_get_limit (cursor);
   /* mark as done if we've hit the limit. */
   if (limit && cursor->count >= llabs (limit)) {
      cursor->state = DONE;
      return true;
   }
   return false;
}

static void
_prime (mongoc_cursor_t *cursor)
{
   if (_hit_limit (cursor)) {
      return;
   }

   _mongoc_buffer_clear (&cursor->legacy_response.buffer, false);
   _mongoc_cursor_op_query_find (cursor);

   if (cursor->error.domain) {
      cursor->state = DONE;
   }

   if (cursor->state == DONE) {
      return;
   }

   if (_mongoc_cursor_get_opt_bool (cursor, MONGOC_CURSOR_EXHAUST)) {
      cursor->in_exhaust = true;
      cursor->client->in_exhaust = true;
   }
   cursor->state = IN_BATCH;
}

static void
_pop_from_batch (mongoc_cursor_t *cursor, const bson_t **out)
{
   const bson_t *bson;
   bool eof = false;

   if (_hit_limit (cursor)) {
      cursor->state = DONE;
      return;
   }

   bson = bson_reader_read (cursor->legacy_response.reader, &eof);

   if (out) {
      *out = bson;
   }

   if (eof) {
      cursor->state = cursor->cursor_id ? END_OF_BATCH : DONE;
   } else if (!bson) {
      cursor->state = DONE;
   }
}

static void
_get_next_batch (mongoc_cursor_t *cursor)
{
   bool r;

   _mongoc_buffer_clear (&cursor->legacy_response.buffer, false);
   r = _mongoc_cursor_op_getmore (cursor, NULL /* server stream */);
   if (!r || cursor->error.domain) {
      cursor->state = DONE;
   } else {
      cursor->state = IN_BATCH;
   }
}

static void
_get_host (mongoc_cursor_t *cursor, mongoc_host_list_t *host)
{
   _mongoc_cursor_get_host (cursor, host);
}

static void
_destroy (mongoc_cursor_context_t *ctx)
{
}

void
_mongoc_cursor_ctx_find_opquery_init (mongoc_cursor_t *cursor)
{
   cursor->ctx.prime = _prime;
   cursor->ctx.pop_from_batch = _pop_from_batch;
   cursor->ctx.get_next_batch = _get_next_batch;
   cursor->ctx.destroy = _destroy;
   cursor->ctx.init = _mongoc_cursor_ctx_find_opquery_init;
   cursor->ctx.get_host = _get_host;
}