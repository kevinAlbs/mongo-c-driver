#include <mongoc-bulkwrite.h>

mongoc_listof_bulkwritemodel_t *
mongoc_listof_bulkwritemodel_new (void)
{
   return NULL;
}

void
mongoc_listof_bulkwritemodel_destroy (mongoc_listof_bulkwritemodel_t *self)
{
   if (!self) {
      return;
   }
}

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

bool
mongoc_listof_bulkwritemodel_append_insertone (
   mongoc_listof_bulkwritemodel_t *self,
   const char *namespace,
   int namespace_len,
   mongoc_insertone_model_t model,
   bson_error_t *error)
{
   bson_set_error (
      error,
      123,
      456,
      "mongoc_listof_bulkwritemodel_append_insertone is not yet implemented");
   return false;
}
