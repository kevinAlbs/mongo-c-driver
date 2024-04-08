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
#include <mongoc-bulkwritev2.h>
#include <mongoc-bulkwrite.h>

struct _mongoc_bulkwritev2_t {
   mongoc_client_t *client;
   mongoc_listof_bulkwritemodel_t *models;
   bool executed;
};


// `mongoc_client_bulkwritev2_new` creates a new bulk write operation.
mongoc_bulkwritev2_t *
mongoc_client_bulkwritev2_new (mongoc_client_t *self,
                               mongoc_bulkwriteoptionsv2_t opts)
{
   mongoc_bulkwritev2_t *bw = bson_malloc0 (sizeof (mongoc_bulkwritev2_t));
   bw->client = self;
   bw->models = mongoc_listof_bulkwritemodel_new ();
   return bw;
}

void
mongoc_bulkwritev2_destroy (mongoc_bulkwritev2_t *self)
{
   if (!self) {
      return;
   }
   mongoc_listof_bulkwritemodel_destroy (self->models);
   bson_free (self);
}


bool
mongoc_client_bulkwritev2_append_insertone (mongoc_bulkwritev2_t *self,
                                            const char *namespace,
                                            int namespace_len,
                                            mongoc_insertone_modelv2_t model,
                                            bson_error_t *error)
{
   if (self->executed) {
      bson_set_error (error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "bulk write already executed");
      return false;
   }

   return mongoc_listof_bulkwritemodel_append_insertone (
      self->models,
      namespace,
      namespace_len,
      (mongoc_insertone_model_t){
         .document = model.document,
         .extra = model.extra,
         .validate_flags =
            model.validate_flags.isset
               ? MONGOC_OPT_VALIDATE_FLAGS_VAL (model.validate_flags.value)
               : MONGOC_OPT_VALIDATE_FLAGS_UNSET},
      error);
}


struct _mongoc_bulkwritereturnv2_t {
   int placeholder;
};

bool
mongoc_bulkwritereturnv2_acknowledged (const mongoc_bulkwritereturnv2_t *self)
{
   return false;
}

int64_t
mongoc_bulkwritereturnv2_insertedCount (const mongoc_bulkwritereturnv2_t *self)
{
   return 0;
}

int64_t
mongoc_bulkwritereturnv2_upsertedCount (const mongoc_bulkwritereturnv2_t *self)
{
   return 0;
}

int64_t
mongoc_bulkwritereturnv2_matchedCount (const mongoc_bulkwritereturnv2_t *self)
{
   return 0;
}

int64_t
mongoc_bulkwritereturnv2_modifiedCount (const mongoc_bulkwritereturnv2_t *self)
{
   return 0;
}

int64_t
mongoc_bulkwritereturnv2_deletedCount (const mongoc_bulkwritereturnv2_t *self)
{
   return 0;
}

// `mongoc_bulkwritereturnv2_verboseResults` returns NULL if verbose results
// were not requested (the default). Otherwise, returns a document with these
// fields: `insertResults`, `updateResult`, `deleteResults`
const bson_t *
mongoc_bulkwritereturnv2_verboseResults (const mongoc_bulkwritereturnv2_t *self)
{
   return NULL;
}

// `mongoc_bulkwritereturnv2_error` returns true if an error occurred.
// If an error occurred, `error_document` is set to a document containing these
// fields: `errorLabels`, `writeConcernErrors`, `writeErrors`, `errorReplies`
bool
mongoc_bulkwritereturnv2_error (const mongoc_bulkwritereturnv2_t *self,
                                bson_error_t *error,
                                const bson_t **error_document)
{
   bson_set_error (error,
                   MONGOC_ERROR_COMMAND,
                   MONGOC_ERROR_COMMAND_INVALID_ARG,
                   "mongoc_bulkwritereturnv2_error is not yet implemented");
   return true;
}

void
mongoc_bulkwritereturnv2_destroy (mongoc_bulkwritereturnv2_t *self)
{
}

mongoc_bulkwritereturnv2_t *
mongoc_bulkwritev2_execute (mongoc_bulkwritev2_t *self)
{
   return NULL;
}
