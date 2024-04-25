/*
 * Copyright 2024-present MongoDB, Inc.
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

#include <bson/bson.h>
#include <mongoc/mongoc-error.h>
#include <mongoc-bulkwrite.h>
#include <mongoc-array-private.h>
#include <mongoc-buffer-private.h>
#include <mongoc-server-stream-private.h>
#include <mongoc-client-private.h>
#include <mongoc-error-private.h>  // _mongoc_write_error_handle_labels
#include <mongoc-util-private.h>   // _mongoc_iter_document_as_bson
#include <common-macros-private.h> // MC_ENABLE_CONVERSION_WARNING_BEGIN

#include <uthash.h>

MC_ENABLE_CONVERSION_WARNING_BEGIN

typedef struct {
   bool isset;
   bool val;
} mongoc_opt_bool_t;

struct _mongoc_bulkwriteopts_t {
   mongoc_opt_bool_t ordered;
   mongoc_opt_bool_t bypassdocumentvalidation;
   bson_t *let;
   mongoc_write_concern_t *writeconcern;
   mongoc_opt_bool_t verboseresults;
   bson_t *comment;
   mongoc_client_session_t *session;
   bson_t *extra;
   uint32_t serverid;
};


// `set_bson_opt` sets `dst` by copying `src`. If NULL is passed, the `dst` is cleared.
static void
set_bson_opt (bson_t **dst, const bson_t *src)
{
   BSON_ASSERT_PARAM (dst);
   bson_destroy (*dst);
   *dst = NULL;
   if (src) {
      *dst = bson_copy (src);
   }
}

static void
set_hint_opt (bson_value_t *dst, const bson_value_t *src)
{
   BSON_ASSERT_PARAM (dst);
   bson_value_destroy (dst);
   *dst = (bson_value_t){0};
   if (src) {
      bson_value_copy (src, dst);
   }
}

mongoc_bulkwriteopts_t *
mongoc_bulkwriteopts_new (void)
{
   return bson_malloc0 (sizeof (mongoc_bulkwriteopts_t));
}
void
mongoc_bulkwriteopts_set_ordered (mongoc_bulkwriteopts_t *self, bool ordered)
{
   BSON_ASSERT_PARAM (self);
   self->ordered = (mongoc_opt_bool_t){.isset = true, .val = ordered};
}
void
mongoc_bulkwriteopts_set_bypassdocumentvalidation (mongoc_bulkwriteopts_t *self, bool bypassdocumentvalidation)
{
   BSON_ASSERT_PARAM (self);
   self->bypassdocumentvalidation = (mongoc_opt_bool_t){.isset = true, .val = bypassdocumentvalidation};
}
void
mongoc_bulkwriteopts_set_let (mongoc_bulkwriteopts_t *self, const bson_t *let)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (let || true);
   set_bson_opt (&self->let, let);
}
void
mongoc_bulkwriteopts_set_writeconcern (mongoc_bulkwriteopts_t *self, const mongoc_write_concern_t *writeconcern)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (writeconcern || true);
   mongoc_write_concern_destroy (self->writeconcern);
   self->writeconcern = mongoc_write_concern_copy (writeconcern);
}
void
mongoc_bulkwriteopts_set_verboseresults (mongoc_bulkwriteopts_t *self, bool verboseresults)
{
   BSON_ASSERT_PARAM (self);
   self->verboseresults = (mongoc_opt_bool_t){.isset = true, .val = verboseresults};
}
void
mongoc_bulkwriteopts_set_comment (mongoc_bulkwriteopts_t *self, const bson_t *comment)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (comment || true);
   set_bson_opt (&self->comment, comment);
}
void
mongoc_bulkwriteopts_set_session (mongoc_bulkwriteopts_t *self, mongoc_client_session_t *session)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (session || true);
   // Client sessions cannot be copied. Do a non-owning assignment.
   self->session = session;
}
void
mongoc_bulkwriteopts_set_extra (mongoc_bulkwriteopts_t *self, const bson_t *extra)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (extra || true);
   set_bson_opt (&self->extra, extra);
}
void
mongoc_bulkwriteopts_set_serverid (mongoc_bulkwriteopts_t *self, uint32_t serverid)
{
   BSON_ASSERT_PARAM (self);
   self->serverid = serverid;
}
void
mongoc_bulkwriteopts_destroy (mongoc_bulkwriteopts_t *self)
{
   if (!self) {
      return;
   }
   bson_destroy (self->extra);
   bson_destroy (self->comment);
   mongoc_write_concern_destroy (self->writeconcern);
   bson_destroy (self->let);
   bson_free (self);
}

typedef enum { MODEL_OP_INSERT, MODEL_OP_UPDATE, MODEL_OP_DELETE } model_op_t;
typedef struct {
   model_op_t op;
   bson_iter_t id_iter;
   char *ns;
   int ns_len;
} model_data_t;

struct _mongoc_bulkwrite_t {
   mongoc_client_t *client;
   // `executed` is set to true once `mongoc_bulkwrite_execute` is called.
   // `mongoc_bulkwrite_t` may not be executed more than once.
   bool executed;
   // `ops` is a document sequence.
   mongoc_buffer_t ops;
   size_t n_ops;
   // `ns_to_index` maps a namespace to an index.
   bson_t ns_to_index;
   // `model_entries` is an array of `model_data_t` sized to the number of models.
   // It stores metadata describing results (e.g. generated `_id` fields)
   mongoc_array_t model_entries;
   // `max_insert_len` tracks the maximum length of any document to-be inserted.
   uint32_t max_insert_len;
   // `has_multi_write` is true if there are any multi-document update or delete operations. Multi-document
   // writes are ineligible for retryable writes.
   bool has_multi_write;
   int64_t operation_id;
};


// `mongoc_client_bulkwrite_new` creates a new bulk write operation.
mongoc_bulkwrite_t *
mongoc_client_bulkwrite_new (mongoc_client_t *self)
{
   BSON_ASSERT_PARAM (self);
   mongoc_bulkwrite_t *bw = bson_malloc0 (sizeof (mongoc_bulkwrite_t));
   bw->client = self;
   _mongoc_buffer_init (&bw->ops, NULL, 0, NULL, NULL);
   bson_init (&bw->ns_to_index);
   _mongoc_array_init (&bw->model_entries, sizeof (model_data_t));
   bw->operation_id = ++self->cluster.operation_id;
   return bw;
}

void
mongoc_bulkwrite_destroy (mongoc_bulkwrite_t *self)
{
   if (!self) {
      return;
   }
   for (size_t i = 0; i < self->model_entries.len; i++) {
      model_data_t md = _mongoc_array_index (&self->model_entries, model_data_t, i);
      bson_free (md.ns);
   }
   _mongoc_array_destroy (&self->model_entries);
   bson_destroy (&self->ns_to_index);
   _mongoc_buffer_destroy (&self->ops);
   bson_free (self);
}

struct _mongoc_insertoneopts_t {
   // No fields yet. Include an unused placeholder to prevent compiler errors due to an empty struct.
   int unused;
};

mongoc_insertoneopts_t *
mongoc_insertoneopts_new (void)
{
   return bson_malloc0 (sizeof (mongoc_insertoneopts_t));
}

void
mongoc_insertoneopts_destroy (mongoc_insertoneopts_t *self)
{
   if (!self) {
      return;
   }
   bson_free (self);
}

bool
mongoc_bulkwrite_append_insertone (mongoc_bulkwrite_t *self,
                                   const char *ns,
                                   int ns_len,
                                   const bson_t *document,
                                   mongoc_insertoneopts_t *opts, // may be NULL
                                   bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (ns);
   BSON_ASSERT_PARAM (document);
   BSON_ASSERT (document->len >= 5);
   BSON_ASSERT (opts || true);
   BSON_ASSERT (error || true);

   if (self->executed) {
      bson_set_error (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "bulk write already executed");
      return false;
   }

   bson_t op = BSON_INITIALIZER;
   BSON_ASSERT (bson_append_int32 (&op, "insert", 6, -1)); // Append -1 as a placeholder. Will be overwritten later.

   // If `document` does not contain `_id`, add one in the beginning.
   bson_iter_t id_iter;
   if (!bson_iter_init_find (&id_iter, document, "_id")) {
      bson_t tmp = BSON_INITIALIZER;
      bson_oid_t oid;
      bson_oid_init (&oid, NULL);
      BSON_ASSERT (BSON_APPEND_OID (&tmp, "_id", &oid));
      BSON_ASSERT (bson_concat (&tmp, document));
      BSON_ASSERT (bson_append_document (&op, "document", 8, &tmp));
      self->max_insert_len = BSON_MAX (self->max_insert_len, tmp.len);
      bson_destroy (&tmp);
   } else {
      BSON_ASSERT (bson_append_document (&op, "document", 8, document));
      self->max_insert_len = BSON_MAX (self->max_insert_len, document->len);
   }

   BSON_ASSERT (_mongoc_buffer_append (&self->ops, bson_get_data (&op), op.len));

   // Store an iterator to the document's `_id` in the persisted payload:
   {
      BSON_ASSERT (bson_in_range_size_t_unsigned (op.len));
      size_t start = self->ops.len - (size_t) op.len;
      bson_t doc_view;
      BSON_ASSERT (bson_init_static (&doc_view, self->ops.data + start, (size_t) op.len));
      BSON_ASSERT (bson_iter_init (&id_iter, &doc_view));
      BSON_ASSERT (bson_iter_find_descendant (&id_iter, "document._id", &id_iter));
   }

   self->n_ops++;
   model_data_t md = {.op = MODEL_OP_INSERT, .id_iter = id_iter, .ns = bson_strdup (ns), .ns_len = -1};
   _mongoc_array_append_val (&self->model_entries, md);
   bson_destroy (&op);
   return true;
}


static bool
validate_update (const bson_t *update, bson_error_t *error)
{
   BSON_ASSERT_PARAM (update);
   BSON_ASSERT (error || true);

   bson_iter_t iter;
   if (_mongoc_document_is_pipeline (update)) {
      return true;
   }

   if (!bson_iter_init (&iter, update)) {
      bson_set_error (error, MONGOC_ERROR_BSON, MONGOC_ERROR_BSON_INVALID, "update document is corrupt");
      return false;
   }

   if (bson_iter_next (&iter)) {
      const char *key = bson_iter_key (&iter);
      if (key[0] != '$') {
         bson_set_error (error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_COMMAND_INVALID_ARG,
                         "Invalid key '%s': update only works with $ operators"
                         " and pipelines",
                         key);

         return false;
      }
   }
   return true;
}

struct _mongoc_updateoneopts_t {
   bson_t *arrayfilters;
   bson_t *collation;
   bson_value_t hint;
   mongoc_opt_bool_t upsert;
};

mongoc_updateoneopts_t *
mongoc_updateoneopts_new (void)
{
   return bson_malloc0 (sizeof (mongoc_updateoneopts_t));
}
void
mongoc_updateoneopts_set_arrayfilters (mongoc_updateoneopts_t *self, const bson_t *arrayfilters)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (arrayfilters || true);
   set_bson_opt (&self->arrayfilters, arrayfilters);
}
void
mongoc_updateoneopts_set_collation (mongoc_updateoneopts_t *self, const bson_t *collation)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (collation || true);
   set_bson_opt (&self->collation, collation);
}
void
mongoc_updateoneopts_set_hint (mongoc_updateoneopts_t *self, const bson_value_t *hint)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (hint || true);
   set_hint_opt (&self->hint, hint);
}
void
mongoc_updateoneopts_set_upsert (mongoc_updateoneopts_t *self, bool upsert)
{
   BSON_ASSERT_PARAM (self);
   self->upsert = (mongoc_opt_bool_t){.isset = true, .val = upsert};
}
void
mongoc_updateoneopts_destroy (mongoc_updateoneopts_t *self)
{
   if (!self) {
      return;
   }
   bson_destroy (self->arrayfilters);
   bson_destroy (self->collation);
   bson_value_destroy (&self->hint);
   bson_free (self);
}

bool
mongoc_bulkwrite_append_updateone (mongoc_bulkwrite_t *self,
                                   const char *ns,
                                   int ns_len,
                                   const bson_t *filter,
                                   const bson_t *update,
                                   mongoc_updateoneopts_t *opts /* May be NULL */,
                                   bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (ns);
   BSON_ASSERT_PARAM (filter);
   BSON_ASSERT (filter->len >= 5);
   BSON_ASSERT_PARAM (update);
   BSON_ASSERT (update->len >= 5);
   BSON_ASSERT (opts || true);
   BSON_ASSERT (error || true);

   if (self->executed) {
      bson_set_error (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "bulk write already executed");
      return false;
   }

   mongoc_updateoneopts_t defaults = {0};
   if (!opts) {
      opts = &defaults;
   }

   if (!validate_update (update, error)) {
      return false;
   }

   bson_t op = BSON_INITIALIZER;

   BSON_ASSERT (bson_append_int32 (&op, "update", 6, -1)); // Append -1 as a placeholder. Will be overwritten later.

   BSON_ASSERT (bson_append_document (&op, "filter", 6, filter));
   if (_mongoc_document_is_pipeline (update)) {
      BSON_ASSERT (bson_append_array (&op, "updateMods", 10, update));
   } else {
      BSON_ASSERT (bson_append_document (&op, "updateMods", 10, update));
   }
   BSON_ASSERT (bson_append_bool (&op, "multi", 5, false));
   if (opts->arrayfilters) {
      BSON_ASSERT (bson_append_array (&op, "arrayFilters", 12, opts->arrayfilters));
   }
   if (opts->collation) {
      BSON_ASSERT (bson_append_document (&op, "collation", 9, opts->collation));
   }
   if (opts->hint.value_type != BSON_TYPE_EOD) {
      BSON_ASSERT (bson_append_value (&op, "hint", 4, &opts->hint));
   }
   if (opts->upsert.isset) {
      BSON_ASSERT (bson_append_bool (&op, "upsert", 6, opts->upsert.val));
   }

   BSON_ASSERT (_mongoc_buffer_append (&self->ops, bson_get_data (&op), op.len));

   self->n_ops++;
   model_data_t md = {.op = MODEL_OP_UPDATE, .ns = bson_strdup (ns), .ns_len = -1};
   _mongoc_array_append_val (&self->model_entries, md);
   bson_destroy (&op);
   return true;
}

struct _mongoc_replaceoneopts_t {
   bson_t *arrayfilters;
   bson_t *collation;
   bson_value_t hint;
   mongoc_opt_bool_t upsert;
};

mongoc_replaceoneopts_t *
mongoc_replaceoneopts_new (void)
{
   return bson_malloc0 (sizeof (mongoc_replaceoneopts_t));
}
void
mongoc_replaceoneopts_set_arrayfilters (mongoc_replaceoneopts_t *self, const bson_t *arrayfilters)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (arrayfilters || true);
   set_bson_opt (&self->arrayfilters, arrayfilters);
}
void
mongoc_replaceoneopts_set_collation (mongoc_replaceoneopts_t *self, const bson_t *collation)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (collation || true);
   set_bson_opt (&self->collation, collation);
}
void
mongoc_replaceoneopts_set_hint (mongoc_replaceoneopts_t *self, const bson_value_t *hint)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (hint || true);
   set_hint_opt (&self->hint, hint);
}
void
mongoc_replaceoneopts_set_upsert (mongoc_replaceoneopts_t *self, bool upsert)
{
   BSON_ASSERT_PARAM (self);
   self->upsert = (mongoc_opt_bool_t){.isset = true, .val = upsert};
}
void
mongoc_replaceoneopts_destroy (mongoc_replaceoneopts_t *self)
{
   if (!self) {
      return;
   }
   bson_destroy (self->arrayfilters);
   bson_destroy (self->collation);
   bson_value_destroy (&self->hint);
   bson_free (self);
}

bool
validate_replace (const bson_t *doc, bson_error_t *error)
{
   BSON_ASSERT (doc || true);
   BSON_ASSERT (error || true);

   bson_iter_t iter;

   if (!bson_iter_init (&iter, doc)) {
      bson_set_error (error, MONGOC_ERROR_BSON, MONGOC_ERROR_BSON_INVALID, "replace document is corrupt");
      return false;
   }

   if (bson_iter_next (&iter)) {
      const char *key = bson_iter_key (&iter);
      if (key[0] == '$') {
         bson_set_error (error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_COMMAND_INVALID_ARG,
                         "Invalid key '%s': replace prohibits $ operators",
                         key);

         return false;
      }
   }

   return true;
}

bool
mongoc_bulkwrite_append_replaceone (mongoc_bulkwrite_t *self,
                                    const char *ns,
                                    int ns_len,
                                    const bson_t *filter,
                                    const bson_t *replacement,
                                    mongoc_replaceoneopts_t *opts /* May be NULL */,
                                    bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (ns);
   BSON_ASSERT_PARAM (filter);
   BSON_ASSERT (filter->len >= 5);
   BSON_ASSERT_PARAM (replacement);
   BSON_ASSERT (replacement->len >= 5);
   BSON_ASSERT (opts || true);
   BSON_ASSERT (error || true);

   if (self->executed) {
      bson_set_error (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "bulk write already executed");
      return false;
   }

   mongoc_replaceoneopts_t defaults = {0};
   if (!opts) {
      opts = &defaults;
   }

   if (!validate_replace (replacement, error)) {
      return false;
   }

   bson_t op = BSON_INITIALIZER;

   BSON_ASSERT (bson_append_int32 (&op, "update", 6, -1)); // Append -1 as a placeholder. Will be overwritten later.

   BSON_ASSERT (bson_append_document (&op, "filter", 6, filter));
   BSON_ASSERT (bson_append_document (&op, "updateMods", 10, replacement));
   BSON_ASSERT (bson_append_bool (&op, "multi", 5, false));
   if (opts->arrayfilters) {
      BSON_ASSERT (bson_append_array (&op, "arrayFilters", 12, opts->arrayfilters));
   }
   if (opts->collation) {
      BSON_ASSERT (bson_append_document (&op, "collation", 9, opts->collation));
   }
   if (opts->hint.value_type != BSON_TYPE_EOD) {
      BSON_ASSERT (bson_append_value (&op, "hint", 4, &opts->hint));
   }
   if (opts->upsert.isset) {
      BSON_ASSERT (bson_append_bool (&op, "upsert", 6, opts->upsert.val));
   }

   BSON_ASSERT (_mongoc_buffer_append (&self->ops, bson_get_data (&op), op.len));

   self->n_ops++;
   self->max_insert_len = BSON_MAX (self->max_insert_len, replacement->len);
   model_data_t md = {.op = MODEL_OP_UPDATE, .ns = bson_strdup (ns), .ns_len = -1};
   _mongoc_array_append_val (&self->model_entries, md);
   bson_destroy (&op);
   return true;
}

struct _mongoc_updatemanyopts_t {
   bson_t *arrayfilters;
   bson_t *collation;
   bson_value_t hint;
   mongoc_opt_bool_t upsert;
};

mongoc_updatemanyopts_t *
mongoc_updatemanyopts_new (void)
{
   return bson_malloc0 (sizeof (mongoc_updatemanyopts_t));
}
void
mongoc_updatemanyopts_set_arrayfilters (mongoc_updatemanyopts_t *self, const bson_t *arrayfilters)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (arrayfilters || true);
   set_bson_opt (&self->arrayfilters, arrayfilters);
}
void
mongoc_updatemanyopts_set_collation (mongoc_updatemanyopts_t *self, const bson_t *collation)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (collation || true);
   set_bson_opt (&self->collation, collation);
}
void
mongoc_updatemanyopts_set_hint (mongoc_updatemanyopts_t *self, const bson_value_t *hint)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (hint || true);
   set_hint_opt (&self->hint, hint);
}
void
mongoc_updatemanyopts_set_upsert (mongoc_updatemanyopts_t *self, bool upsert)
{
   BSON_ASSERT_PARAM (self);
   self->upsert = (mongoc_opt_bool_t){.isset = true, .val = upsert};
}
void
mongoc_updatemanyopts_destroy (mongoc_updatemanyopts_t *self)
{
   if (!self) {
      return;
   }
   bson_destroy (self->arrayfilters);
   bson_destroy (self->collation);
   bson_value_destroy (&self->hint);
   bson_free (self);
}

bool
mongoc_bulkwrite_append_updatemany (mongoc_bulkwrite_t *self,
                                    const char *ns,
                                    int ns_len,
                                    const bson_t *filter,
                                    const bson_t *update,
                                    mongoc_updatemanyopts_t *opts /* May be NULL */,
                                    bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (ns);
   BSON_ASSERT_PARAM (filter);
   BSON_ASSERT (filter->len >= 5);
   BSON_ASSERT_PARAM (update);
   BSON_ASSERT (update->len >= 5);
   BSON_ASSERT (opts || true);
   BSON_ASSERT (error || true);

   if (self->executed) {
      bson_set_error (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "bulk write already executed");
      return false;
   }

   mongoc_updatemanyopts_t defaults = {0};
   if (!opts) {
      opts = &defaults;
   }

   if (!validate_update (update, error)) {
      return false;
   }

   bson_t op = BSON_INITIALIZER;

   BSON_ASSERT (bson_append_int32 (&op, "update", 6, -1)); // Append -1 as a placeholder. Will be overwritten later.

   BSON_ASSERT (bson_append_document (&op, "filter", 6, filter));
   if (_mongoc_document_is_pipeline (update)) {
      BSON_ASSERT (bson_append_array (&op, "updateMods", 10, update));
   } else {
      BSON_ASSERT (bson_append_document (&op, "updateMods", 10, update));
   }
   BSON_ASSERT (bson_append_bool (&op, "multi", 5, true));
   if (opts->arrayfilters) {
      BSON_ASSERT (bson_append_array (&op, "arrayFilters", 12, opts->arrayfilters));
   }
   if (opts->collation) {
      BSON_ASSERT (bson_append_document (&op, "collation", 9, opts->collation));
   }
   if (opts->hint.value_type != BSON_TYPE_EOD) {
      BSON_ASSERT (bson_append_value (&op, "hint", 4, &opts->hint));
   }
   if (opts->upsert.isset) {
      BSON_ASSERT (bson_append_bool (&op, "upsert", 6, opts->upsert.val));
   }

   BSON_ASSERT (_mongoc_buffer_append (&self->ops, bson_get_data (&op), op.len));

   self->has_multi_write = true;
   self->n_ops++;
   model_data_t md = {.op = MODEL_OP_UPDATE, .ns = bson_strdup (ns), .ns_len = -1};
   _mongoc_array_append_val (&self->model_entries, md);
   bson_destroy (&op);
   return true;
}

struct _mongoc_deleteoneopts_t {
   bson_t *collation;
   bson_value_t hint;
};

mongoc_deleteoneopts_t *
mongoc_deleteoneopts_new (void)
{
   return bson_malloc0 (sizeof (mongoc_deleteoneopts_t));
}
void
mongoc_deleteoneopts_set_collation (mongoc_deleteoneopts_t *self, const bson_t *collation)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (collation || true);
   set_bson_opt (&self->collation, collation);
}
void
mongoc_deleteoneopts_set_hint (mongoc_deleteoneopts_t *self, const bson_value_t *hint)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (hint || true);
   set_hint_opt (&self->hint, hint);
}
void
mongoc_deleteoneopts_destroy (mongoc_deleteoneopts_t *self)
{
   if (!self) {
      return;
   }
   bson_value_destroy (&self->hint);
   bson_destroy (self->collation);
   bson_free (self);
}

bool
mongoc_bulkwrite_append_deleteone (mongoc_bulkwrite_t *self,
                                   const char *ns,
                                   int ns_len,
                                   const bson_t *filter,
                                   mongoc_deleteoneopts_t *opts /* May be NULL */,
                                   bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (ns);
   BSON_ASSERT_PARAM (filter);
   BSON_ASSERT (filter->len >= 5);
   BSON_ASSERT (opts || true);
   BSON_ASSERT (error || true);

   if (self->executed) {
      bson_set_error (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "bulk write already executed");
      return false;
   }

   mongoc_deleteoneopts_t defaults = {0};
   if (!opts) {
      opts = &defaults;
   }

   bson_t op = BSON_INITIALIZER;

   BSON_ASSERT (bson_append_int32 (&op, "delete", 6, -1)); // Append -1 as a placeholder. Will be overwritten later.
   BSON_ASSERT (bson_append_document (&op, "filter", 6, filter));
   BSON_ASSERT (bson_append_bool (&op, "multi", 5, false));
   if (opts->collation) {
      BSON_ASSERT (bson_append_document (&op, "collation", 9, opts->collation));
   }
   if (opts->hint.value_type != BSON_TYPE_EOD) {
      BSON_ASSERT (bson_append_value (&op, "hint", 4, &opts->hint));
   }

   BSON_ASSERT (_mongoc_buffer_append (&self->ops, bson_get_data (&op), op.len));

   self->n_ops++;
   model_data_t md = {.op = MODEL_OP_DELETE, .ns = bson_strdup (ns), .ns_len = -1};
   _mongoc_array_append_val (&self->model_entries, md);
   bson_destroy (&op);
   return true;
}

struct _mongoc_deletemanyopts_t {
   bson_t *collation;
   bson_value_t hint;
};

mongoc_deletemanyopts_t *
mongoc_deletemanyopts_new (void)
{
   return bson_malloc0 (sizeof (mongoc_deletemanyopts_t));
}
void
mongoc_deletemanyopts_set_collation (mongoc_deletemanyopts_t *self, const bson_t *collation)
{
   BSON_ASSERT_PARAM (self);
   set_bson_opt (&self->collation, collation);
}
void
mongoc_deletemanyopts_set_hint (mongoc_deletemanyopts_t *self, const bson_value_t *hint)
{
   BSON_ASSERT_PARAM (self);
   set_hint_opt (&self->hint, hint);
}
void
mongoc_deletemanyopts_destroy (mongoc_deletemanyopts_t *self)
{
   if (!self) {
      return;
   }
   bson_value_destroy (&self->hint);
   bson_destroy (self->collation);
   bson_free (self);
}

bool
mongoc_bulkwrite_append_deletemany (mongoc_bulkwrite_t *self,
                                    const char *ns,
                                    int ns_len,
                                    const bson_t *filter,
                                    mongoc_deletemanyopts_t *opts /* May be NULL */,
                                    bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (ns);
   BSON_ASSERT_PARAM (filter);
   BSON_ASSERT (filter->len >= 5);
   BSON_ASSERT (opts || true);
   BSON_ASSERT (error || true);

   if (self->executed) {
      bson_set_error (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "bulk write already executed");
      return false;
   }

   mongoc_deletemanyopts_t defaults = {0};
   if (!opts) {
      opts = &defaults;
   }

   bson_t op = BSON_INITIALIZER;

   BSON_ASSERT (bson_append_int32 (&op, "delete", 6, -1)); // Append -1 as a placeholder. Will be overwritten later.
   BSON_ASSERT (bson_append_document (&op, "filter", 6, filter));
   BSON_ASSERT (bson_append_bool (&op, "multi", 5, true));
   if (opts->collation) {
      BSON_ASSERT (bson_append_document (&op, "collation", 9, opts->collation));
   }
   if (opts->hint.value_type != BSON_TYPE_EOD) {
      BSON_ASSERT (bson_append_value (&op, "hint", 4, &opts->hint));
   }

   BSON_ASSERT (_mongoc_buffer_append (&self->ops, bson_get_data (&op), op.len));

   self->has_multi_write = true;
   self->n_ops++;
   model_data_t md = {.op = MODEL_OP_DELETE, .ns = bson_strdup (ns), .ns_len = -1};
   _mongoc_array_append_val (&self->model_entries, md);
   bson_destroy (&op);
   return true;
}


struct _mongoc_bulkwriteresult_t {
   int64_t acknowledged;
   int64_t insertedcount;
   int64_t upsertedcount;
   int64_t matchedcount;
   int64_t modifiedcount;
   int64_t deletedcount;
   uint32_t serverid;
   bson_t insertresults;
   bson_t updateresults;
   bson_t deleteresults;
   bool verboseresults;
};

bool
mongoc_bulkwriteresult_acknowledged (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->acknowledged;
}

int64_t
mongoc_bulkwriteresult_insertedcount (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->insertedcount;
}

int64_t
mongoc_bulkwriteresult_upsertedcount (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->upsertedcount;
}

int64_t
mongoc_bulkwriteresult_matchedcount (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->matchedcount;
}

int64_t
mongoc_bulkwriteresult_modifiedcount (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->modifiedcount;
}

int64_t
mongoc_bulkwriteresult_deletedcount (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->deletedcount;
}

const bson_t *
mongoc_bulkwriteresult_insertresults (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   if (!self->verboseresults) {
      return NULL;
   }
   return &self->insertresults;
}

const bson_t *
mongoc_bulkwriteresult_updateresults (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   if (!self->verboseresults) {
      return NULL;
   }
   return &self->updateresults;
}

const bson_t *
mongoc_bulkwriteresult_deleteresults (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   if (!self->verboseresults) {
      return NULL;
   }
   return &self->deleteresults;
}

uint32_t
mongoc_bulkwriteresult_serverid (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->serverid;
}

void
mongoc_bulkwriteresult_destroy (mongoc_bulkwriteresult_t *self)
{
   if (!self) {
      return;
   }
   bson_destroy (&self->deleteresults);
   bson_destroy (&self->updateresults);
   bson_destroy (&self->insertresults);
   bson_free (self);
}

static mongoc_bulkwriteresult_t *
_bulkwriteresult_new (void)
{
   mongoc_bulkwriteresult_t *self = bson_malloc0 (sizeof (*self));
   bson_init (&self->insertresults);
   bson_init (&self->updateresults);
   bson_init (&self->deleteresults);
   return self;
}

static bool
_bulkwriteresult_set_updateresult (mongoc_bulkwriteresult_t *self,
                                   int32_t n,
                                   int32_t nModified,
                                   const bson_value_t *upserted_id,
                                   size_t models_idx,
                                   bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (upserted_id || true);
   BSON_ASSERT (error || true);

   bson_t updateresult;
   {
      char *key = bson_strdup_printf ("%zu", models_idx);
      BSON_APPEND_DOCUMENT_BEGIN (&self->updateresults, key, &updateresult);
      bson_free (key);
   }

   bool bson_ok = true;
   bson_ok = bson_ok && bson_append_int32 (&updateresult, "matchedCount", 12, n);
   bson_ok = bson_ok && bson_append_int32 (&updateresult, "modifiedCount", 13, nModified);
   if (upserted_id) {
      bson_ok = bson_ok && bson_append_value (&updateresult, "upsertedId", 10, upserted_id);
   }
   bson_ok = bson_ok && bson_append_document_end (&self->updateresults, &updateresult);
   if (!bson_ok) {
      bson_set_error (error, MONGOC_ERROR_BSON, MONGOC_ERROR_BSON_INVALID, "failed to build update result");
      return false;
   }
   return true;
}

static bool
_bulkwriteresult_set_deleteresult (mongoc_bulkwriteresult_t *self, int32_t n, size_t models_idx, bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (error || true);

   bson_t deleteresult;
   {
      char *key = bson_strdup_printf ("%zu", models_idx);
      BSON_APPEND_DOCUMENT_BEGIN (&self->deleteresults, key, &deleteresult);
      bson_free (key);
   }

   bool bson_ok = true;
   bson_ok = bson_ok && bson_append_int32 (&deleteresult, "deletedCount", 12, n);
   bson_ok = bson_ok && bson_append_document_end (&self->deleteresults, &deleteresult);
   if (!bson_ok) {
      bson_set_error (error, MONGOC_ERROR_BSON, MONGOC_ERROR_BSON_INVALID, "failed to build delete result");
      return false;
   }
   return true;
}

static bool
_bulkwriteresult_set_insertresult (mongoc_bulkwriteresult_t *self,
                                   const bson_iter_t *id_iter,
                                   size_t models_idx,
                                   bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (id_iter);
   BSON_ASSERT (error || true);

   bson_t insertresult;
   {
      char *key = bson_strdup_printf ("%zu", models_idx);
      BSON_APPEND_DOCUMENT_BEGIN (&self->insertresults, key, &insertresult);
      bson_free (key);
   }

   bool bson_ok = true;
   bson_ok = bson_ok && bson_append_iter (&insertresult, "insertedId", 10, id_iter);
   bson_ok = bson_ok && bson_append_document_end (&self->insertresults, &insertresult);
   if (!bson_ok) {
      bson_set_error (error, MONGOC_ERROR_BSON, MONGOC_ERROR_BSON_INVALID, "failed to build insert result");
      return false;
   }

   return true;
}

struct _mongoc_bulkwriteexception_t {
   bson_error_t error;
   bson_t error_reply;
   bson_t write_concern_errors;
   size_t write_concern_errors_len;
   bson_t write_errors;
   // If `has_any_error` is false, the bulk write exception is not returned.
   bool has_any_error;
};

static mongoc_bulkwriteexception_t *
_bulkwriteexception_new (void)
{
   mongoc_bulkwriteexception_t *self = bson_malloc0 (sizeof (*self));
   bson_init (&self->write_concern_errors);
   bson_init (&self->write_errors);
   bson_init (&self->error_reply);
   return self;
}

// Returns true if there was a top-level error.
bool
mongoc_bulkwriteexception_error (const mongoc_bulkwriteexception_t *self, bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (error || true);

   if (self->error.code != 0) {
      memcpy (error, &self->error, sizeof (*error));
      return true;
   }
   return false; // No top-level error.
}

const bson_t *
mongoc_bulkwriteexception_writeerrors (const mongoc_bulkwriteexception_t *self)
{
   BSON_ASSERT_PARAM (self);
   return &self->write_errors;
}

const bson_t *
mongoc_bulkwriteexception_writeconcernerrors (const mongoc_bulkwriteexception_t *self)
{
   BSON_ASSERT_PARAM (self);
   return &self->write_concern_errors;
}

const bson_t *
mongoc_bulkwriteexception_errorreply (const mongoc_bulkwriteexception_t *self)
{
   BSON_ASSERT_PARAM (self);
   return &self->error_reply;
}

void
mongoc_bulkwriteexception_destroy (mongoc_bulkwriteexception_t *self)
{
   if (!self) {
      return;
   }
   bson_destroy (&self->write_errors);
   bson_destroy (&self->write_concern_errors);
   bson_destroy (&self->error_reply);
   bson_free (self);
}

static void
_bulkwriteexception_set_error (mongoc_bulkwriteexception_t *self, bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (error);

   BSON_ASSERT (error->code != 0);
   memcpy (&self->error, error, sizeof (*error));
   self->has_any_error = true;
}

static void
_bulkwriteexception_set_error_reply (mongoc_bulkwriteexception_t *self, const bson_t *error_reply)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (error_reply);

   bson_copy_to (error_reply, &self->error_reply);
   self->has_any_error = true;
}

static bool
_bulkwriteexception_append_writeconcernerror (mongoc_bulkwriteexception_t *self,
                                              int32_t code,
                                              const char *errmsg,
                                              const bson_t *errInfo)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (errmsg);
   BSON_ASSERT_PARAM (errInfo);

   char *key = bson_strdup_printf ("%zu", self->write_concern_errors_len);
   self->write_concern_errors_len++;

   bson_error_t error;
   bson_t write_concern_error;
   bool bson_ok = true;
   bson_ok = bson_ok && bson_append_document_begin (&self->write_concern_errors, key, -1, &write_concern_error);
   bson_ok = bson_ok && bson_append_int32 (&write_concern_error, "code", 4, code);
   bson_ok = bson_ok && bson_append_utf8 (&write_concern_error, "message", 7, errmsg, -1);
   bson_ok = bson_ok && bson_append_document (&write_concern_error, "details", 7, errInfo);
   bson_ok = bson_ok && bson_append_document_end (&self->write_concern_errors, &write_concern_error);
   if (!bson_ok) {
      // Check for BSON building error. `errmsg` and `errInfo` come from a server reply.
      bson_set_error (&error, MONGOC_ERROR_BSON, MONGOC_ERROR_BSON_INVALID, "failed to build writeConcernError");
      _bulkwriteexception_set_error (self, &error);
      bson_free (key);
      return false;
   }
   self->has_any_error = true;
   bson_free (key);
   return true;
}

static bool
_bulkwriteexception_set_writeerror (
   mongoc_bulkwriteexception_t *self, int32_t code, const char *errmsg, const bson_t *errInfo, size_t models_idx)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (errmsg);
   BSON_ASSERT_PARAM (errInfo);

   bson_error_t error;
   bson_t write_error;
   {
      char *key = bson_strdup_printf ("%zu", models_idx);
      BSON_APPEND_DOCUMENT_BEGIN (&self->write_errors, key, &write_error);
      bson_free (key);
   }

   bool bson_ok = bson_append_int32 (&write_error, "code", 4, code);
   bson_ok = bson_ok && bson_append_utf8 (&write_error, "message", 7, errmsg, -1);
   bson_ok = bson_ok && bson_append_document (&write_error, "details", 7, errInfo);
   bson_ok = bson_ok && bson_append_document_end (&self->write_errors, &write_error);
   if (!bson_ok) {
      // Check for BSON building error. `errmsg` and `errInfo` come from a server reply.
      bson_set_error (&error, MONGOC_ERROR_BSON, MONGOC_ERROR_BSON_INVALID, "failed to build writeError");
      _bulkwriteexception_set_error (self, &error);
      return false;
   }
   self->has_any_error = true;
   return true;
}

static bool
lookup_int32 (const bson_t *bson, const char *key, int32_t *out, const char *source, mongoc_bulkwriteexception_t *exc)
{
   BSON_ASSERT_PARAM (bson);
   BSON_ASSERT_PARAM (key);
   BSON_ASSERT_PARAM (out);
   BSON_ASSERT (source || true);
   BSON_ASSERT_PARAM (exc);

   bson_iter_t iter;
   if (bson_iter_init_find (&iter, bson, key) && BSON_ITER_HOLDS_INT32 (&iter)) {
      *out = bson_iter_int32 (&iter);
      return true;
   }
   bson_error_t error;
   if (source) {
      bson_set_error (&error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "expected to find int32 `%s` in %s, but did not",
                      key,
                      source);
   } else {
      bson_set_error (&error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "expected to find int32 `%s`, but did not",
                      key);
   }
   _bulkwriteexception_set_error (exc, &error);
   return false;
}

static bool
lookup_double (const bson_t *bson, const char *key, double *out, const char *source, mongoc_bulkwriteexception_t *exc)
{
   BSON_ASSERT_PARAM (bson);
   BSON_ASSERT_PARAM (key);
   BSON_ASSERT_PARAM (out);
   BSON_ASSERT (source || true);
   BSON_ASSERT_PARAM (exc);

   bson_iter_t iter;
   if (bson_iter_init_find (&iter, bson, key) && BSON_ITER_HOLDS_DOUBLE (&iter)) {
      *out = bson_iter_double (&iter);
      return true;
   }
   bson_error_t error;
   if (source) {
      bson_set_error (&error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "expected to find double `%s` in %s, but did not",
                      key,
                      source);
   } else {
      bson_set_error (&error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "expected to find double `%s`, but did not",
                      key);
   }
   _bulkwriteexception_set_error (exc, &error);
   return false;
}

static bool
lookup_string (
   const bson_t *bson, const char *key, const char **out, const char *source, mongoc_bulkwriteexception_t *exc)
{
   BSON_ASSERT_PARAM (bson);
   BSON_ASSERT_PARAM (key);
   BSON_ASSERT_PARAM (out);
   BSON_ASSERT (source || true);
   BSON_ASSERT_PARAM (exc);

   bson_iter_t iter;
   if (bson_iter_init_find (&iter, bson, key) && BSON_ITER_HOLDS_UTF8 (&iter)) {
      *out = bson_iter_utf8 (&iter, NULL);
      return true;
   }
   bson_error_t error;
   if (source) {
      bson_set_error (&error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "expected to find string `%s` in %s, but did not",
                      key,
                      source);
   } else {
      bson_set_error (&error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "expected to find string `%s`, but did not",
                      key);
   }
   _bulkwriteexception_set_error (exc, &error);
   return false;
}

typedef struct {
   bson_t ns_to_index; // TODO: replace with a hash map with faster look-up.
   int32_t count;
   mongoc_buffer_t payload;
} nsinfo_list_t;

static nsinfo_list_t *
nsinfo_list_new (void)
{
   nsinfo_list_t *self = bson_malloc0 (sizeof (*self));
   bson_init (&self->ns_to_index);
   _mongoc_buffer_init (&self->payload, NULL, 0, NULL, NULL);
   return self;
}

static void
nsinfo_list_destroy (nsinfo_list_t *self)
{
   if (!self) {
      return;
   }
   bson_destroy (&self->ns_to_index);
   _mongoc_buffer_destroy (&self->payload);
   bson_free (self);
}

// `nsinfo_list_append` adds to the list. It is the callers responsibility to ensure duplicates are not inserted.
// `*ns_index` is set to the resulting index in the list.
// Returns -1 on error.
static int32_t
nsinfo_list_append (nsinfo_list_t *self, const char *ns, int ns_len, bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (ns);
   BSON_ASSERT (error || true);

   int32_t ns_index = self->count;
   if (self->count == INT32_MAX) {
      bson_set_error (error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "Only %" PRId32 " distinct collections may be used",
                      INT32_MAX);
      return -1;
   }
   self->count++;
   BSON_ASSERT (bson_append_int32 (&self->ns_to_index, ns, ns_len, ns_index));
   // Append to buffer.
   bson_t nsinfo_bson = BSON_INITIALIZER;
   BSON_ASSERT (bson_append_utf8 (&nsinfo_bson, "ns", 2, ns, ns_len));
   BSON_ASSERT (_mongoc_buffer_append (&self->payload, bson_get_data (&nsinfo_bson), nsinfo_bson.len));
   bson_destroy (&nsinfo_bson);
   return ns_index;
}

static int32_t
nsinfo_list_find (nsinfo_list_t *self, const char *ns, int ns_len)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (ns);
   BSON_UNUSED (ns_len);

   bson_iter_t iter;
   if (bson_iter_init_find (&iter, &self->ns_to_index, ns)) {
      return bson_iter_int32 (&iter);
   }
   return -1;
}

static uint32_t
nsinfo_list_get_bson_size (nsinfo_list_t *self, const char *ns, int ns_len)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (ns);
   // Calculate overhead of the BSON document { "ns": "<ns>" }. See BSON specification.
   bson_t as_bson = BSON_INITIALIZER;
   BSON_ASSERT (bson_append_utf8 (&as_bson, "ns", 2, ns, ns_len));
   uint32_t size = as_bson.len;
   bson_destroy (&as_bson);
   return size;
}

const mongoc_buffer_t *
nsinfo_list_as_document_sequence (nsinfo_list_t *self)
{
   return &self->payload;
}

mongoc_bulkwritereturn_t
mongoc_bulkwrite_execute (mongoc_bulkwrite_t *self, mongoc_bulkwriteopts_t *opts)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (opts || true);

   mongoc_bulkwritereturn_t ret = {0};
   bson_error_t error = {0};
   mongoc_server_stream_t *ss = NULL;
   bson_t cmd = BSON_INITIALIZER;
   mongoc_cmd_parts_t parts = {{0}};
   mongoc_bulkwriteopts_t defaults = {{0}};

   typedef struct {
      bson_t *bson; // Use a pointer since `bson_t` is not trivially relocatable.
      bool used;
      int32_t index; // Index into the batch payload.
   } nsinfo_t;
   mongoc_array_t all_nsinfos; // Maps the command index into the batch index.
   _mongoc_array_init (&all_nsinfos, sizeof (nsinfo_t));
   mongoc_buffer_t nsInfo_payload;
   if (!opts) {
      opts = &defaults;
   }
   bool is_acknowledged = false;
   // Create empty result and exception to collect results/errors from batches.
   ret.res = _bulkwriteresult_new ();
   ret.exc = _bulkwriteexception_new ();
   ret.res->serverid = opts->serverid;
   _mongoc_buffer_init (&nsInfo_payload, NULL, 0, NULL, NULL);

   if (self->executed) {
      bson_set_error (&error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "bulk write already executed");
      _bulkwriteexception_set_error (ret.exc, &error);
      goto fail;
   }
   self->executed = true;

   if (self->n_ops == 0) {
      bson_set_error (
         &error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "cannot do `bulkWrite` with no models");
      _bulkwriteexception_set_error (ret.exc, &error);
      goto fail;
   }

   // Select a stream.
   {
      bson_t reply;

      if (opts->serverid) {
         ss = mongoc_cluster_stream_for_server (
            &self->client->cluster, opts->serverid, true /* reconnect_ok */, opts->session, &reply, &error);
      } else {
         ss = mongoc_cluster_stream_for_writes (
            &self->client->cluster, opts->session, NULL /* deprioritized servers */, &reply, &error);
      }

      if (!ss) {
         _bulkwriteexception_set_error (ret.exc, &error);
         _bulkwriteexception_set_error_reply (ret.exc, &reply);
         bson_destroy (&reply);
         goto fail;
      } else {
         ret.res->serverid = ss->sd->id;
      }
   }

   bool verboseresults = (opts->verboseresults.isset) ? opts->verboseresults.val : false;
   ret.res->verboseresults = verboseresults;

   int32_t maxBsonObjectSize = mongoc_server_stream_max_bson_obj_size (ss);
   // Create the payload 0.
   {
      BSON_ASSERT (bson_append_int32 (&cmd, "bulkWrite", 9, 1));
      // errorsOnly is default true. Set to false if verboseResults requested.
      BSON_ASSERT (bson_append_bool (&cmd, "errorsOnly", 10, !verboseresults));
      // ordered is default true.
      BSON_ASSERT (bson_append_bool (&cmd, "ordered", 7, (opts->ordered.isset) ? opts->ordered.val : true));

      if (opts->comment) {
         BSON_ASSERT (bson_append_document (&cmd, "comment", 7, opts->comment));
      }

      if (opts->bypassdocumentvalidation.isset) {
         BSON_ASSERT (bson_append_bool (&cmd, "bypassDocumentValidation", 24, opts->bypassdocumentvalidation.val));
      }

      if (opts->let) {
         BSON_ASSERT (bson_append_document (&cmd, "let", 3, opts->let));
      }

      // Add optional extra fields.
      if (opts->extra) {
         if (!bson_concat (&cmd, opts->extra)) {
            bson_set_error (&error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "failed to append extra");
            _bulkwriteexception_set_error (ret.exc, &error);
            goto fail;
         }
      }

      mongoc_cmd_parts_init (&parts, self->client, "admin", MONGOC_QUERY_NONE, &cmd);
      parts.assembled.operation_id = self->operation_id;

      parts.allow_txn_number = MONGOC_CMD_PARTS_ALLOW_TXN_NUMBER_YES; // To append `lsid`.
      if (self->has_multi_write) {
         // Write commands that include multi-document operations are not
         // retryable.
         parts.allow_txn_number = MONGOC_CMD_PARTS_ALLOW_TXN_NUMBER_NO;
      }
      parts.is_write_command = true; // To append `txnNumber`.

      if (opts->session) {
         mongoc_cmd_parts_set_session (&parts, opts->session);
      }

      // Apply write concern:
      {
         const mongoc_write_concern_t *wc = self->client->write_concern; // Default to client.
         if (opts->writeconcern) {
            if (_mongoc_client_session_in_txn (opts->session)) {
               bson_set_error (&error,
                               MONGOC_ERROR_COMMAND,
                               MONGOC_ERROR_COMMAND_INVALID_ARG,
                               "Cannot set write concern after starting a transaction.");
               _bulkwriteexception_set_error (ret.exc, &error);
               goto fail;
            }
            wc = opts->writeconcern;
         }
         if (!mongoc_cmd_parts_set_write_concern (&parts, wc, &error)) {
            _bulkwriteexception_set_error (ret.exc, &error);
            goto fail;
         }
         if (!mongoc_write_concern_is_acknowledged (wc) &&
             bson_cmp_greater_us (self->max_insert_len, maxBsonObjectSize)) {
            bson_set_error (&error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "Unacknowledged `bulkWrite` includes insert of size: %" PRIu32
                            ", exceeding maxBsonObjectSize: %" PRId32,
                            self->max_insert_len,
                            maxBsonObjectSize);
            _bulkwriteexception_set_error (ret.exc, &error);
            goto fail;
         }
         is_acknowledged = mongoc_write_concern_is_acknowledged (wc);
      }

      if (!mongoc_cmd_parts_assemble (&parts, ss, &error)) {
         _bulkwriteexception_set_error (ret.exc, &error);
         goto fail;
      }
   }


   // Send one or more `bulkWrite` commands. Split input payload if necessary to
   // satisfy server size limits.
   int32_t maxWriteBatchSize = mongoc_server_stream_max_write_batch_size (ss);
   int32_t maxMessageSizeBytes = mongoc_server_stream_max_msg_size (ss);
   size_t writeBatchSize_offset = 0;
   size_t payload_offset = 0;
   // Calculate overhead of OP_MSG and the `bulkWrite` command. See bulk write specification for explanation.
   size_t opmsg_overhead = 0;
   {
      opmsg_overhead += 1000;
      // Add size of `bulkWrite` command. Exclude command-agnostic fields added in `mongoc_cmd_parts_assemble` (e.g.
      // `txnNumber` and `lsid`).
      opmsg_overhead += cmd.len;
   }

   while (true) {
      bool has_write_errors = false;
      bool batch_ok = false;
      bson_t cmd_reply = BSON_INITIALIZER;
      mongoc_cursor_t *reply_cursor = NULL;
      size_t payload_len = 0;
      size_t payload_writeBatchSize = 0;

      if (payload_offset == self->ops.len) {
         // All write models were sent.
         break;
      }

      // Track the nsInfo entries to include in this batch.
      nsinfo_list_t *nl = nsinfo_list_new ();

      // Read as many documents from payload as possible.
      while (true) {
         if (payload_offset + payload_len >= self->ops.len) {
            // All remaining ops are readied.
            break;
         }

         if (payload_writeBatchSize >= maxWriteBatchSize) {
            // Maximum number of operations are readied.
            break;
         }

         // Read length of next document.
         uint32_t ulen;
         memcpy (&ulen, self->ops.data + payload_offset + payload_len, 4);
         ulen = BSON_UINT32_FROM_LE (ulen);

         // Check if adding this operation requires adding an `nsInfo` entry.
         // `models_idx` is the index of the model that produced this result.
         size_t models_idx = payload_writeBatchSize + writeBatchSize_offset;
         model_data_t *md = &_mongoc_array_index (&self->model_entries, model_data_t, models_idx);
         uint32_t nsinfo_bson_size = 0;
         int32_t ns_index = nsinfo_list_find (nl, md->ns, md->ns_len);
         if (ns_index == -1) {
            // Need to append `nsInfo` entry. Append after checking that both the document and the `nsInfo` entry fit.
            nsinfo_bson_size = nsinfo_list_get_bson_size (nl, md->ns, md->ns_len);
         }

         if (opmsg_overhead + payload_len + ulen + nsinfo_bson_size > maxMessageSizeBytes) {
            if (payload_len == 0) {
               // Could not even fit one document within an OP_MSG.
               bson_set_error (&error,
                               MONGOC_ERROR_COMMAND,
                               MONGOC_ERROR_COMMAND_INVALID_ARG,
                               "unable to send document at index %zu. Sending "
                               "would exceed maxMessageSizeBytes=%" PRId32,
                               payload_writeBatchSize,
                               maxMessageSizeBytes);
               _bulkwriteexception_set_error (ret.exc, &error);
               goto batch_fail;
            }
            break;
         }

         // Check if a new `nsInfo` entry is needed.
         if (ns_index == -1) {
            ns_index = nsinfo_list_append (nl, md->ns, md->ns_len, &error);
            if (ns_index == -1) {
               _bulkwriteexception_set_error (ret.exc, &error);
               goto batch_fail;
            }
         }

         // Overwrite the placeholder to the index of the `nsInfo` entry.
         {
            bson_iter_t nsinfo_iter;
            bson_t doc;
            BSON_ASSERT (bson_init_static (&doc, self->ops.data + payload_offset + payload_len, ulen));
            // Find the index.
            BSON_ASSERT (bson_iter_init (&nsinfo_iter, &doc));
            BSON_ASSERT (bson_iter_next (&nsinfo_iter));
            bson_iter_overwrite_int32 (&nsinfo_iter, ns_index);
         }

         // Include document.
         {
            payload_len += ulen;
            payload_writeBatchSize += 1;
         }
      }

      // Send batch.
      {
         parts.assembled.payloads_count = 2;

         // Create the `nsInfo` payload.
         {
            mongoc_cmd_payload_t *payload = &parts.assembled.payloads[0];
            const mongoc_buffer_t *nsinfo_docseq = nsinfo_list_as_document_sequence (nl);
            payload->documents = nsinfo_docseq->data;
            BSON_ASSERT (bson_in_range_int32_t_unsigned (nsinfo_docseq->len));
            payload->size = (int32_t) nsinfo_docseq->len;
            payload->identifier = "nsInfo";
         }

         // Create the `ops` payload.
         {
            mongoc_cmd_payload_t *payload = &parts.assembled.payloads[1];
            payload->identifier = "ops";
            payload->documents = self->ops.data + payload_offset;
            BSON_ASSERT (bson_in_range_int32_t_unsigned (payload_len));
            payload->size = (int32_t) payload_len;
         }

         // Send command.
         {
            mongoc_server_stream_t *new_ss = NULL;
            bool ok = mongoc_cluster_run_retryable_write (
               &self->client->cluster, &parts.assembled, parts.is_retryable_write, &new_ss, &cmd_reply, &error);
            if (new_ss) {
               mongoc_server_stream_cleanup (ss);
               ss = new_ss;
               parts.assembled.server_stream = ss;
            }

            // Check for a command ('ok': 0) error.
            if (!ok) {
               if (error.code != 0) {
                  // The original error was a command ('ok': 0) error.
                  _bulkwriteexception_set_error (ret.exc, &error);
               }
               _bulkwriteexception_set_error_reply (ret.exc, &cmd_reply);
               goto batch_fail;
            }
         }

         // Add to result and/or exception.
         if (is_acknowledged) {
            bson_iter_t iter;

            // Parse top-level fields.
            {
               int32_t nInserted;
               if (!lookup_int32 (&cmd_reply, "nInserted", &nInserted, NULL, ret.exc)) {
                  goto batch_fail;
               }
               ret.res->insertedcount += (int64_t) nInserted;

               int32_t nMatched;
               if (!lookup_int32 (&cmd_reply, "nMatched", &nMatched, NULL, ret.exc)) {
                  goto batch_fail;
               }
               ret.res->matchedcount += (int64_t) nMatched;

               int32_t nModified;
               if (!lookup_int32 (&cmd_reply, "nModified", &nModified, NULL, ret.exc)) {
                  goto batch_fail;
               }
               ret.res->modifiedcount += (int64_t) nModified;

               int32_t nDeleted;
               if (!lookup_int32 (&cmd_reply, "nDeleted", &nDeleted, NULL, ret.exc)) {
                  goto batch_fail;
               }
               ret.res->deletedcount += (int64_t) nDeleted;

               int32_t nUpserted;
               if (!lookup_int32 (&cmd_reply, "nUpserted", &nUpserted, NULL, ret.exc)) {
                  goto batch_fail;
               }
               ret.res->upsertedcount += (int64_t) nUpserted;
            }

            if (bson_iter_init_find (&iter, &cmd_reply, "writeConcernError")) {
               bson_iter_t wce_iter;
               bson_t wce_bson;

               if (!_mongoc_iter_document_as_bson (&iter, &wce_bson, &error)) {
                  _bulkwriteexception_set_error (ret.exc, &error);
                  _bulkwriteexception_set_error_reply (ret.exc, &cmd_reply);
                  goto batch_fail;
               }

               // Parse `code`.
               int32_t code;
               if (!lookup_int32 (&wce_bson, "code", &code, "writeConcernError", ret.exc)) {
                  goto batch_fail;
               }

               // Parse `errmsg`.
               const char *errmsg;
               if (!lookup_string (&wce_bson, "errmsg", &errmsg, "writeConcernError", ret.exc)) {
                  goto batch_fail;
               }

               // Parse optional `errInfo`.
               bson_t errInfo = BSON_INITIALIZER;
               if (bson_iter_init_find (&wce_iter, &wce_bson, "errInfo")) {
                  if (!_mongoc_iter_document_as_bson (&wce_iter, &errInfo, &error)) {
                     _bulkwriteexception_set_error (ret.exc, &error);
                     _bulkwriteexception_set_error_reply (ret.exc, &cmd_reply);
                  }
               }

               if (!_bulkwriteexception_append_writeconcernerror (ret.exc, code, errmsg, &errInfo)) {
                  goto batch_fail;
               }
            }

            {
               bson_t cursor_opts = BSON_INITIALIZER;
               {
                  uint32_t serverid = parts.assembled.server_stream->sd->id;
                  BSON_ASSERT (bson_in_range_int_unsigned (serverid));
                  int serverid_int = (int) serverid;
                  BSON_ASSERT (bson_append_int32 (&cursor_opts, "serverId", 8, serverid_int));
                  // Use same session.
                  if (!mongoc_client_session_append (parts.assembled.session, &cursor_opts, &error)) {
                     _bulkwriteexception_set_error (ret.exc, &error);
                     _bulkwriteexception_set_error_reply (ret.exc, &cmd_reply);
                     goto batch_fail;
                  }
               }

               // Construct the reply cursor.
               reply_cursor = mongoc_cursor_new_from_command_reply_with_opts (self->client, &cmd_reply, &cursor_opts);
               bson_destroy (&cursor_opts);
               // `cmd_reply` is stolen. Clear it.
               bson_init (&cmd_reply);

               // Ensure constructing cursor did not error.
               {
                  const bson_t *error_document;
                  if (mongoc_cursor_error_document (reply_cursor, &error, &error_document)) {
                     _bulkwriteexception_set_error (ret.exc, &error);
                     if (error_document) {
                        _bulkwriteexception_set_error_reply (ret.exc, error_document);
                     }
                     goto batch_fail;
                  }
               }

               // Iterate.
               const bson_t *result;
               while (mongoc_cursor_next (reply_cursor, &result)) {
                  // Parse for `ok`.
                  double ok;
                  if (!lookup_double (result, "ok", &ok, "result", ret.exc)) {
                     goto batch_fail;
                  }


                  // Parse `idx`.
                  int32_t idx;
                  {
                     if (!lookup_int32 (result, "idx", &idx, "result", ret.exc)) {
                        goto batch_fail;
                     }
                     if (idx < 0) {
                        bson_set_error (&error,
                                        MONGOC_ERROR_COMMAND,
                                        MONGOC_ERROR_COMMAND_INVALID_ARG,
                                        "expected to find non-negative int32 `idx` in "
                                        "result, but did not");
                        _bulkwriteexception_set_error (ret.exc, &error);
                        goto batch_fail;
                     }
                  }

                  BSON_ASSERT (bson_in_range_size_t_signed (idx));
                  // `models_idx` is the index of the model that produced this result.
                  size_t models_idx = (size_t) idx + writeBatchSize_offset;
                  if (ok == 0) {
                     bson_iter_t result_iter;
                     has_write_errors = true;

                     // Parse `code`.
                     int32_t code;
                     if (!lookup_int32 (result, "code", &code, "result", ret.exc)) {
                        goto batch_fail;
                     }

                     // Parse `errmsg`.
                     const char *errmsg;
                     if (!lookup_string (result, "errmsg", &errmsg, "result", ret.exc)) {
                        goto batch_fail;
                     }

                     // Parse optional `errInfo`.
                     bson_t errInfo = BSON_INITIALIZER;
                     if (bson_iter_init_find (&result_iter, result, "errInfo")) {
                        if (!_mongoc_iter_document_as_bson (&result_iter, &errInfo, &error)) {
                           _bulkwriteexception_set_error (ret.exc, &error);
                           goto batch_fail;
                        }
                     }

                     // Store a copy of the write error.
                     if (!_bulkwriteexception_set_writeerror (ret.exc, code, errmsg, &errInfo, models_idx)) {
                        goto batch_fail;
                     }
                  } else {
                     // This is a successful result of an individual operation.
                     // Server only reports successful results of individual
                     // operations when verbose results are requested
                     // (`errorsOnly: false` is sent).

                     model_data_t *md = &_mongoc_array_index (&self->model_entries, model_data_t, models_idx);
                     // Check if model is an update.
                     switch (md->op) {
                     case MODEL_OP_UPDATE: {
                        bson_iter_t result_iter;
                        // Parse `n`.
                        int32_t n;
                        if (!lookup_int32 (result, "n", &n, "result", ret.exc)) {
                           goto batch_fail;
                        }

                        // Parse `nModified`.
                        int32_t nModified;
                        if (!lookup_int32 (result, "nModified", &nModified, "result", ret.exc)) {
                           goto batch_fail;
                        }

                        // Check for an optional `upsertedId`.
                        const bson_value_t *upserted_id = NULL;
                        bson_iter_t id_iter;
                        if (bson_iter_init_find (&result_iter, result, "upserted")) {
                           BSON_ASSERT (bson_iter_init (&result_iter, result));
                           if (!bson_iter_find_descendant (&result_iter, "upserted._id", &id_iter)) {
                              bson_set_error (&error,
                                              MONGOC_ERROR_COMMAND,
                                              MONGOC_ERROR_COMMAND_INVALID_ARG,
                                              "expected `upserted` to be a document "
                                              "containing `_id`, but did not find `_id`");
                              _bulkwriteexception_set_error (ret.exc, &error);
                              goto batch_fail;
                           }
                           upserted_id = bson_iter_value (&id_iter);
                        }

                        if (!_bulkwriteresult_set_updateresult (
                               ret.res, n, nModified, upserted_id, models_idx, &error)) {
                           _bulkwriteexception_set_error (ret.exc, &error);
                           goto batch_fail;
                        }
                        break;
                     }
                     case MODEL_OP_DELETE: {
                        // Parse `n`.
                        int32_t n;
                        if (!lookup_int32 (result, "n", &n, "result", ret.exc)) {
                           goto batch_fail;
                        }

                        if (!_bulkwriteresult_set_deleteresult (ret.res, n, models_idx, &error)) {
                           _bulkwriteexception_set_error (ret.exc, &error);
                           goto batch_fail;
                        }
                        break;
                     }
                     case MODEL_OP_INSERT: {
                        if (!_bulkwriteresult_set_insertresult (ret.res, &md->id_iter, models_idx, &error)) {
                           _bulkwriteexception_set_error (ret.exc, &error);
                           goto batch_fail;
                        }
                        break;
                     }
                     }
                  }
               }
               // Ensure iterating cursor did not error.
               {
                  const bson_t *error_document;
                  if (mongoc_cursor_error_document (reply_cursor, &error, &error_document)) {
                     _bulkwriteexception_set_error (ret.exc, &error);
                     if (error_document) {
                        _bulkwriteexception_set_error_reply (ret.exc, error_document);
                     }
                     goto batch_fail;
                  }
               }
            }
         }

         // Check if stream is valid.
         // `mongoc_cluster_run_retryable_write` may have invalidated stream (e.g. due to processing an error).
         // If invalid, select a new stream before processing more batches.
         if (!mongoc_cluster_stream_valid (&self->client->cluster, parts.assembled.server_stream)) {
            bson_t reply;
            // Select a server and create a stream again.
            mongoc_server_stream_cleanup (ss);
            ss = mongoc_cluster_stream_for_writes (
               &self->client->cluster, NULL /* session */, NULL /* deprioritized servers */, &reply, &error);

            if (ss) {
               parts.assembled.server_stream = ss;
            } else {
               _bulkwriteexception_set_error (ret.exc, &error);
               goto batch_fail;
            }
         }
      }

      writeBatchSize_offset += payload_writeBatchSize;
      payload_offset += payload_len;
      batch_ok = true;
   batch_fail:
      nsinfo_list_destroy (nl);
      mongoc_cursor_destroy (reply_cursor);
      bson_destroy (&cmd_reply);
      if (!batch_ok) {
         goto fail;
      }
      bool is_ordered = opts->ordered.isset ? opts->ordered.val : true; // default.
      if (has_write_errors && is_ordered) {
         // Ordered writes must not continue to send batches once an error is
         // occurred. An individual write error is not a top-level error.
         break;
      }
   }

fail:
   for (size_t i = 0; i < all_nsinfos.len; i++) {
      nsinfo_t nsinfo = _mongoc_array_index (&all_nsinfos, nsinfo_t, i);
      bson_destroy (nsinfo.bson);
   }
   _mongoc_array_destroy (&all_nsinfos);
   _mongoc_buffer_destroy (&nsInfo_payload);
   if (!is_acknowledged) {
      mongoc_bulkwriteresult_destroy (ret.res);
      ret.res = NULL;
   }
   if (parts.body) {
      // Only clean-up if initialized.
      mongoc_cmd_parts_cleanup (&parts);
   }
   bson_destroy (&cmd);
   mongoc_server_stream_cleanup (ss);
   if (!ret.exc->has_any_error) {
      mongoc_bulkwriteexception_destroy (ret.exc);
      ret.exc = NULL;
   }
   return ret;
}

MC_ENABLE_CONVERSION_WARNING_END
