#include <mongoc-bulkwrite.h>
#include <mongoc-buffer-private.h>
#include <mongoc-array-private.h>
#include <mongoc-error.h>

mongoc_bulkwritereturn_t
mongoc_client_bulkwrite (mongoc_client_t *client,
                         mongoc_listof_bulkwritemodel_t *models,
                         mongoc_bulkwriteoptions_t *options // may be NULL
)
{
   return (mongoc_bulkwritereturn_t){0};
}

const mongoc_insertoneresult_t *
mongoc_mapof_insertoneresult_lookup (mongoc_mapof_insertoneresult_t *self,
                                     size_t idx)
{
   return NULL;
}

void
mongoc_bulkwritereturn_cleanup (mongoc_bulkwritereturn_t *self)
{
   if (!self) {
      return;
   }
}

struct _mongoc_listof_bulkwritemodel_t {
   // `ops` is a document sequence.
   mongoc_buffer_t ops;
   size_t n_ops;
   // `ns_to_index` maps a namespace to an index.
   bson_t ns_to_index;
   // `inserted_ids` is an array sized to the number of operations.
   // If the operation was an insert, the `id` is stored.
   mongoc_array_t inserted_ids;
};

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
   if (!self) {
      return;
   }
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
   mongoc_insertone_model_t model,
   bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_ASSERT_PARAM (namespace);
   bson_t *document = model.document;
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

   // If `document` does not contain `_id`, add one in the beginning.
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
