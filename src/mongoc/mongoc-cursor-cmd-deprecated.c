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

typedef struct _data_cmd_deprecated_t {
} data_cmd_deprecated_t;


static void
_prime (mongoc_cursor_t *cursor)
{
   /* cursor created with deprecated mongoc_client_command() */
   bson_destroy (&cursor->deprecated_reply);

   if (_mongoc_cursor_run_command (
          cursor, &cursor->filter, &cursor->opts, &cursor->deprecated_reply)) {
      cursor->state = IN_BATCH;
   } else {
      cursor->state = DONE;
   }
}

static void
_pop_from_batch (mongoc_cursor_t *cursor, const bson_t **out)
{
   *out = &cursor->deprecated_reply;
   cursor->state = DONE;
}

static void
_get_next_batch (mongoc_cursor_t *cursor)
{
   fprintf (stderr, "cannot get more on a deprecated command cursor.\n");
   BSON_ASSERT (false);
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
_mongoc_cursor_ctx_cmd_deprecated_init (mongoc_cursor_t *cursor)
{
   cursor->ctx.prime = _prime;
   cursor->ctx.pop_from_batch = _pop_from_batch;
   cursor->ctx.get_next_batch = _get_next_batch;
   cursor->ctx.destroy = _destroy;
   cursor->ctx.init = _mongoc_cursor_ctx_cmd_deprecated_init;
   cursor->ctx.get_host = _get_host;
}