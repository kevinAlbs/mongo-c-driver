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

#include <mongoc/mongoc.h>
#include <test-libmongoc.h>
#include <TestSuite.h>
#include <test-conveniences.h>
#include <mongoc-array-private.h>

#include <mongoc-client-private.h>

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
   bson_iter_t id_iter; // Iterator to the `_id` field.
   bool failed;         // True if insert was attempted but failed.
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

   MONGOC_DEBUG ("appending op: %s", tmp_json (&op));

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
   // `inserted_ids` is an array sized to the number of operations.
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
   return iie;
}

typedef struct _mongoc_bulkwriteexception_t {
   struct {
      bson_error_t error;
      bson_t document;
      bool isset;
   } optional_error;

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

const bson_t *
mongoc_bulkwriteexception_get_writeerror (
   const mongoc_bulkwriteexception_t *self, size_t index)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT (false &&
                "mongoc_bulkwriteexception_get_writeerror not yet implemented");
   return NULL;
}

static mongoc_bulkwriteexception_t *
mongoc_bulkwriteexception_new_from_error (bson_error_t *error,
                                          bson_t *error_document)
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
mongoc_bulkwriteexception_destroy (mongoc_bulkwriteexception_t *self)
{
   if (!self) {
      return;
   }

   bson_destroy (&self->optional_error.document);
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
   bson_t cmd_reply = BSON_INITIALIZER;
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
   }

   MONGOC_DEBUG ("created command: %s", tmp_json (&cmd));

   // Create the payload 1 and send.
   {
      mongoc_cmd_parts_init (&parts, self, "admin", MONGOC_QUERY_NONE, &cmd);
      bson_error_t error;
      if (!mongoc_cmd_parts_assemble (&parts, ss, &error)) {
         ret.exc = mongoc_bulkwriteexception_new_from_error (&error, NULL);
         goto fail;
      }
      parts.assembled.payload_identifier = "ops";
      if (models->ops.len > 10000) {
         bson_set_error (&error, 123, 456, "Large writes not yet supported");
         ret.exc = mongoc_bulkwriteexception_new_from_error (&error, NULL);
         goto fail;
      }
      parts.assembled.payload = models->ops.data;
      BSON_ASSERT (bson_in_range_int32_t_unsigned (models->ops.len));
      parts.assembled.payload_size = (int32_t) models->ops.len;

      bool ok = mongoc_cluster_run_command_monitored (
         &self->cluster, &parts.assembled, &cmd_reply, &error);
      if (!ok) {
         ret.exc =
            mongoc_bulkwriteexception_new_from_error (&error, &cmd_reply);
         goto fail;
      }
      MONGOC_DEBUG ("got reply: %s\n", tmp_json (&cmd_reply));
   }

   // Construct a result and/or exception.
   {
      // TODO: parse other top-level fields and errors.
      ret.res = mongoc_bulkwriteresult_new ();

      // Check for insertedCount.
      bson_iter_t iter;

      // Server returns an int32.
      if (bson_iter_init_find (&iter, &cmd_reply, "nInserted") &&
          BSON_ITER_HOLDS_INT32 (&iter)) {
         ret.res->insertedcount.isset = true;
         ret.res->insertedcount.value = (int64_t) bson_iter_int32 (&iter);
      } else {
         bson_error_t error;
         bson_set_error (&error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_COMMAND_INVALID_ARG,
                         "expected to find int32 `nInserted`, but did not");
         ret.exc =
            mongoc_bulkwriteexception_new_from_error (&error, &cmd_reply);
         goto fail;
      }

      // Copy `inserted_ids` to the result.
      _mongoc_array_copy (&ret.res->inserted_ids, &models->inserted_ids);


      if (bson_iter_init_find (&iter, &cmd_reply, "nErrors") &&
          BSON_ITER_HOLDS_INT32 (&iter)) {
         if (bson_iter_int32 (&iter) != 0) {
            BSON_ASSERT (false &&
                         "TODO: handling nErrors > 0 is not-yet-implemented");
            // TODO: Remove insertedIDs for inserts that resulted in an error.
         }
      } else {
         bson_error_t error;
         bson_set_error (&error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_COMMAND_INVALID_ARG,
                         "expected to find int32 `nErrors`, but did not");
         ret.exc =
            mongoc_bulkwriteexception_new_from_error (&error, &cmd_reply);
         goto fail;
      }
   }

fail:
   bson_destroy (&cmd_reply);
   mongoc_cmd_parts_cleanup (&parts);
   bson_destroy (&cmd);
   mongoc_server_stream_cleanup (ss);
   return ret;
}

static void
test_bulkWrite_insert (void *unused)
{
   BSON_UNUSED (unused);
   mongoc_client_t *client = test_framework_new_default_client ();

   // Drop prior data.
   {
      mongoc_collection_t *coll =
         mongoc_client_get_collection (client, "db", "coll");
      mongoc_collection_drop (coll, NULL); // Ignore return.
      mongoc_collection_destroy (coll);
   }

   // Create list of insert models.
   mongoc_listof_bulkwritemodel_t *lb;
   {
      lb = mongoc_listof_bulkwritemodel_new ();
      bson_error_t error;
      ASSERT_OR_PRINT (
         mongoc_listof_bulkwritemodel_append_insertone (
            lb, "db.coll", -1, tmp_bson ("{'_id': 123 }"), &error),
         error);
      ASSERT_OR_PRINT (
         mongoc_listof_bulkwritemodel_append_insertone (
            lb, "db.coll", -1, tmp_bson ("{'_id': 456 }"), &error),
         error);
   }

   // Do the bulk write.
   mongoc_bulkwritereturn_t br =
      mongoc_client_bulkwrite (client, lb, NULL /* opts */);

   // Ensure no error.
   {
      if (br.exc) {
         const bson_t *error_document;
         bson_error_t error;
         if (mongoc_bulkwriteexception_has_error (br.exc)) {
            mongoc_bulkwriteexception_get_error (
               br.exc, &error, &error_document);
            test_error ("Expected no exception, got: %s\n%s\n",
                        error.message,
                        tmp_json (error_document));
         }
         test_error ("Expected no exception, got one with no top-level error");
      }
   }

   // Ensure results report IDs inserted.
   {
      ASSERT (br.res);

      ASSERT (mongoc_bulkwriteresult_has_insertedcount (br.res));
      int64_t count = mongoc_bulkwriteresult_get_insertedcount (br.res);
      ASSERT_CMPINT64 (count, ==, 2);

      // Check index 0.
      {
         const mongoc_insertoneresult_t *ir =
            mongoc_bulkwriteresult_get_insertoneresult (br.res, 0);
         ASSERT (ir);
         const bson_value_t *id = mongoc_insertoneresult_get_insertedid (ir);
         bson_value_t expected = {.value_type = BSON_TYPE_INT32,
                                  .value = {.v_int32 = 123}};
         ASSERT_BSONVALUE_EQ (id, &expected);
      }

      // Check index 1.
      {
         const mongoc_insertoneresult_t *ir =
            mongoc_bulkwriteresult_get_insertoneresult (br.res, 1);
         ASSERT (ir);
         const bson_value_t *id = mongoc_insertoneresult_get_insertedid (ir);
         bson_value_t expected = {.value_type = BSON_TYPE_INT32,
                                  .value = {.v_int32 = 456}};
         ASSERT_BSONVALUE_EQ (id, &expected);
      }

      // Check no index 2.
      ASSERT (!mongoc_bulkwriteresult_get_insertoneresult (br.res, 2));
   }

   mongoc_bulkwritereturn_cleanup (&br);
   mongoc_listof_bulkwritemodel_destroy (lb);
   mongoc_client_destroy (client);
}

static void
test_bulkWrite_insert_with_writeError (void *unused)
{
   BSON_UNUSED (unused);
   mongoc_client_t *client = test_framework_new_default_client ();

   // Drop prior data.
   {
      mongoc_collection_t *coll =
         mongoc_client_get_collection (client, "db", "coll");
      mongoc_collection_drop (coll, NULL); // Ignore return.
      mongoc_collection_destroy (coll);
   }

   // Create list of insert models.
   mongoc_listof_bulkwritemodel_t *lb;
   {
      lb = mongoc_listof_bulkwritemodel_new ();
      bson_error_t error;
      ASSERT_OR_PRINT (
         mongoc_listof_bulkwritemodel_append_insertone (
            lb, "db.coll", -1, tmp_bson ("{'_id': 123 }"), &error),
         error);
      ASSERT_OR_PRINT (
         mongoc_listof_bulkwritemodel_append_insertone (
            lb, "db.coll", -1, tmp_bson ("{'_id': 123 }"), &error),
         error);
   }

   // Do the bulk write.
   mongoc_bulkwritereturn_t br =
      mongoc_client_bulkwrite (client, lb, NULL /* opts */);

   // Expect an error due to duplicate keys.
   {
      ASSERT (br.exc);
      const bson_t *error =
         mongoc_bulkwriteexception_get_writeerror (br.exc, 0);
      ASSERT (error);
      ASSERT_MATCH (error, BSON_STR ({
                       "errmsg" : "E11000 duplicate key error collection: "
                                  "db.coll index: _id_ dup key: { _id: 123 }"
                    }));
   }

   // Ensure only reported ID of successful insert is reported.
   {
      ASSERT (br.res);

      ASSERT (mongoc_bulkwriteresult_has_insertedcount (br.res));
      int64_t count = mongoc_bulkwriteresult_get_insertedcount (br.res);
      ASSERT_CMPINT64 (count, ==, 2);

      // Check index 0.
      {
         const mongoc_insertoneresult_t *ir =
            mongoc_bulkwriteresult_get_insertoneresult (br.res, 0);
         ASSERT (ir);
         const bson_value_t *id = mongoc_insertoneresult_get_insertedid (ir);
         bson_value_t expected = {.value_type = BSON_TYPE_INT32,
                                  .value = {.v_int32 = 123}};
         ASSERT_BSONVALUE_EQ (id, &expected);
      }

      // Expect failed insert not to be reported.
      {
         const mongoc_insertoneresult_t *ir =
            mongoc_bulkwriteresult_get_insertoneresult (br.res, 1);
         ASSERT (!ir);
      }
   }

   mongoc_bulkwritereturn_cleanup (&br);
   mongoc_listof_bulkwritemodel_destroy (lb);
   mongoc_client_destroy (client);
}


void
test_bulkWrite_install (TestSuite *suite)
{
   TestSuite_AddFull (
      suite,
      "/bulkWrite/insert",
      test_bulkWrite_insert,
      NULL /* dtor */,
      NULL /* ctx */,
      test_framework_skip_if_max_wire_version_less_than_25 // require server 8.0
   );

   TestSuite_AddFull (
      suite,
      "/bulkWrite/insert/writeError",
      test_bulkWrite_insert_with_writeError,
      NULL /* dtor */,
      NULL /* ctx */,
      test_framework_skip_if_max_wire_version_less_than_25 // require server 8.0
   );
}
