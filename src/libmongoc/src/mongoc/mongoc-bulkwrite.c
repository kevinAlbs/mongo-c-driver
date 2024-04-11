#include <mongoc-bulkwrite.h>
#include <mongoc-buffer-private.h>
#include <mongoc-array-private.h>
#include <mongoc-error.h>
#include <mongoc-server-stream-private.h>
#include <mongoc-cmd-private.h>
#include <mongoc-client-private.h>
#include <mongoc-util-private.h>  // _mongoc_iter_document_as_bson
#include <mongoc-error-private.h> // _mongoc_write_error_handle_labels

int32_t _mock_maxWriteBatchSize = 0;
int32_t _mock_maxMessageSizeBytes = 0;

struct _mongoc_insertoneresult_t {
   bool is_insert;
   bson_iter_t id_iter;  // Iterator to the `_id` field.
   bool has_write_error; // True if insert was attempted but failed.
};

struct _mongoc_listof_bulkwritemodel_t {
   // `ops` is a document sequence.
   mongoc_buffer_t ops;
   size_t n_ops;
   // `ns_to_index` maps a namespace to an index.
   bson_t ns_to_index;
   // `entries` is an array of `mongoc_insertoneresult_t` sized to the number of
   // operations. If the operation was an insert, the `id` is stored.
   mongoc_array_t entries;
   // `updates` is an array of bools sized to the number of operations. If the
   // operation was an update, the value is true.
   mongoc_array_t updates;
   // `deletes` is an array of bools sized to the number of operations. If the
   // operation was a delete, the value is true.
   mongoc_array_t deletes;
   // TODO: Consider combining `entries`, `updates`, and `deletes` into a
   // `verbose_results` array to contain:
   // - Iterators to the `_id` for inserts
   // - Identifier of the operation (to construct results)
   bool has_multi_write;
   // `max_insert_len` tracks the maximum length of any document to-be inserted.
   uint32_t max_insert_len;
};

struct _mongoc_mapof_insertoneresult_t {
   // `entries` is an array of `mongoc_insertoneresult_t`.
   // The array is sized to the number of operations.
   // If the operation was an insert, the `id` is stored.
   mongoc_array_t entries;
};

struct _mongoc_mapof_updateresult_t {
   // `entries` is an array of `mongoc_updateresult_t`.
   // The array is sized to the number of operations.
   mongoc_array_t entries;
};

struct _mongoc_mapof_deleteresult_t {
   // `entries` is an array of `mongoc_deleteresult_t`.
   // The array is sized to the number of operations.
   mongoc_array_t entries;
};

struct _mongoc_bulkwriteresult_t {
   int64_t insertedcount;
   int64_t matchedcount;
   int64_t modifiedcount;
   int64_t deletedcount;
   int64_t upsertedcount;
   mongoc_mapof_insertoneresult_t mapof_ior;
   mongoc_mapof_updateresult_t mapof_ur;
   mongoc_mapof_deleteresult_t mapof_dr;
};

struct _mongoc_updateresult_t {
   bool is_update;
   /**
    * The number of documents that matched the filter.
    */
   int64_t matchedCount;

   /**
    * The number of documents that were modified.
    */
   int64_t modifiedCount;

   /**
    * The _id field of the upserted document if an upsert occurred.
    *
    * It MUST be possible to discern between a BSON Null upserted ID value and
    * this field being unset. If necessary, drivers MAY add a didUpsert boolean
    * field to differentiate between these two cases.
    */
   bson_value_t upsertedId;
   bool didUpsert;
};

struct _mongoc_deleteresult_t {
   bool is_delete;
   /**
    * The number of documents that were deleted.
    */
   int64_t deletedCount;

   bool succeeded;
};

struct _mongoc_mapof_writeerror_t {
   // `entries` is an array of `mongoc_write_error_t*`.
   // The array is sized to the number of operations.
   mongoc_array_t entries;
};

struct _mongoc_listof_writeconcernerror_t {
   // `entries` is an array of `mongoc_writeconcernerror_t`.
   mongoc_array_t entries;
};

struct _mongoc_listof_errorlabel_t {
   // `entries` is an array of `char*`.
   mongoc_array_t entries;
};


struct _mongoc_bulkwriteexception_t {
   struct {
      bson_error_t error;
      bson_t document;
      bool isset;
   } optional_error;

   mongoc_mapof_writeerror_t mapof_we;
   mongoc_listof_writeconcernerror_t listof_wce;
   mongoc_listof_errorlabel_t listof_el;

   // If `has_any_error` is false, the bulk write exception is not returned.
   bool has_any_error;
   bson_t *error_reply;
};


const mongoc_listof_errorlabel_t *
mongoc_bulkwriteexception_errorLabels (const mongoc_bulkwriteexception_t *self)
{
   BSON_ASSERT_PARAM (self);
   return &self->listof_el;
}

const char *
mongoc_listof_errorlabel_at (const mongoc_listof_errorlabel_t *self, size_t idx)
{
   BSON_ASSERT_PARAM (self);
   if (idx > self->entries.len) {
      return NULL;
   }
   return _mongoc_array_index (&self->entries, char *, idx);
}

size_t
mongoc_listof_errorlabel_len (const mongoc_listof_errorlabel_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->entries.len;
}

// `mongoc_listof_errorlabel_upsert` checks `reply` for the `errorLabels`
// fields, and upserts into the list.
static void
mongoc_listof_errorlabel_upsert (mongoc_listof_errorlabel_t *self, const bson_t *reply)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (reply);

   bson_iter_t iter, error_labels;
   if (bson_iter_init_find (&iter, reply, "errorLabels") && bson_iter_recurse (&iter, &error_labels)) {
      while (bson_iter_next (&error_labels)) {
         const char *to_upsert = bson_iter_utf8 (&error_labels, NULL);
         if (!to_upsert) {
            MONGOC_ERROR ("Skipping unexpected non-UTF8 error label.");
            continue;
         }
         // Check if label already present.
         for (size_t i = 0; i < self->entries.len; i++) {
            const char *existing = _mongoc_array_index (&self->entries, char *, i);
            if (0 == strcmp (existing, to_upsert)) {
               // Already present, ignore it.
               continue;
            }
         }
         // Not present. Insert a copy.
         char *to_upsert_copy = bson_strdup (to_upsert);
         _mongoc_array_append_val (&self->entries, to_upsert_copy);
      }
   }
}


struct _mongoc_writeerror_t {
   int32_t code;
   bson_t *details;
   char *message;
};


static mongoc_writeerror_t *
mongoc_writeerror_new (void)
{
   return bson_malloc0 (sizeof (mongoc_writeerror_t));
}

static void
mongoc_writeerror_destroy (mongoc_writeerror_t *self)
{
   if (!self) {
      return;
   }
   bson_destroy (self->details);
   bson_free (self->message);
   bson_free (self);
}

struct _mongoc_writeconcernerror_t {
   int32_t code;
   bson_t *details;
   char *message;
};

static mongoc_writeconcernerror_t *
mongoc_writeconcernerror_new (void)
{
   return bson_malloc0 (sizeof (mongoc_writeconcernerror_t));
}

static void
mongoc_writeconcernerror_destroy (mongoc_writeconcernerror_t *self)
{
   if (!self) {
      return;
   }
   bson_destroy (self->details);
   bson_free (self->message);
   bson_free (self);
}

static mongoc_bulkwriteexception_t *
mongoc_bulkwriteexception_new (size_t nmodels)
{
   mongoc_bulkwriteexception_t *self = bson_malloc0 (sizeof (*self));
   bson_init (&self->optional_error.document);
   size_t nentries = nmodels;
   if (nentries == 0) {
      // Initialize with 1 to avoid assert.
      nentries = 1;
   }
   _mongoc_array_init_with_zerofill (&self->mapof_we.entries, sizeof (mongoc_writeerror_t *), nentries);
   _mongoc_array_init (&self->listof_wce.entries, sizeof (mongoc_writeconcernerror_t *));
   _mongoc_array_init (&self->listof_el.entries, sizeof (char *));
   self->has_any_error = false;
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
   if (error_document) {
      mongoc_listof_errorlabel_upsert (&self->listof_el, error_document);
   }
}

static void
mongoc_bulkwriteexception_set_writeerror (mongoc_bulkwriteexception_t *self, mongoc_writeerror_t *we, size_t idx)
{
   mongoc_writeerror_t **we_loc = &_mongoc_array_index (&self->mapof_we.entries, mongoc_writeerror_t *, idx);
   *we_loc = we;
   self->has_any_error = true;
}

static void
mongoc_bulkwriteexception_destroy (mongoc_bulkwriteexception_t *self)
{
   if (!self) {
      return;
   }

   bson_destroy (&self->optional_error.document);

   // Destroy all write errors entries.
   for (size_t i = 0; i < self->mapof_we.entries.len; i++) {
      mongoc_writeerror_t *entry = _mongoc_array_index (&self->mapof_we.entries, mongoc_writeerror_t *, i);
      mongoc_writeerror_destroy (entry);
   }
   _mongoc_array_destroy (&self->mapof_we.entries);

   // Destroy all write concern errors entries.
   for (size_t i = 0; i < self->listof_wce.entries.len; i++) {
      mongoc_writeconcernerror_t *entry =
         _mongoc_array_index (&self->listof_wce.entries, mongoc_writeconcernerror_t *, i);
      mongoc_writeconcernerror_destroy (entry);
   }
   _mongoc_array_destroy (&self->listof_wce.entries);

   // Destroy all error labels.
   for (size_t i = 0; i < self->listof_el.entries.len; i++) {
      char *entry = _mongoc_array_index (&self->listof_el.entries, char *, i);
      bson_free (entry);
   }
   _mongoc_array_destroy (&self->listof_el.entries);


   bson_destroy (self->error_reply);

   bson_free (self);
}

static mongoc_bulkwriteresult_t *
mongoc_bulkwriteresult_new (mongoc_listof_bulkwritemodel_t *models)
{
   mongoc_bulkwriteresult_t *self = bson_malloc0 (sizeof (*self));
   size_t nentries = models->n_ops;
   if (nentries == 0) {
      // Initialize with 1 to avoid assert.
      nentries = 1;
   }

   _mongoc_array_init_with_zerofill (&self->mapof_ur.entries, sizeof (mongoc_updateresult_t), nentries);

   for (size_t i = 0; i < models->updates.len; i++) {
      mongoc_updateresult_t *ur = &_mongoc_array_index (&self->mapof_ur.entries, mongoc_updateresult_t, i);
      ur->is_update = _mongoc_array_index (&models->updates, bool, i);
      // Set other fields later when parsing results.
   }

   _mongoc_array_init_with_zerofill (&self->mapof_dr.entries, sizeof (mongoc_deleteresult_t), nentries);

   for (size_t i = 0; i < models->deletes.len; i++) {
      mongoc_deleteresult_t *dr = &_mongoc_array_index (&self->mapof_dr.entries, mongoc_deleteresult_t, i);
      dr->is_delete = _mongoc_array_index (&models->deletes, bool, i);
      // Set other fields later when parsing results.
   }

   return self;
}

static void
mongoc_bulkwriteresult_destroy (mongoc_bulkwriteresult_t *self)
{
   if (!self) {
      return;
   }
   _mongoc_array_destroy (&self->mapof_ior.entries);
   for (size_t i = 0; i < self->mapof_ur.entries.len; i++) {
      mongoc_updateresult_t ur = _mongoc_array_index (&self->mapof_ur.entries, mongoc_updateresult_t, i);
      if (ur.is_update) {
         if (ur.didUpsert) {
            bson_value_destroy (&ur.upsertedId);
         }
      }
   }
   _mongoc_array_destroy (&self->mapof_ur.entries);
   _mongoc_array_destroy (&self->mapof_dr.entries);
   bson_free (self);
}


mongoc_bulkwritereturn_t
mongoc_client_bulkwrite (mongoc_client_t *self,
                         mongoc_listof_bulkwritemodel_t *models,
                         mongoc_bulkwriteoptions_t *options // may be NULL
)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (models);

   mongoc_bulkwritereturn_t ret = {0};
   mongoc_server_stream_t *ss = NULL;
   bson_t cmd = BSON_INITIALIZER;
   mongoc_cmd_parts_t parts = {0};
   mongoc_bulkwriteoptions_t defaults = {0};
   mongoc_server_stream_t *retry_ss = NULL;
   if (!options) {
      options = &defaults;
   }

   // Create empty result and exception to collect results/errors from batches.
   ret.res = mongoc_bulkwriteresult_new (models);
   // Copy `entries` to the result.
   _mongoc_array_copy (&ret.res->mapof_ior.entries, &models->entries);
   ret.exc = mongoc_bulkwriteexception_new (models->n_ops);

   if (models->n_ops == 0) {
      bson_error_t error;
      bson_set_error (
         &error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "cannot do `bulkWrite` with no models");
      mongoc_bulkwriteexception_set_error (ret.exc, &error, NULL);
      goto fail;
   }

   // Select a stream.
   {
      bson_t reply;
      bson_error_t error;
      ss = mongoc_cluster_stream_for_writes (
         &self->cluster, NULL /* session */, NULL /* deprioritized servers */, &reply, &error);
      if (!ss) {
         mongoc_bulkwriteexception_set_error (ret.exc, &error, &reply);
         bson_destroy (&reply);
         goto fail;
      }
   }

   int32_t maxBsonObjectSize = mongoc_server_stream_max_bson_obj_size (ss);

   // Create the payload 0.
   {
      BSON_ASSERT (bson_append_int32 (&cmd, "bulkWrite", 9, 1));
      // errorsOnly is default true. Set to false if verboseResults requested.
      BSON_ASSERT (bson_append_bool (&cmd, "errorsOnly", 10, (options->verboseResults) ? false : true));
      // ordered is default true.
      BSON_ASSERT (bson_append_bool (&cmd, "ordered", 7, (options->ordered.isset) ? options->ordered.value : true));

      if (options->comment) {
         BSON_ASSERT (bson_append_document (&cmd, "comment", 7, options->comment));
      }

      if (options->bypassDocumentValidation.isset) {
         BSON_ASSERT (bson_append_bool (&cmd, "bypassDocumentValidation", 24, options->bypassDocumentValidation.value));
      }

      if (options->let) {
         BSON_ASSERT (bson_append_document (&cmd, "let", 3, options->let));
      }

      // Append 'nsInfo' array.
      bson_array_builder_t *nsInfo;
      BSON_ASSERT (bson_append_array_builder_begin (&cmd, "nsInfo", 6, &nsInfo));
      bson_iter_t ns_iter;
      BSON_ASSERT (bson_iter_init (&ns_iter, &models->ns_to_index));
      while (bson_iter_next (&ns_iter)) {
         bson_t nsInfo_element;
         BSON_ASSERT (bson_array_builder_append_document_begin (nsInfo, &nsInfo_element));
         BSON_ASSERT (
            bson_append_utf8 (&nsInfo_element, "ns", 2, bson_iter_key (&ns_iter), bson_iter_key_len (&ns_iter)));
         BSON_ASSERT (bson_array_builder_append_document_end (nsInfo, &nsInfo_element));
      }
      BSON_ASSERT (bson_append_array_builder_end (&cmd, nsInfo));

      mongoc_cmd_parts_init (&parts, self, "admin", MONGOC_QUERY_NONE, &cmd);
      bson_error_t error;

      parts.allow_txn_number = MONGOC_CMD_PARTS_ALLOW_TXN_NUMBER_YES; // To append `lsid`.
      if (models->has_multi_write) {
         // Write commands that include multi-document operations are not
         // retryable.
         parts.allow_txn_number = MONGOC_CMD_PARTS_ALLOW_TXN_NUMBER_NO;
      }
      parts.is_write_command = true; // To append `txnNumber`.

      if (options->session) {
         // TODO: do not set session if write is unacknowledged? (matches
         // existing behavior)
         mongoc_cmd_parts_set_session (&parts, options->session);
      }

      // Apply write concern:
      {
         const mongoc_write_concern_t *wc = self->write_concern; // Default to client.
         if (options->writeConcern) {
            wc = options->writeConcern;
         }
         if (!mongoc_cmd_parts_set_write_concern (&parts, wc, &error)) {
            mongoc_bulkwriteexception_set_error (ret.exc, &error, NULL);
            goto fail;
         }
         if (!mongoc_write_concern_is_acknowledged (wc) &&
             bson_cmp_greater_us (models->max_insert_len, maxBsonObjectSize)) {
            bson_set_error (&error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "Unacknowledged `bulkWrite` includes insert of size: %" PRIu32
                            ", exceeding maxBsonObjectSize: %" PRId32,
                            models->max_insert_len,
                            maxBsonObjectSize);
            mongoc_bulkwriteexception_set_error (ret.exc, &error, NULL);
            goto fail;
         }
      }

      if (!mongoc_cmd_parts_assemble (&parts, ss, &error)) {
         mongoc_bulkwriteexception_set_error (ret.exc, &error, NULL);
         goto fail;
      }
   }

   // Send one or more `bulkWrite` commands. Split input payload if necessary to
   // satisfy server size limits.
   int32_t maxWriteBatchSize = mongoc_server_stream_max_write_batch_size (ss);
   if (_mock_maxWriteBatchSize > 0) {
      maxWriteBatchSize = _mock_maxWriteBatchSize;
   }
   int32_t maxMessageSizeBytes = mongoc_server_stream_max_msg_size (ss);
   if (_mock_maxMessageSizeBytes > 0) {
      maxMessageSizeBytes = _mock_maxMessageSizeBytes;
   }

   size_t writeBatchSize_offset = 0;
   size_t payload_offset = 0;
   while (true) {
      bool has_write_errors = false;
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
          * + X Payload 0 document: {bulkWrite: 1, writeConcern: {...}}
          * + Y Payload 1 identifier: "ops" + \0
          */
         size_t overhead = 26 + parts.assembled.command->len + strlen ("ops") + 1;
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
               mongoc_bulkwriteexception_set_error (ret.exc, &error, &cmd_reply);
               goto batch_fail;
            }
            break;
         }
         payload_len += ulen;
         payload_writeBatchSize += 1;
      }

      // Send batch.
      {
         // Create the payload 1 and send.
         {
            bson_error_t error;
            parts.assembled.payload_identifier = "ops";
            parts.assembled.payload = models->ops.data + payload_offset;
            BSON_ASSERT (bson_in_range_int32_t_unsigned (payload_len));
            parts.assembled.payload_size = (int32_t) payload_len;

            bool is_retryable = parts.is_retryable_write;

            /* increment the transaction number for the first attempt of each
             * retryable write command */
            if (is_retryable) {
               bson_iter_t txn_number_iter;
               BSON_ASSERT (bson_iter_init_find (&txn_number_iter, parts.assembled.command, "txnNumber"));
               bson_iter_overwrite_int64 (&txn_number_iter, ++parts.assembled.session->server_session->txn_number);
            }

            // Send in possible retry.
         retry: {
            bool ok = mongoc_cluster_run_command_monitored (&self->cluster, &parts.assembled, &cmd_reply, &error);

            if (parts.is_retryable_write) {
               _mongoc_write_error_handle_labels (ok, &error, &cmd_reply, parts.assembled.server_stream->sd);
            }

            mongoc_write_err_type_t error_type = _mongoc_write_error_get_type (&cmd_reply);
            // Check for a retryable write error.
            if (error_type == MONGOC_WRITE_ERR_RETRY && is_retryable) {
               is_retryable = false; // Only retry once.
               bson_error_t ignored_error;

               // Select a server and create a stream again.
               retry_ss = mongoc_cluster_stream_for_writes (&self->cluster,
                                                            NULL /* session */,
                                                            NULL /* deprioritized servers */,
                                                            NULL /* reply */,
                                                            &ignored_error);

               if (retry_ss) {
                  parts.assembled.server_stream = retry_ss;
                  bson_destroy (&cmd_reply);
                  bson_init (&cmd_reply);
                  goto retry;
               }
            }

            // Check for a command ('ok': 0) error.
            if (!ok) {
               mongoc_bulkwriteexception_set_error (ret.exc, &error, &cmd_reply);
               ret.exc->error_reply = bson_copy (&cmd_reply);
               goto batch_fail;
            }
         }
         }

         // Add to result and/or exception.
         {
            bson_iter_t iter;

            if (bson_iter_init_find (&iter, &cmd_reply, "nInserted") && BSON_ITER_HOLDS_INT32 (&iter)) {
               ret.res->insertedcount += (int64_t) bson_iter_int32 (&iter);
            } else {
               bson_error_t error;
               bson_set_error (&error,
                               MONGOC_ERROR_COMMAND,
                               MONGOC_ERROR_COMMAND_INVALID_ARG,
                               "expected to find int32 `nInserted`, but did not");
               mongoc_bulkwriteexception_set_error (ret.exc, &error, &cmd_reply);
               goto batch_fail;
            }

            if (bson_iter_init_find (&iter, &cmd_reply, "nMatched") && BSON_ITER_HOLDS_INT32 (&iter)) {
               ret.res->matchedcount += (int64_t) bson_iter_int32 (&iter);
            } else {
               bson_error_t error;
               bson_set_error (&error,
                               MONGOC_ERROR_COMMAND,
                               MONGOC_ERROR_COMMAND_INVALID_ARG,
                               "expected to find int32 `nMatched`, but did not");
               mongoc_bulkwriteexception_set_error (ret.exc, &error, &cmd_reply);
               goto batch_fail;
            }

            if (bson_iter_init_find (&iter, &cmd_reply, "nModified") && BSON_ITER_HOLDS_INT32 (&iter)) {
               ret.res->modifiedcount += (int64_t) bson_iter_int32 (&iter);
            } else {
               bson_error_t error;
               bson_set_error (&error,
                               MONGOC_ERROR_COMMAND,
                               MONGOC_ERROR_COMMAND_INVALID_ARG,
                               "expected to find int32 `nModified`, but did not");
               mongoc_bulkwriteexception_set_error (ret.exc, &error, &cmd_reply);
               goto batch_fail;
            }

            if (bson_iter_init_find (&iter, &cmd_reply, "nDeleted") && BSON_ITER_HOLDS_INT32 (&iter)) {
               ret.res->deletedcount += (int64_t) bson_iter_int32 (&iter);
            } else {
               bson_error_t error;
               bson_set_error (&error,
                               MONGOC_ERROR_COMMAND,
                               MONGOC_ERROR_COMMAND_INVALID_ARG,
                               "expected to find int32 `nDeleted`, but did not");
               mongoc_bulkwriteexception_set_error (ret.exc, &error, &cmd_reply);
               goto batch_fail;
            }

            if (bson_iter_init_find (&iter, &cmd_reply, "nUpserted") && BSON_ITER_HOLDS_INT32 (&iter)) {
               ret.res->upsertedcount += (int64_t) bson_iter_int32 (&iter);
            } else {
               bson_error_t error;
               bson_set_error (&error,
                               MONGOC_ERROR_COMMAND,
                               MONGOC_ERROR_COMMAND_INVALID_ARG,
                               "expected to find int32 `nUpserted`, but did not");
               mongoc_bulkwriteexception_set_error (ret.exc, &error, &cmd_reply);
               goto batch_fail;
            }

            if (bson_iter_init_find (&iter, &cmd_reply, "writeConcernError")) {
               bson_iter_t wce_iter;
               bson_t wce_bson;

               {
                  bson_error_t error;
                  if (!_mongoc_iter_document_as_bson (&iter, &wce_bson, &error)) {
                     mongoc_bulkwriteexception_set_error (ret.exc, &error, &cmd_reply);
                     goto batch_fail;
                  }
               }

               // Parse `code`.
               int32_t code;
               {
                  if (!bson_iter_init_find (&wce_iter, &wce_bson, "code") || !BSON_ITER_HOLDS_INT32 (&wce_iter)) {
                     bson_error_t error;
                     bson_set_error (&error,
                                     MONGOC_ERROR_COMMAND,
                                     MONGOC_ERROR_COMMAND_INVALID_ARG,
                                     "expected to find int32 `code` in "
                                     "writeConcernError, but did not");
                     mongoc_bulkwriteexception_set_error (ret.exc, &error, &wce_bson);
                     goto batch_fail;
                  }
                  code = bson_iter_int32 (&wce_iter);
               }

               // Parse `errmsg`.
               const char *errmsg;
               {
                  if (!bson_iter_init_find (&wce_iter, &wce_bson, "errmsg") || !BSON_ITER_HOLDS_UTF8 (&wce_iter)) {
                     bson_error_t error;
                     bson_set_error (&error,
                                     MONGOC_ERROR_COMMAND,
                                     MONGOC_ERROR_COMMAND_INVALID_ARG,
                                     "expected to find utf8 `errmsg` in "
                                     "writeConcernError, but did not");
                     mongoc_bulkwriteexception_set_error (ret.exc, &error, &wce_bson);
                     goto batch_fail;
                  }
                  errmsg = bson_iter_utf8 (&wce_iter, NULL);
               }

               // Parse optional `errInfo`.
               bson_t errInfo = BSON_INITIALIZER;
               if (bson_iter_init_find (&wce_iter, &wce_bson, "errInfo")) {
                  bson_error_t error;
                  if (!_mongoc_iter_document_as_bson (&wce_iter, &errInfo, &error)) {
                     mongoc_bulkwriteexception_set_error (ret.exc, &error, &wce_bson);
                  }
               }

               // Store a copy of the write concern error.
               mongoc_writeconcernerror_t *wce = mongoc_writeconcernerror_new ();
               wce->code = code;
               wce->message = bson_strdup (errmsg);
               wce->details = bson_copy (&errInfo);

               _mongoc_array_append_val (&ret.exc->listof_wce.entries, wce);
               ret.exc->has_any_error = true;
            }

            {
               bson_t cursor_opts = BSON_INITIALIZER;
               {
                  bson_error_t error;
                  BSON_ASSERT (bson_append_int32 (&cursor_opts, "serverId", 8, parts.assembled.server_stream->sd->id));
                  // Use same session.
                  if (!mongoc_client_session_append (parts.assembled.session, &cursor_opts, &error)) {
                     mongoc_bulkwriteexception_set_error (ret.exc, &error, NULL);
                     goto batch_fail;
                  }
               }
               // Construct the reply cursor.
               reply_cursor = mongoc_cursor_new_from_command_reply_with_opts (self, &cmd_reply, &cursor_opts);
               bson_destroy (&cursor_opts);
               // `cmd_reply` is stolen. Clear it.
               bson_init (&cmd_reply);

               // Ensure constructing cursor did not error.
               {
                  bson_error_t error;
                  const bson_t *error_document;
                  if (mongoc_cursor_error_document (reply_cursor, &error, &error_document)) {
                     mongoc_bulkwriteexception_set_error (ret.exc, &error, error_document);
                     goto batch_fail;
                  }
               }

               // Iterate.
               const bson_t *result;
               while (mongoc_cursor_next (reply_cursor, &result)) {
                  // Parse for `ok`.
                  double ok;
                  {
                     bson_iter_t result_iter;
                     // The server BulkWriteReplyItem represents `ok` as double.
                     if (!bson_iter_init_find (&result_iter, result, "ok") || !BSON_ITER_HOLDS_DOUBLE (&result_iter)) {
                        bson_error_t error;
                        bson_set_error (&error,
                                        MONGOC_ERROR_COMMAND,
                                        MONGOC_ERROR_COMMAND_INVALID_ARG,
                                        "expected to find double `ok` in "
                                        "result, but did not");
                        mongoc_bulkwriteexception_set_error (ret.exc, &error, result);
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
                     if (!bson_iter_init_find (&result_iter, result, "idx") || !BSON_ITER_HOLDS_INT32 (&result_iter) ||
                         bson_iter_int32 (&result_iter) < 0) {
                        bson_error_t error;
                        bson_set_error (&error,
                                        MONGOC_ERROR_COMMAND,
                                        MONGOC_ERROR_COMMAND_INVALID_ARG,
                                        "expected to find non-negative int32 `idx` in "
                                        "result, but did not");
                        mongoc_bulkwriteexception_set_error (ret.exc, &error, result);
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
                     has_write_errors = true;

                     // Parse `code`.
                     int32_t code;
                     {
                        if (!bson_iter_init_find (&result_iter, result, "code") ||
                            !BSON_ITER_HOLDS_INT32 (&result_iter)) {
                           bson_error_t error;
                           bson_set_error (&error,
                                           MONGOC_ERROR_COMMAND,
                                           MONGOC_ERROR_COMMAND_INVALID_ARG,
                                           "expected to find int32 `code` in "
                                           "result, but did not");
                           mongoc_bulkwriteexception_set_error (ret.exc, &error, result);
                           goto batch_fail;
                        }
                        code = bson_iter_int32 (&result_iter);
                     }

                     // Parse `errmsg`.
                     const char *errmsg;
                     {
                        if (!bson_iter_init_find (&result_iter, result, "errmsg") ||
                            !BSON_ITER_HOLDS_UTF8 (&result_iter)) {
                           bson_error_t error;
                           bson_set_error (&error,
                                           MONGOC_ERROR_COMMAND,
                                           MONGOC_ERROR_COMMAND_INVALID_ARG,
                                           "expected to find utf8 `errmsg` in "
                                           "result, but did not");
                           mongoc_bulkwriteexception_set_error (ret.exc, &error, result);
                           goto batch_fail;
                        }
                        errmsg = bson_iter_utf8 (&result_iter, NULL);
                     }

                     // Parse optional `errInfo`.
                     bson_t errInfo = BSON_INITIALIZER;
                     if (bson_iter_init_find (&result_iter, result, "errInfo")) {
                        bson_error_t error;
                        if (!_mongoc_iter_document_as_bson (&result_iter, &errInfo, &error)) {
                           mongoc_bulkwriteexception_set_error (ret.exc, &error, result);
                        }
                     }

                     // Store a copy of the write error.
                     mongoc_writeerror_t *we = mongoc_writeerror_new ();
                     we->code = code;
                     we->message = bson_strdup (errmsg);
                     we->details = bson_copy (&errInfo);

                     mongoc_bulkwriteexception_set_writeerror (ret.exc, we, models_idx);

                     // Mark in the insert so the insert IDs are not reported.
                     mongoc_insertoneresult_t *ior =
                        &_mongoc_array_index (&ret.res->mapof_ior.entries, mongoc_insertoneresult_t, models_idx);
                     ior->has_write_error = true;
                  } else {
                     // This is a successful result of an individual operation.
                     // Server only reports successful results of individual
                     // operations when verbose results are requested
                     // (`errorsOnly: false` is sent).

                     // Check if model is an update.
                     {
                        mongoc_updateresult_t *ur =
                           &_mongoc_array_index (&ret.res->mapof_ur.entries, mongoc_updateresult_t, models_idx);
                        if (ur->is_update) {
                           bson_iter_t result_iter;
                           // Parse `n`.
                           int32_t n;
                           {
                              if (!bson_iter_init_find (&result_iter, result, "n") ||
                                  !BSON_ITER_HOLDS_INT32 (&result_iter)) {
                                 bson_error_t error;
                                 bson_set_error (&error,
                                                 MONGOC_ERROR_COMMAND,
                                                 MONGOC_ERROR_COMMAND_INVALID_ARG,
                                                 "expected to find int32 `n` in "
                                                 "result, but did not");
                                 mongoc_bulkwriteexception_set_error (ret.exc, &error, result);
                                 goto batch_fail;
                              }
                              n = bson_iter_int32 (&result_iter);
                           }

                           // Parse `nModified`.
                           int32_t nModified;
                           {
                              if (!bson_iter_init_find (&result_iter, result, "nModified") ||
                                  !BSON_ITER_HOLDS_INT32 (&result_iter)) {
                                 bson_error_t error;
                                 bson_set_error (&error,
                                                 MONGOC_ERROR_COMMAND,
                                                 MONGOC_ERROR_COMMAND_INVALID_ARG,
                                                 "expected to find int32 `nModified` in "
                                                 "result, but did not");
                                 mongoc_bulkwriteexception_set_error (ret.exc, &error, result);
                                 goto batch_fail;
                              }
                              nModified = bson_iter_int32 (&result_iter);
                           }

                           // Check for an optional `upsertId`.
                           if (bson_iter_init_find (&result_iter, result, "upserted")) {
                              bson_iter_t id_iter;
                              BSON_ASSERT (bson_iter_init (&result_iter, result));
                              if (!bson_iter_find_descendant (&result_iter, "upserted._id", &id_iter)) {
                                 bson_error_t error;
                                 bson_set_error (&error,
                                                 MONGOC_ERROR_COMMAND,
                                                 MONGOC_ERROR_COMMAND_INVALID_ARG,
                                                 "expected `upserted` to be a document "
                                                 "containing `_id`, but did not find `_id`");
                                 mongoc_bulkwriteexception_set_error (ret.exc, &error, result);
                                 goto batch_fail;
                              }
                              ur->didUpsert = true;
                              const bson_value_t *id_value = bson_iter_value (&id_iter);
                              bson_value_copy (id_value, &ur->upsertedId);
                           }


                           ur->matchedCount = n;
                           ur->modifiedCount = nModified;
                        }
                     }

                     // Check if model is a delete.
                     {
                        mongoc_deleteresult_t *dr =
                           &_mongoc_array_index (&ret.res->mapof_dr.entries, mongoc_deleteresult_t, models_idx);
                        if (dr->is_delete) {
                           bson_iter_t result_iter;
                           // Parse `n`.
                           int32_t n;
                           {
                              if (!bson_iter_init_find (&result_iter, result, "n") ||
                                  !BSON_ITER_HOLDS_INT32 (&result_iter)) {
                                 bson_error_t error;
                                 bson_set_error (&error,
                                                 MONGOC_ERROR_COMMAND,
                                                 MONGOC_ERROR_COMMAND_INVALID_ARG,
                                                 "expected to find int32 `n` in "
                                                 "result, but did not");
                                 mongoc_bulkwriteexception_set_error (ret.exc, &error, result);
                                 goto batch_fail;
                              }
                              n = bson_iter_int32 (&result_iter);
                           }

                           dr->deletedCount = n;
                           dr->succeeded = true;
                        }
                     }
                  }
               }
               // Ensure iterating cursor did not error.
               {
                  bson_error_t error;
                  const bson_t *error_document;
                  if (mongoc_cursor_error_document (reply_cursor, &error, &error_document)) {
                     mongoc_bulkwriteexception_set_error (ret.exc, &error, result);
                     ret.exc->error_reply = bson_copy (&cmd_reply);
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
      bool is_ordered = options->ordered.isset ? options->ordered.value : true; // default.
      if (has_write_errors && is_ordered) {
         // Ordered writes must not continue to send batches once an error is
         // occurred. An individual write error is not a top-level error.
         break;
      }
   }

fail:
   if (retry_ss) {
      mongoc_server_stream_cleanup (retry_ss);
   }
   if (parts.body) {
      // Only clean-up if initialized.
      mongoc_cmd_parts_cleanup (&parts);
   }
   bson_destroy (&cmd);
   mongoc_server_stream_cleanup (ss);
   if (ret.exc && !ret.exc->has_any_error) {
      mongoc_bulkwriteexception_destroy (ret.exc);
      ret.exc = NULL;
   }
   return ret;
}

const mongoc_insertoneresult_t *
mongoc_mapof_insertoneresult_lookup (const mongoc_mapof_insertoneresult_t *self, size_t idx)
{
   BSON_ASSERT_PARAM (self);
   if (idx >= self->entries.len) {
      return NULL;
   }
   mongoc_insertoneresult_t *ior = &_mongoc_array_index (&self->entries, mongoc_insertoneresult_t, idx);
   if (!ior->is_insert) {
      return NULL;
   }
   if (ior->has_write_error) {
      // TODO: do not return if operation did not return success.
      // If operation was not run (due to earlier error), `has_write_error` may
      // be false.
      return NULL;
   }
   return ior;
}

const bson_value_t *
mongoc_insertoneresult_inserted_id (const mongoc_insertoneresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   // Remove the const. `bson_iter_value` does not modify the argument.
   bson_iter_t *id_iter = (bson_iter_t *) &self->id_iter;
   return bson_iter_value (id_iter);
}

const mongoc_mapof_insertoneresult_t *
mongoc_bulkwriteresult_insertResults (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return &self->mapof_ior;
}

const mongoc_updateresult_t *
mongoc_mapof_updateresult_lookup (const mongoc_mapof_updateresult_t *self, size_t idx)
{
   BSON_ASSERT_PARAM (self);
   if (idx >= self->entries.len) {
      return NULL;
   }
   mongoc_updateresult_t *ur = &_mongoc_array_index (&self->entries, mongoc_updateresult_t, idx);
   if (!ur->is_update) {
      return NULL;
   }
   // TODO: do not return if operation did not return success.
   return ur;
}

int64_t
mongoc_updateresult_matchedCount (const mongoc_updateresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->matchedCount;
}

int64_t
mongoc_updateresult_modifiedCount (const mongoc_updateresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->modifiedCount;
}

const bson_value_t *
mongoc_updateresult_upsertedId (const mongoc_updateresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   if (self->didUpsert) {
      return &self->upsertedId;
   }
   return NULL;
}

const mongoc_mapof_updateresult_t *
mongoc_bulkwriteresult_updateResult (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return &self->mapof_ur;
}

const mongoc_deleteresult_t *
mongoc_mapof_deleteresult_lookup (const mongoc_mapof_deleteresult_t *self, size_t idx)
{
   BSON_ASSERT_PARAM (self);
   if (idx >= self->entries.len) {
      return NULL;
   }
   mongoc_deleteresult_t *dr = &_mongoc_array_index (&self->entries, mongoc_deleteresult_t, idx);
   if (!dr->is_delete) {
      return NULL;
   }
   if (!dr->succeeded) {
      return NULL;
   }
   return dr;
}

int64_t
mongoc_deleteresult_deletedCount (const mongoc_deleteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->deletedCount;
}

const mongoc_mapof_deleteresult_t *
mongoc_bulkwriteresult_deleteResults (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return &self->mapof_dr;
}


int64_t
mongoc_bulkwriteresult_insertedCount (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->insertedcount;
}

int64_t
mongoc_bulkwriteresult_upsertedCount (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->upsertedcount;
}

int64_t
mongoc_bulkwriteresult_matchedCount (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->matchedcount;
}

int64_t
mongoc_bulkwriteresult_modifiedCount (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->modifiedcount;
}

int64_t
mongoc_bulkwriteresult_deletedCount (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->deletedcount;
}

bool
mongoc_bulkwriteexception_error (const mongoc_bulkwriteexception_t *self,
                                 bson_error_t *error,
                                 const bson_t **error_document)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (error);
   // `error_document` is optional.

   if (self->optional_error.isset) {
      memcpy (error, &self->optional_error.error, sizeof (bson_error_t));
      if (error_document) {
         *error_document = &self->optional_error.document;
      }
      return true;
   }

   memset (error, 0, sizeof (*error));
   if (error_document) {
      *error_document = 0;
   }
   return false;
}

void
mongoc_bulkwritereturn_cleanup (mongoc_bulkwritereturn_t *self)
{
   if (!self) {
      return;
   }
   mongoc_bulkwriteresult_destroy (self->res);
   mongoc_bulkwriteexception_destroy (self->exc);
}

mongoc_listof_bulkwritemodel_t *
mongoc_listof_bulkwritemodel_new (void)
{
   mongoc_listof_bulkwritemodel_t *self = bson_malloc0 (sizeof (mongoc_listof_bulkwritemodel_t));
   _mongoc_buffer_init (&self->ops, NULL, 0, NULL, NULL);
   bson_init (&self->ns_to_index);
   _mongoc_array_init (&self->entries, sizeof (mongoc_insertoneresult_t));
   _mongoc_array_init (&self->updates, sizeof (bool));
   _mongoc_array_init (&self->deletes, sizeof (bool));
   return self;
}

void
mongoc_listof_bulkwritemodel_destroy (mongoc_listof_bulkwritemodel_t *self)
{
   if (!self) {
      return;
   }
   _mongoc_array_destroy (&self->entries);
   _mongoc_array_destroy (&self->updates);
   _mongoc_array_destroy (&self->deletes);
   bson_destroy (&self->ns_to_index);
   _mongoc_buffer_destroy (&self->ops);
   bson_free (self);
}

bool
mongoc_listof_bulkwritemodel_append_insertone (mongoc_listof_bulkwritemodel_t *self,
                                               const char *namespace,
                                               int namespace_len,
                                               mongoc_insertone_model_t model,
                                               bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (namespace);
   const bson_t *document = model.document;
   BSON_ASSERT_PARAM (document);
   BSON_ASSERT (document->len >= 5);

   bson_validate_flags_t validate_flags = _mongoc_default_insert_vflags;
   if (model.validate_flags.isset) {
      validate_flags = model.validate_flags.value;
   }

   if (!_mongoc_validate_new_document (document, validate_flags, error)) {
      return false;
   }

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
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "Only %" PRId32 " distinct collections may be inserted into. Got %" PRIu32,
                            INT32_MAX,
                            key_count);
            return false;
         }
         ns_index = (int32_t) key_count;
         bson_append_int32 (&self->ns_to_index, namespace, namespace_len, ns_index);
      }
      BSON_ASSERT (bson_append_int32 (&op, "insert", 6, ns_index));
   }

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
   mongoc_insertoneresult_t ior = {.is_insert = true, .id_iter = id_iter};
   _mongoc_array_append_val (&self->entries, ior);
   bool is_update = false;
   _mongoc_array_append_val (&self->updates, is_update);
   bool is_delete = false;
   _mongoc_array_append_val (&self->deletes, is_delete);
   bson_destroy (&op);
   return true;
}

bool
mongoc_listof_bulkwritemodel_append_updateone (mongoc_listof_bulkwritemodel_t *self,
                                               const char *namespace,
                                               int namespace_len,
                                               mongoc_updateone_model_t model,
                                               bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (namespace);
   const bson_t *filter = model.filter;
   const bson_t *update = model.update;
   BSON_ASSERT_PARAM (filter);
   BSON_ASSERT (filter->len >= 5);
   BSON_ASSERT_PARAM (update);
   BSON_ASSERT (update->len >= 5);

   bson_validate_flags_t validate_flags = _mongoc_default_update_vflags;
   if (model.validate_flags.isset) {
      validate_flags = model.validate_flags.value;
   }

   if (!_mongoc_validate_update (update, validate_flags, error)) {
      return false;
   }

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
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "Only %" PRId32 " distinct collections may be inserted into. Got %" PRIu32,
                            INT32_MAX,
                            key_count);
            return false;
         }
         ns_index = (int32_t) key_count;
         bson_append_int32 (&self->ns_to_index, namespace, namespace_len, ns_index);
      }
      BSON_ASSERT (bson_append_int32 (&op, "update", 6, ns_index));
   }


   BSON_ASSERT (bson_append_document (&op, "filter", 6, filter));
   if (_mongoc_document_is_pipeline (update)) {
      BSON_ASSERT (bson_append_array (&op, "updateMods", 10, update));
   } else {
      BSON_ASSERT (bson_append_document (&op, "updateMods", 10, update));
   }
   BSON_ASSERT (bson_append_bool (&op, "multi", 5, false));
   if (model.arrayFilters) {
      BSON_ASSERT (bson_append_array (&op, "arrayFilters", 12, model.arrayFilters));
   }
   if (model.collation) {
      BSON_ASSERT (bson_append_document (&op, "collation", 9, model.collation));
   }
   if (model.hint) {
      BSON_ASSERT (bson_append_value (&op, "hint", 4, model.hint));
   }
   if (model.upsert.isset) {
      BSON_ASSERT (bson_append_bool (&op, "upsert", 6, model.upsert.value));
   }

   BSON_ASSERT (_mongoc_buffer_append (&self->ops, bson_get_data (&op), op.len));

   self->n_ops++;
   // Add a slot in `entries` to keep the 1:1 mapping with the models.
   mongoc_insertoneresult_t ior = {.is_insert = false};
   _mongoc_array_append_val (&self->entries, ior);
   bool is_update = true;
   _mongoc_array_append_val (&self->updates, is_update);
   bool is_delete = false;
   _mongoc_array_append_val (&self->deletes, is_delete);
   bson_destroy (&op);
   return true;
}

bool
mongoc_listof_bulkwritemodel_append_updatemany (mongoc_listof_bulkwritemodel_t *self,
                                                const char *namespace,
                                                int namespace_len,
                                                mongoc_updatemany_model_t model,
                                                bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (namespace);
   const bson_t *filter = model.filter;
   const bson_t *update = model.update;
   BSON_ASSERT_PARAM (filter);
   BSON_ASSERT (filter->len >= 5);
   BSON_ASSERT_PARAM (update);
   BSON_ASSERT (update->len >= 5);

   bson_validate_flags_t validate_flags = _mongoc_default_update_vflags;
   if (model.validate_flags.isset) {
      validate_flags = model.validate_flags.value;
   }

   if (!_mongoc_validate_update (update, validate_flags, error)) {
      return false;
   }

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
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "Only %" PRId32 " distinct collections may be inserted into. Got %" PRIu32,
                            INT32_MAX,
                            key_count);
            return false;
         }
         ns_index = (int32_t) key_count;
         bson_append_int32 (&self->ns_to_index, namespace, namespace_len, ns_index);
      }
      BSON_ASSERT (bson_append_int32 (&op, "update", 6, ns_index));
   }


   BSON_ASSERT (bson_append_document (&op, "filter", 6, filter));
   if (_mongoc_document_is_pipeline (update)) {
      BSON_ASSERT (bson_append_array (&op, "updateMods", 10, update));
   } else {
      BSON_ASSERT (bson_append_document (&op, "updateMods", 10, update));
   }
   BSON_ASSERT (bson_append_bool (&op, "multi", 5, true));
   if (model.arrayFilters) {
      BSON_ASSERT (bson_append_array (&op, "arrayFilters", 12, model.arrayFilters));
   }
   if (model.collation) {
      BSON_ASSERT (bson_append_document (&op, "collation", 9, model.collation));
   }
   if (model.hint) {
      BSON_ASSERT (bson_append_value (&op, "hint", 4, model.hint));
   }
   if (model.upsert.isset) {
      BSON_ASSERT (bson_append_bool (&op, "upsert", 6, model.upsert.value));
   }

   BSON_ASSERT (_mongoc_buffer_append (&self->ops, bson_get_data (&op), op.len));

   self->n_ops++;
   // Add a slot in `entries` to keep the 1:1 mapping with the models.
   mongoc_insertoneresult_t ior = {.is_insert = false};
   _mongoc_array_append_val (&self->entries, ior);
   bool is_update = true;
   _mongoc_array_append_val (&self->updates, is_update);
   bool is_delete = false;
   _mongoc_array_append_val (&self->deletes, is_delete);

   self->has_multi_write = true;

   bson_destroy (&op);
   return true;
}


bool
mongoc_listof_bulkwritemodel_append_replaceone (mongoc_listof_bulkwritemodel_t *self,
                                                const char *namespace,
                                                int namespace_len,
                                                mongoc_replaceone_model_t model,
                                                bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (namespace);
   const bson_t *filter = model.filter;
   const bson_t *replacement = model.replacement;
   BSON_ASSERT_PARAM (filter);
   BSON_ASSERT (filter->len >= 5);
   BSON_ASSERT_PARAM (replacement);
   BSON_ASSERT (replacement->len >= 5);

   bson_validate_flags_t validate_flags = _mongoc_default_replace_vflags;
   if (model.validate_flags.isset) {
      validate_flags = model.validate_flags.value;
   }

   if (!_mongoc_validate_replace (replacement, validate_flags, error)) {
      return false;
   }

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
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "Only %" PRId32 " distinct collections may be inserted into. Got %" PRIu32,
                            INT32_MAX,
                            key_count);
            return false;
         }
         ns_index = (int32_t) key_count;
         bson_append_int32 (&self->ns_to_index, namespace, namespace_len, ns_index);
      }
      BSON_ASSERT (bson_append_int32 (&op, "update", 6, ns_index));
   }


   BSON_ASSERT (bson_append_document (&op, "filter", 6, filter));
   BSON_ASSERT (bson_append_document (&op, "updateMods", 10, replacement));

   self->max_insert_len = BSON_MAX (self->max_insert_len, replacement->len);

   BSON_ASSERT (bson_append_bool (&op, "multi", 5, false));
   if (model.upsert.isset) {
      BSON_ASSERT (bson_append_bool (&op, "upsert", 6, model.upsert.value));
   }
   if (model.collation) {
      BSON_ASSERT (bson_append_document (&op, "collation", 9, model.collation));
   }
   if (model.hint) {
      BSON_ASSERT (bson_append_value (&op, "hint", 4, model.hint));
   }

   BSON_ASSERT (_mongoc_buffer_append (&self->ops, bson_get_data (&op), op.len));

   self->n_ops++;
   // Add a slot in `entries` to keep the 1:1 mapping with the models.
   mongoc_insertoneresult_t ior = {.is_insert = false};
   _mongoc_array_append_val (&self->entries, ior);
   bool is_update = true;
   _mongoc_array_append_val (&self->updates, is_update);
   bool is_delete = false;
   _mongoc_array_append_val (&self->deletes, is_delete);

   bson_destroy (&op);
   return true;
}

bool
mongoc_listof_bulkwritemodel_append_deleteone (mongoc_listof_bulkwritemodel_t *self,
                                               const char *namespace,
                                               int namespace_len,
                                               mongoc_deleteone_model_t model,
                                               bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (namespace);
   const bson_t *filter = model.filter;
   BSON_ASSERT_PARAM (filter);
   BSON_ASSERT (filter->len >= 5);

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
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "Only %" PRId32 " distinct collections may be inserted into. Got %" PRIu32,
                            INT32_MAX,
                            key_count);
            return false;
         }
         ns_index = (int32_t) key_count;
         bson_append_int32 (&self->ns_to_index, namespace, namespace_len, ns_index);
      }
      BSON_ASSERT (bson_append_int32 (&op, "delete", 6, ns_index));
   }


   BSON_ASSERT (bson_append_document (&op, "filter", 6, filter));
   BSON_ASSERT (bson_append_bool (&op, "multi", 5, false));
   if (model.collation) {
      BSON_ASSERT (bson_append_document (&op, "collation", 9, model.collation));
   }
   if (model.hint) {
      BSON_ASSERT (bson_append_value (&op, "hint", 4, model.hint));
   }

   BSON_ASSERT (_mongoc_buffer_append (&self->ops, bson_get_data (&op), op.len));

   self->n_ops++;
   // Add a slot in `entries` to keep the 1:1 mapping with the models.
   mongoc_insertoneresult_t ior = {.is_insert = false};
   _mongoc_array_append_val (&self->entries, ior);
   bool is_update = false;
   _mongoc_array_append_val (&self->updates, is_update);
   bool is_delete = true;
   _mongoc_array_append_val (&self->deletes, is_delete);

   bson_destroy (&op);
   return true;
}


bool
mongoc_listof_bulkwritemodel_append_deletemany (mongoc_listof_bulkwritemodel_t *self,
                                                const char *namespace,
                                                int namespace_len,
                                                mongoc_deletemany_model_t model,
                                                bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (namespace);
   const bson_t *filter = model.filter;
   BSON_ASSERT_PARAM (filter);
   BSON_ASSERT (filter->len >= 5);

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
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "Only %" PRId32 " distinct collections may be inserted into. Got %" PRIu32,
                            INT32_MAX,
                            key_count);
            return false;
         }
         ns_index = (int32_t) key_count;
         bson_append_int32 (&self->ns_to_index, namespace, namespace_len, ns_index);
      }
      BSON_ASSERT (bson_append_int32 (&op, "delete", 6, ns_index));
   }


   BSON_ASSERT (bson_append_document (&op, "filter", 6, filter));
   BSON_ASSERT (bson_append_bool (&op, "multi", 5, true));
   if (model.collation) {
      BSON_ASSERT (bson_append_document (&op, "collation", 9, model.collation));
   }
   if (model.hint) {
      BSON_ASSERT (bson_append_value (&op, "hint", 4, model.hint));
   }

   BSON_ASSERT (_mongoc_buffer_append (&self->ops, bson_get_data (&op), op.len));

   self->n_ops++;
   // Add a slot in `entries` to keep the 1:1 mapping with the models.
   mongoc_insertoneresult_t ior = {.is_insert = false};
   _mongoc_array_append_val (&self->entries, ior);
   bool is_update = false;
   _mongoc_array_append_val (&self->updates, is_update);
   bool is_delete = true;
   _mongoc_array_append_val (&self->deletes, is_delete);

   self->has_multi_write = true;

   bson_destroy (&op);
   return true;
}

const mongoc_listof_writeconcernerror_t *
mongoc_bulkwriteexception_writeConcernErrors (const mongoc_bulkwriteexception_t *self)
{
   BSON_ASSERT_PARAM (self);
   return &self->listof_wce;
}

const mongoc_writeconcernerror_t *
mongoc_listof_writeconcernerror_at (const mongoc_listof_writeconcernerror_t *self, size_t idx)
{
   BSON_ASSERT_PARAM (self);
   if (idx > self->entries.len) {
      return NULL;
   }

   return _mongoc_array_index (&self->entries, mongoc_writeconcernerror_t *, idx);
}

size_t
mongoc_listof_writeconcernerror_len (const mongoc_listof_writeconcernerror_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->entries.len;
}

int32_t
mongoc_writeconcernerror_code (const mongoc_writeconcernerror_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->code;
}

const bson_t *
mongoc_writeconcernerror_details (const mongoc_writeconcernerror_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->details;
}

const char *
mongoc_writeconcernerror_message (const mongoc_writeconcernerror_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->message;
}


const mongoc_mapof_writeerror_t *
mongoc_bulkwriteexception_writeErrors (const mongoc_bulkwriteexception_t *self)
{
   BSON_ASSERT_PARAM (self);
   return &self->mapof_we;
}

const bson_t *
mongoc_bulkwriteexception_errorReply (const mongoc_bulkwriteexception_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->error_reply;
}


const mongoc_writeerror_t *
mongoc_mapof_writeerror_lookup (const mongoc_mapof_writeerror_t *self, size_t idx)
{
   BSON_ASSERT_PARAM (self);

   if (idx > self->entries.len) {
      return NULL;
   }
   mongoc_writeerror_t *we = _mongoc_array_index (&self->entries, mongoc_writeerror_t *, idx);
   if (we == NULL) {
      return NULL;
   }
   return we;
}

int32_t
mongoc_writeerror_code (const mongoc_writeerror_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->code;
}

const bson_t *
mongoc_writeerror_details (const mongoc_writeerror_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->details;
}

const char *
mongoc_writeerror_message (const mongoc_writeerror_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->message;
}
