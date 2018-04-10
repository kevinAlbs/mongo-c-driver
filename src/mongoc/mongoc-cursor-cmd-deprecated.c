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

typedef struct _data_cmd_deprecated_t {
   bson_t reply;
} data_cmd_deprecated_t;


static void
_prime (mongoc_cursor_t *cursor)
{
   data_cmd_deprecated_t *data = (data_cmd_deprecated_t *) cursor->ctx.data;
   bson_destroy (&data->reply);
   if (_mongoc_cursor_run_command (
          cursor, &cursor->filter, &cursor->opts, &data->reply)) {
      cursor->state = IN_BATCH;
   } else {
      cursor->state = DONE;
   }
}


static void
_pop_from_batch (mongoc_cursor_t *cursor, const bson_t **out)
{
   data_cmd_deprecated_t *data = (data_cmd_deprecated_t *) cursor->ctx.data;
   *out = &data->reply;
   cursor->state = DONE;
}


static void
_clone (mongoc_cursor_ctx_t *dst, const mongoc_cursor_ctx_t *src)
{
   data_cmd_deprecated_t *data = bson_malloc0 (sizeof (data_cmd_deprecated_t));
   bson_init (&data->reply);
   dst->data = data;
}


static void
_destroy (mongoc_cursor_ctx_t *ctx)
{
   data_cmd_deprecated_t *data = (data_cmd_deprecated_t *) ctx->data;
   bson_destroy (&data->reply);
   bson_free (data);
}


mongoc_cursor_t *
_mongoc_cursor_cmd_deprecated_new (mongoc_client_t *client,
                                   const char *db_and_coll,
                                   const bson_t *cmd,
                                   const mongoc_read_prefs_t *read_prefs)
{
   mongoc_cursor_t *cursor = _mongoc_cursor_new_with_opts (
      client, db_and_coll, cmd, NULL, read_prefs, NULL);
   data_cmd_deprecated_t *data = bson_malloc0 (sizeof (data_cmd_deprecated_t));
   bson_init (&data->reply);
   cursor->ctx.prime = _prime;
   cursor->ctx.pop_from_batch = _pop_from_batch;
   cursor->ctx.data = data;
   cursor->ctx.clone = _clone;
   cursor->ctx.destroy = _destroy;
   return cursor;
}