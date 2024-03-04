#include <mongoc/mongoc.h>
#include <mongoc-array-private.h>
#include <mongoc-client-private.h>
#include <mongoc-cursor.h>
#include <mongoc-util-private.h>


// This is an API proposal attempting to closely match the specification:
typedef struct _mongoc_listof_bulkwritemodel_t {
   // `ops` is a document sequence.
   mongoc_buffer_t ops;
   size_t n_ops;
   // `ns_to_index` maps a namespace to an index.
   bson_t ns_to_index;
   // `inserted_ids` is an array sized to the number of operations.
   // If the operation was an insert, the `id` is stored.
   mongoc_array_t inserted_ids;
} mongoc_listof_bulkwritemodel_t;

typedef struct _inserted_ids_entry {
   bool is_insert;
   bson_iter_t id_iter;  // Iterator to the `_id` field.
   bool has_write_error; // True if insert was attempted but failed.
} inserted_ids_entry;

mongoc_listof_bulkwritemodel_t *
mongoc_listof_bulkwritemodel_new (void)
{
   mongoc_listof_bulkwritemodel_t *self =
      bson_malloc0 (sizeof (mongoc_listof_bulkwritemodel_t));
   _mongoc_buffer_init (&self->ops, NULL, 0, NULL, NULL);
   bson_init (&self->ns_to_index);
   _mongoc_array_init (&self->inserted_ids, sizeof (inserted_ids_entry));
   return self;
}

void
mongoc_listof_bulkwritemodel_destroy (mongoc_listof_bulkwritemodel_t *self)
{
   _mongoc_array_destroy (&self->inserted_ids);
   bson_destroy (&self->ns_to_index);
   _mongoc_buffer_destroy (&self->ops);
   bson_free (self);
}

bool
mongoc_listof_bulkwritemodel_append_insertone (
   mongoc_listof_bulkwritemodel_t *self,
   const char *namespace,
   int namespace_len,
   bson_t *document,
   bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (namespace);
   BSON_ASSERT_PARAM (document);

   BSON_ASSERT (document->len >= 5);

   bson_t op = BSON_INITIALIZER;

   // Find or create the namespace index.
   {
      bson_iter_t iter;
      int32_t ns_index;
      if (bson_iter_init_find (&iter, &self->ns_to_index, namespace)) {
         ns_index = bson_iter_int32 (&iter);
      } else {
         uint32_t key_count = bson_count_keys (&self->ns_to_index);
         if (!bson_in_range_int32_t_unsigned (key_count)) {
            bson_set_error (
               error,
               MONGOC_ERROR_COMMAND,
               MONGOC_ERROR_COMMAND_INVALID_ARG,
               "Only %" PRId32
               " distinct collections may be inserted into. Got %" PRIu32,
               INT32_MAX,
               key_count);
            return false;
         }
         ns_index = (int32_t) key_count;
         bson_append_int32 (
            &self->ns_to_index, namespace, namespace_len, ns_index);
      }
      BSON_ASSERT (bson_append_int32 (&op, "insert", 6, ns_index));
   }

   // If `document` does not contain `_id`, generate one.
   bson_iter_t id_iter;
   if (!bson_iter_init_find (&id_iter, document, "_id")) {
      bson_t tmp = BSON_INITIALIZER;
      bson_oid_t oid;
      bson_oid_init (&oid, NULL);
      BSON_ASSERT (BSON_APPEND_OID (&tmp, "_id", &oid));
      BSON_ASSERT (bson_concat (&tmp, document));
      BSON_ASSERT (bson_append_document (&op, "document", 8, &tmp));
      bson_destroy (&tmp);
   } else {
      BSON_ASSERT (bson_append_document (&op, "document", 8, document));
   }

   // MONGOC_DEBUG ("appending op: %s", tmp_json (&op));

   BSON_ASSERT (
      _mongoc_buffer_append (&self->ops, bson_get_data (&op), op.len));

   // Store an iterator to the document's `_id` in the persisted payload:
   {
      BSON_ASSERT (bson_in_range_size_t_unsigned (op.len));
      size_t start = self->ops.len - (size_t) op.len;
      bson_t doc_view;
      BSON_ASSERT (
         bson_init_static (&doc_view, self->ops.data + start, (size_t) op.len));
      BSON_ASSERT (bson_iter_init (&id_iter, &doc_view));
      BSON_ASSERT (
         bson_iter_find_descendant (&id_iter, "document._id", &id_iter));
   }

   self->n_ops++;
   inserted_ids_entry iie = {.is_insert = true, .id_iter = id_iter};
   _mongoc_array_append_val (&self->inserted_ids, iie);
   return true;
}

typedef struct _mongoc_bulkwriteresult_t {
   struct {
      bool isset;
      int64_t value;
   } insertedcount;
   // `inserted_ids` is an array of `inserted_ids_entry`.
   // The array is sized to the number of operations.
   // If the operation was an insert, the `id` is stored.
   mongoc_array_t inserted_ids;
} mongoc_bulkwriteresult_t;

static mongoc_bulkwriteresult_t *
mongoc_bulkwriteresult_new (void)
{
   mongoc_bulkwriteresult_t *self = bson_malloc0 (sizeof (*self));
   return self;
}

static void
mongoc_bulkwriteresult_destroy (mongoc_bulkwriteresult_t *self)
{
   if (!self) {
      return;
   }
   _mongoc_array_destroy (&self->inserted_ids);
   bson_free (self);
}

bool
mongoc_bulkwriteresult_has_insertedcount (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->insertedcount.isset;
}

int64_t
mongoc_bulkwriteresult_get_insertedcount (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (self->insertedcount.isset);
   return self->insertedcount.value;
}

typedef inserted_ids_entry mongoc_insertoneresult_t;

const bson_value_t *
mongoc_insertoneresult_get_insertedid (const mongoc_insertoneresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   // Remove the const. `bson_iter_value` does not modify the argument.
   bson_iter_t *id_iter = (bson_iter_t *) &self->id_iter;
   return bson_iter_value (id_iter);
}

const mongoc_insertoneresult_t *
mongoc_bulkwriteresult_get_insertoneresult (
   const mongoc_bulkwriteresult_t *self, size_t index)
{
   BSON_ASSERT_PARAM (self);
   if (index >= self->inserted_ids.len) {
      return NULL;
   }
   inserted_ids_entry *iie =
      &_mongoc_array_index (&self->inserted_ids, inserted_ids_entry, index);
   if (!iie->is_insert) {
      return NULL;
   }
   if (iie->has_write_error) {
      return NULL;
   }
   return iie;
}

typedef struct _mongoc_write_error_t {
   int32_t code;
   bson_t *details;
   char *message;
} mongoc_write_error_t;

static mongoc_write_error_t *
mongoc_write_error_new (void)
{
   return bson_malloc0 (sizeof (mongoc_write_error_t));
}

static void
mongoc_write_error_destroy (mongoc_write_error_t *self)
{
   if (!self) {
      return;
   }
   bson_destroy (self->details);
   bson_free (self->message);
   bson_free (self);
}

int32_t
mongoc_write_error_get_code (const mongoc_write_error_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->code;
}

const char *
mongoc_write_error_get_message (const mongoc_write_error_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->message;
}

const bson_t *
mongoc_write_error_get_details (const mongoc_write_error_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->details;
}

typedef struct _mongoc_bulkwriteexception_t {
   struct {
      bson_error_t error;
      bson_t document;
      bool isset;
   } optional_error;
   // `write_errors` is an array of `mongoc_write_error_t*`.
   // The array is sized to the number of operations.
   mongoc_array_t write_errors;
   // If `has_any_error` is false, the bulk write exception is not returned.
   bool has_any_error;
} mongoc_bulkwriteexception_t;

bool
mongoc_bulkwriteexception_has_error (const mongoc_bulkwriteexception_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->optional_error.isset;
}

void
mongoc_bulkwriteexception_get_error (const mongoc_bulkwriteexception_t *self,
                                     bson_error_t *error,
                                     const bson_t **error_document)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (self->optional_error.isset);
   memcpy (error, &self->optional_error.error, sizeof (*error));
   *error_document = &self->optional_error.document;
}

const mongoc_write_error_t *
mongoc_bulkwriteexception_get_writeerror (
   const mongoc_bulkwriteexception_t *self, size_t index)
{
   BSON_ASSERT_PARAM (self);

   if (index > self->write_errors.len) {
      return NULL;
   }

   return _mongoc_array_index (
      &self->write_errors, mongoc_write_error_t *, index);
}

static mongoc_bulkwriteexception_t *
mongoc_bulkwriteexception_new_from_error (bson_error_t *error,
                                          const bson_t *error_document)
{
   BSON_ASSERT_PARAM (error);

   mongoc_bulkwriteexception_t *self = bson_malloc0 (sizeof (*self));
   memcpy (&self->optional_error.error, error, sizeof (*error));
   if (error_document) {
      bson_copy_to (error_document, &self->optional_error.document);
   } else {
      // Initialize an empty document.
      bson_init (&self->optional_error.document);
   }
   self->optional_error.isset = true;
   return self;
}

static void
mongoc_bulkwriteexception_set_error (mongoc_bulkwriteexception_t *self,
                                     bson_error_t *error,
                                     const bson_t *error_document)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (error);

   memcpy (&self->optional_error.error, error, sizeof (*error));
   if (self->optional_error.isset) {
      bson_destroy (&self->optional_error.document);
   }
   if (error_document) {
      bson_copy_to (error_document, &self->optional_error.document);
   } else {
      // Initialize an empty document.
      bson_init (&self->optional_error.document);
   }
   self->optional_error.isset = true;
   self->has_any_error = true;
}

static void
mongoc_bulkwriteexception_set_writeerror (mongoc_bulkwriteexception_t *self,
                                          mongoc_write_error_t *we,
                                          size_t idx)
{
   mongoc_write_error_t **we_loc =
      &_mongoc_array_index (&self->write_errors, mongoc_write_error_t *, idx);
   *we_loc = we;
   self->has_any_error = true;
}

static mongoc_bulkwriteexception_t *
mongoc_bulkwriteexception_new (size_t nmodels)
{
   mongoc_bulkwriteexception_t *self = bson_malloc0 (sizeof (*self));
   bson_init (&self->optional_error.document);
   _mongoc_array_init (&self->write_errors, sizeof (mongoc_write_error_t *));
   _mongoc_array_resize (&self->write_errors, nmodels);
   memset (self->write_errors.data, 0, self->write_errors.allocated);

   self->has_any_error = false;
   return self;
}


static void
mongoc_bulkwriteexception_destroy (mongoc_bulkwriteexception_t *self)
{
   if (!self) {
      return;
   }

   bson_destroy (&self->optional_error.document);

   // Destroy all write errors entries.
   for (size_t i = 0; i < self->write_errors.len; i++) {
      mongoc_write_error_t *entry =
         _mongoc_array_index (&self->write_errors, mongoc_write_error_t *, i);
      mongoc_write_error_destroy (entry);
   }
   _mongoc_array_destroy (&self->write_errors);
   bson_free (self);
}

typedef struct mongoc_bulkwritereturn_t {
   mongoc_bulkwriteexception_t *exc;
   mongoc_bulkwriteresult_t *res;
} mongoc_bulkwritereturn_t;

void
mongoc_bulkwritereturn_cleanup (mongoc_bulkwritereturn_t *self)
{
   if (!self) {
      return;
   }
   mongoc_bulkwriteresult_destroy (self->res);
   mongoc_bulkwriteexception_destroy (self->exc);
}

typedef struct _mongoc_bulkwriteopts_t {
   int placeholder;
} mongoc_bulkwriteopts_t;

int32_t _mock_maxWriteBatchSize = 0;
int32_t _mock_maxMessageSizeBytes = 0;

mongoc_bulkwritereturn_t
mongoc_client_bulkwrite (mongoc_client_t *self,
                         mongoc_listof_bulkwritemodel_t *models,
                         mongoc_bulkwriteopts_t *opts)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (models);

   mongoc_bulkwritereturn_t ret = {0};
   mongoc_server_stream_t *ss = NULL;
   bson_t cmd = BSON_INITIALIZER;
   mongoc_cmd_parts_t parts = {0};

   // Select a stream.
   {
      bson_t reply;
      bson_error_t error;
      ss = mongoc_cluster_stream_for_writes (&self->cluster,
                                             NULL /* session */,
                                             NULL /* deprioritized servers */,
                                             &reply,
                                             &error);
      if (!ss) {
         ret.exc = mongoc_bulkwriteexception_new_from_error (&error, &reply);
         bson_destroy (&reply);
         goto fail;
      }
   }

   // Create the payload 0.
   {
      BSON_ASSERT (bson_append_int32 (&cmd, "bulkWrite", 9, 1));
      // Append 'nsInfo' array.
      bson_array_builder_t *nsInfo;
      BSON_ASSERT (
         bson_append_array_builder_begin (&cmd, "nsInfo", 6, &nsInfo));
      bson_iter_t ns_iter;
      BSON_ASSERT (bson_iter_init (&ns_iter, &models->ns_to_index));
      while (bson_iter_next (&ns_iter)) {
         bson_t nsInfo_element;
         BSON_ASSERT (
            bson_array_builder_append_document_begin (nsInfo, &nsInfo_element));
         BSON_ASSERT (bson_append_utf8 (&nsInfo_element,
                                        "ns",
                                        2,
                                        bson_iter_key (&ns_iter),
                                        bson_iter_key_len (&ns_iter)));
         BSON_ASSERT (
            bson_array_builder_append_document_end (nsInfo, &nsInfo_element));
      }
      BSON_ASSERT (bson_append_array_builder_end (&cmd, nsInfo));

      mongoc_cmd_parts_init (&parts, self, "admin", MONGOC_QUERY_NONE, &cmd);
      bson_error_t error;
      if (!mongoc_cmd_parts_assemble (&parts, ss, &error)) {
         mongoc_bulkwriteexception_set_error (ret.exc, &error, NULL);
         goto fail;
      }
   }

   // MONGOC_DEBUG ("created command: %s", tmp_json (&cmd));

   // Create one or more batches.
   int32_t maxWriteBatchSize = mongoc_server_stream_max_write_batch_size (ss);
   if (_mock_maxWriteBatchSize > 0) {
      maxWriteBatchSize = _mock_maxWriteBatchSize;
   }
   int32_t maxMessageSizeBytes = mongoc_server_stream_max_msg_size (ss);
   if (_mock_maxMessageSizeBytes > 0) {
      maxMessageSizeBytes = _mock_maxMessageSizeBytes;
   }

   // Create empty result and exception to collect results/errors from batches.
   ret.res = mongoc_bulkwriteresult_new ();
   // Copy `inserted_ids` to the result.
   _mongoc_array_copy (&ret.res->inserted_ids, &models->inserted_ids);
   ret.exc = mongoc_bulkwriteexception_new (models->n_ops);

   size_t writeBatchSize_offset = 0;
   size_t payload_offset = 0;
   while (true) {
      bool batch_ok = false;
      bson_t cmd_reply = BSON_INITIALIZER;
      mongoc_cursor_t *reply_cursor = NULL;
      size_t payload_len = 0;
      size_t payload_writeBatchSize = 0;

      if (payload_offset == models->ops.len) {
         // All write models were sent.
         break;
      }

      // Read as many documents from payload as possible.
      while (true) {
         if (payload_offset + payload_len >= models->ops.len) {
            // All remaining ops are readied.
            break;
         }

         if (payload_writeBatchSize >= maxWriteBatchSize) {
            // Maximum number of operations are readied.
            break;
         }

         // Read length of next document.
         uint32_t ulen;
         memcpy (&ulen, models->ops.data + payload_offset + payload_len, 4);
         ulen = BSON_UINT32_FROM_LE (ulen);

         /*
          * OP_MSG header == 16 byte
          * + 4 bytes flagBits
          * + 1 byte payload type = 0
          * + 1 byte payload type = 1
          * + 4 byte size of payload
          * == 26 bytes opcode overhead
          * + X Full command document {insert: "test", writeConcern: {...}}
          * + Y command identifier ("documents", "deletes", "updates") ( + \0)
          */
         size_t overhead =
            26 + parts.assembled.command->len + strlen ("ops") + 1;
         // MONGOC_DEBUG ("overhead=%zu", overhead);
         if (overhead + payload_len + ulen > maxMessageSizeBytes) {
            if (payload_len == 0) {
               // Could not even fit one document within an OP_MSG.
               bson_error_t error;
               bson_set_error (&error,
                               MONGOC_ERROR_COMMAND,
                               MONGOC_ERROR_COMMAND_INVALID_ARG,
                               "unable to send document at index %zu. Sending "
                               "would exceed maxMessageSizeBytes=%" PRId32,
                               payload_writeBatchSize,
                               maxMessageSizeBytes);
               mongoc_bulkwriteexception_set_error (
                  ret.exc, &error, &cmd_reply);
               goto batch_fail;
            }
            break;
         }
         payload_len += ulen;
         payload_writeBatchSize += 1;
      }

      // Send batch.
      {
         // MONGOC_DEBUG ("Sending batch: payload_offset=%zu, payload_len=%zu",
         //               payload_offset,
         //               payload_len);

         // Create the payload 1 and send.
         {
            bson_error_t error;
            parts.assembled.payload_identifier = "ops";
            parts.assembled.payload = models->ops.data + payload_offset;
            BSON_ASSERT (bson_in_range_int32_t_unsigned (payload_len));
            parts.assembled.payload_size = (int32_t) payload_len;

            bool ok = mongoc_cluster_run_command_monitored (
               &self->cluster, &parts.assembled, &cmd_reply, &error);
            if (!ok) {
               mongoc_bulkwriteexception_set_error (ret.exc, &error, NULL);
               goto batch_fail;
            }
            // MONGOC_DEBUG ("got reply: %s\n", tmp_json (&cmd_reply));
         }

         // Add to result and/or exception.
         {
            // Check for insertedCount.
            bson_iter_t iter;

            // Server returns an int32.
            if (bson_iter_init_find (&iter, &cmd_reply, "nInserted") &&
                BSON_ITER_HOLDS_INT32 (&iter)) {
               ret.res->insertedcount.isset = true;
               ret.res->insertedcount.value +=
                  (int64_t) bson_iter_int32 (&iter);
            } else {
               bson_error_t error;
               bson_set_error (
                  &error,
                  MONGOC_ERROR_COMMAND,
                  MONGOC_ERROR_COMMAND_INVALID_ARG,
                  "expected to find int32 `nInserted`, but did not");
               mongoc_bulkwriteexception_set_error (
                  ret.exc, &error, &cmd_reply);
               goto batch_fail;
            }

            int32_t nerrors = 0;
            if (bson_iter_init_find (&iter, &cmd_reply, "nErrors") &&
                BSON_ITER_HOLDS_INT32 (&iter)) {
               nerrors = bson_iter_int32 (&iter);
            } else {
               bson_error_t error;
               bson_set_error (&error,
                               MONGOC_ERROR_COMMAND,
                               MONGOC_ERROR_COMMAND_INVALID_ARG,
                               "expected to find int32 `nErrors`, but did not");
               mongoc_bulkwriteexception_set_error (
                  ret.exc, &error, &cmd_reply);
               goto batch_fail;
            }

            if (nerrors > 0) {
               // Construct the reply cursor.
               reply_cursor = mongoc_cursor_new_from_command_reply_with_opts (
                  self, &cmd_reply, NULL);
               // `cmd_reply` is stolen. Clear it.
               bson_init (&cmd_reply);

               // Ensure constructing cursor did not error.
               {
                  bson_error_t error;
                  const bson_t *error_document;
                  if (mongoc_cursor_error_document (
                         reply_cursor, &error, &error_document)) {
                     mongoc_bulkwriteexception_set_error (
                        ret.exc, &error, error_document);
                     goto batch_fail;
                  }
               }

               // Iterate, and search for write errors.
               const bson_t *result;
               while (mongoc_cursor_next (reply_cursor, &result)) {
                  // Parse for `ok`.
                  double ok;
                  {
                     bson_iter_t result_iter;
                     // The server BulkWriteReplyItem represents `ok` as double.
                     if (!bson_iter_init_find (&result_iter, result, "ok") ||
                         !BSON_ITER_HOLDS_DOUBLE (&result_iter)) {
                        bson_error_t error;
                        bson_set_error (&error,
                                        MONGOC_ERROR_COMMAND,
                                        MONGOC_ERROR_COMMAND_INVALID_ARG,
                                        "expected to find double `ok` in "
                                        "result, but did not");
                        mongoc_bulkwriteexception_set_error (
                           ret.exc, &error, result);
                        goto batch_fail;
                     }
                     ok = bson_iter_double (&result_iter);
                  }

                  // Parse `idx`.
                  int32_t idx;
                  {
                     bson_iter_t result_iter;
                     // The server BulkWriteReplyItem represents `index` as
                     // int32.
                     if (!bson_iter_init_find (&result_iter, result, "idx") ||
                         !BSON_ITER_HOLDS_INT32 (&result_iter) ||
                         bson_iter_int32 (&result_iter) < 0) {
                        bson_error_t error;
                        bson_set_error (
                           &error,
                           MONGOC_ERROR_COMMAND,
                           MONGOC_ERROR_COMMAND_INVALID_ARG,
                           "expected to find non-negative int32 `idx` in "
                           "result, but did not");
                        mongoc_bulkwriteexception_set_error (
                           ret.exc, &error, result);
                        goto batch_fail;
                     }
                     idx = bson_iter_int32 (&result_iter);
                  }


                  BSON_ASSERT (bson_in_range_size_t_signed (idx));
                  // `models_idx` is the index of the model that produced this
                  // result.
                  size_t models_idx = (size_t) idx + writeBatchSize_offset;
                  if (ok == 0) {
                     bson_iter_t result_iter;

                     // Parse `code`.
                     int32_t code;
                     {
                        if (!bson_iter_init_find (
                               &result_iter, result, "code") ||
                            !BSON_ITER_HOLDS_INT32 (&result_iter)) {
                           bson_error_t error;
                           bson_set_error (&error,
                                           MONGOC_ERROR_COMMAND,
                                           MONGOC_ERROR_COMMAND_INVALID_ARG,
                                           "expected to find int32 `code` in "
                                           "result, but did not");
                           mongoc_bulkwriteexception_set_error (
                              ret.exc, &error, result);
                           goto batch_fail;
                        }
                        code = bson_iter_int32 (&result_iter);
                     }

                     // Parse `errmsg`.
                     const char *errmsg;
                     {
                        if (!bson_iter_init_find (
                               &result_iter, result, "errmsg") ||
                            !BSON_ITER_HOLDS_UTF8 (&result_iter)) {
                           bson_error_t error;
                           bson_set_error (&error,
                                           MONGOC_ERROR_COMMAND,
                                           MONGOC_ERROR_COMMAND_INVALID_ARG,
                                           "expected to find utf8 `errmsg` in "
                                           "result, but did not");
                           mongoc_bulkwriteexception_set_error (
                              ret.exc, &error, result);
                           goto batch_fail;
                        }
                        errmsg = bson_iter_utf8 (&result_iter, NULL);
                     }

                     // Parse optional `errInfo`.
                     bson_t errInfo = BSON_INITIALIZER;
                     if (bson_iter_init_find (
                            &result_iter, result, "errInfo")) {
                        bson_error_t error;
                        if (!_mongoc_iter_document_as_bson (
                               &result_iter, &errInfo, &error)) {
                           mongoc_bulkwriteexception_set_error (
                              ret.exc, &error, result);
                        }
                     }

                     // Store a copy of the write error.
                     mongoc_write_error_t *we = mongoc_write_error_new ();
                     we->code = code;
                     we->message = bson_strdup (errmsg);
                     we->details = bson_copy (&errInfo);

                     mongoc_bulkwriteexception_set_writeerror (
                        ret.exc, we, models_idx);

                     // Mark in the insert so the insert IDs are not reported.
                     inserted_ids_entry *iie = &_mongoc_array_index (
                        &ret.res->inserted_ids, inserted_ids_entry, models_idx);
                     iie->has_write_error = true;
                  }
               }
               // Ensure iterating cursor did not error.
               {
                  bson_error_t error;
                  const bson_t *error_document;
                  if (mongoc_cursor_error_document (
                         reply_cursor, &error, &error_document)) {
                     mongoc_bulkwriteexception_set_error (
                        ret.exc, &error, result);
                     goto batch_fail;
                  }
               }
            }
         }
      }

      writeBatchSize_offset += payload_writeBatchSize;
      payload_offset += payload_len;
      batch_ok = true;
   batch_fail:
      mongoc_cursor_destroy (reply_cursor);
      bson_destroy (&cmd_reply);
      if (!batch_ok) {
         goto fail;
      }
   }

fail:
   mongoc_cmd_parts_cleanup (&parts);
   bson_destroy (&cmd);
   mongoc_server_stream_cleanup (ss);
   if (ret.exc && !ret.exc->has_any_error) {
      mongoc_bulkwriteexception_destroy (ret.exc);
      ret.exc = NULL;
   }
   return ret;
}
