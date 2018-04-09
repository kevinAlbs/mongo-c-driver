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
#include "mongoc-util-private.h"
#include "mongoc-client-private.h"


extern void
_mongoc_cursor_init_find_cmd_ctx (mongoc_cursor_t *cursor);
extern void
_mongoc_cursor_init_find_opquery_ctx (mongoc_cursor_t *cursor);

static void
_destroy (mongoc_cursor_context_t *ctx)
{
}


static void
_get_host (mongoc_cursor_t *cursor, mongoc_host_list_t *host)
{
   /* there is no host yet */
   memset (host, sizeof (mongoc_host_list_t), 0);
}


static void
_prime (mongoc_cursor_t *cursor)
{
   bool use_find_command;
   uint32_t server_id;
   mongoc_server_stream_t *server_stream;

   /* determine if this should be a command or op_query cursor. */
   if (!_mongoc_get_server_id_from_opts (&cursor->opts,
                                         MONGOC_ERROR_CURSOR,
                                         MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                                         &server_id,
                                         &cursor->error)) {
      cursor->state = DONE;
      return;
   }

   /* may set server_id. */
   server_stream = _mongoc_cursor_fetch_stream (cursor);

   if (!server_stream) {
      cursor->state = DONE;
      return;
   }
   /* a find command can not be used for exhaust cursors. */
   use_find_command =
      server_stream->sd->max_wire_version >= WIRE_VERSION_FIND_CMD &&
      !_mongoc_cursor_get_opt_bool (cursor, MONGOC_CURSOR_EXHAUST);
   mongoc_server_stream_cleanup (server_stream);

   if (use_find_command) {
      _mongoc_cursor_init_find_cmd_ctx (cursor);
   } else {
      _mongoc_cursor_init_find_opquery_ctx (cursor);
   }

   cursor->ctx.prime (cursor);
}

static void
_pop_from_batch (mongoc_cursor_t *cursor, const bson_t **out)
{
   fprintf (stderr, "_pop_from_batch called on find cursor.\n");
   BSON_ASSERT (false);
}


static void
_get_next_batch (mongoc_cursor_t *cursor)
{
   fprintf (stderr, "_get_next_batch called on find cursor.\n");
   BSON_ASSERT (false);
}


void
_mongoc_cursor_init_find_ctx (mongoc_cursor_t *cursor)
{
   cursor->ctx.prime = _prime;
   cursor->ctx.pop_from_batch = _pop_from_batch;
   cursor->ctx.get_next_batch = _get_next_batch;
   cursor->ctx.init = _mongoc_cursor_init_find_ctx;
   cursor->ctx.destroy = _destroy;
   cursor->ctx.get_host = _get_host;
}