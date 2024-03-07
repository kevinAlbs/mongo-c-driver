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
#include <mongoc-client.h>
#include <mongoc-write-concern.h>

BSON_BEGIN_DECLS

// Model the API close to the specification to help review the specification.
// API patterns:
// Opaque types are pointers (e.g. `mongoc_listof_bulkwritemodel_t`)
// Optional struct values are pointers (e.g. `mongoc_bulkwriteoptions_t`)
// Return types (e.g. `mongoc_bulkwriteresult_t`) have cleanup functions.
// Non-opaque argument types do not own their fields (e.g.
// `mongoc_bulkwriteoptions_t` has no `cleanup` function).

typedef struct _mongoc_bulkwriteoptions_t mongoc_bulkwriteoptions_t;
typedef struct _mongoc_bulkwriteresult_t mongoc_bulkwriteresult_t;
typedef struct _mongoc_bulkwriteexception_t mongoc_bulkwriteexception_t;
typedef struct _mongoc_listof_bulkwritemodel_t mongoc_listof_bulkwritemodel_t;

BSON_EXPORT (mongoc_listof_bulkwritemodel_t *)
mongoc_listof_bulkwritemodel_new (void);

BSON_EXPORT (void)
mongoc_listof_bulkwritemodel_destroy (mongoc_listof_bulkwritemodel_t *self);

typedef struct {
   // `res` may be NULL if an error occurred or if an unacknowledged write
   // concern was used.
   mongoc_bulkwriteresult_t *res;
   // `exc` may be NULL if no error occurred or if an unacknowledged write
   // concern was used.
   mongoc_bulkwriteexception_t *exc;
} mongoc_bulkwritereturn_t;

BSON_EXPORT (void)
mongoc_bulkwritereturn_cleanup (mongoc_bulkwritereturn_t *self);

/**
 * Executes a list of mixed write operations.
 *
 * @throws BulkWriteException
 */
BSON_EXPORT (mongoc_bulkwritereturn_t)
mongoc_client_bulkwrite (mongoc_client_t *self,
                         mongoc_listof_bulkwritemodel_t *models,
                         mongoc_bulkwriteoptions_t *options // may be NULL
);

typedef struct {
   /**
    * The document to insert.
    */
   bson_t *document;
   // `extra` is appended to the update operation.
   // It is included to support future server options.
   bson_t *extra; // may be NULL.
} mongoc_insertone_model_t;

BSON_EXPORT (bool)
mongoc_listof_bulkwritemodel_append_insertone (
   mongoc_listof_bulkwritemodel_t *self,
   const char *namespace,
   int namespace_len,
   mongoc_insertone_model_t model,
   bson_error_t *error);

typedef struct {
   bool value;
   bool isset;
} mongoc_opt_bool_t;

#define MONGOC_OPT_BOOL_TRUE       \
   (mongoc_opt_bool_t)             \
   {                               \
      .value = true, .isset = true \
   }
#define MONGOC_OPT_BOOL_FALSE       \
   (mongoc_opt_bool_t)              \
   {                                \
      .value = false, .isset = true \
   }
#define MONGOC_OPT_BOOL_UNSET \
   (mongoc_opt_bool_t)        \
   {                          \
      .isset = false          \
   }

typedef struct {
   /**
    * The filter to apply.
    */
   bson_t *filter;
   /**
    * The update document or pipeline to apply to the selected document.
    */
   bson_t *update;
   /**
    * A set of filters specifying to which array elements an update should
    * apply.
    *
    * This option is sent only if the caller explicitly provides a value.
    */
   bson_t *arrayFilters; // may be NULL

   /**
    * Specifies a collation.
    *
    * This option is sent only if the caller explicitly provides a value.
    */
   bson_t *collation; // may be NULL

   /**
    * The index to use. Specify either the index name as a string or the index
    * key pattern. If specified, then the query system will only consider plans
    * using the hinted index.
    *
    * This option is only sent if the caller explicitly provides a value.
    */
   bson_value_t *hint; // may be NULL

   /**
    * When true, creates a new document if no document matches the query.
    * Defaults to false.
    *
    * This options is sent only if the caller explicitly provides a value.
    */
   mongoc_opt_bool_t upsert;
   // `extra` is appended to the update operation.
   // It is included to support future server options.
   bson_t *extra; // may be NULL.
} mongoc_updateone_model_t;

typedef struct {
   /**
    * The filter to apply.
    */
   bson_t *filter;
   /**
    * The update document or pipeline to apply to the selected document.
    */

   bson_t *update;
   // If `is_pipeline` is true, `update` is treated as a pipeline.
   bool is_pipeline;

   /**
    * A set of filters specifying to which array elements an update should
    * apply.
    *
    * This option is sent only if the caller explicitly provides a value.
    */
   bson_t *arrayFilters; // may be NULL

   /**
    * Specifies a collation.
    *
    * This option is sent only if the caller explicitly provides a value.
    */
   bson_t *collation; // may be NULL

   /**
    * The index to use. Specify either the index name as a string or the index
    * key pattern. If specified, then the query system will only consider plans
    * using the hinted index.
    *
    * This option is only sent if the caller explicitly provides a value.
    */
   bson_value_t *hint; // may be NULL

   /**
    * When true, creates a new document if no document matches the query.
    * Defaults to false.
    *
    * This options is sent only if the caller explicitly provides a value.
    */
   mongoc_opt_bool_t upsert;
   // `extra` is appended to the update operation.
   // It is included to support future server options.
   bson_t *extra; // may be NULL.
} mongoc_updatemany_model_t;

typedef struct {
   /**
    * The filter to apply.
    */
   bson_t *filter;

   /**
    * Specifies a collation.
    *
    * This option is sent only if the caller explicitly provides a value.
    */
   bson_t *collation; // may be NULL

   /**
    * The index to use. Specify either the index name as a string or the index
    * key pattern. If specified, then the query system will only consider plans
    * using the hinted index.
    *
    * This option is only sent if the caller explicitly provides a value.
    */
   bson_value_t *hint; // may be NULL

   // `extra` is appended to the delete operation.
   // It is included to support future server options.
   bson_t *extra; // may be NULL.
} mongoc_deleteone_model_t;

typedef struct {
   /**
    * The filter to apply.
    */
   bson_t *filter;

   /**
    * Specifies a collation.
    *
    * This option is sent only if the caller explicitly provides a value.
    */
   bson_t *collation; // may be NULL

   /**
    * The index to use. Specify either the index name as a string or the index
    * key pattern. If specified, then the query system will only consider plans
    * using the hinted index.
    *
    * This option is only sent if the caller explicitly provides a value.
    */
   bson_value_t *hint; // may be NULL

   // `extra` is appended to the delete operation.
   // It is included to support future server options.
   bson_t *extra; // may be NULL.
} mongoc_deletemany_model_t;

struct _mongoc_bulkwriteoptions_t {
   /**
    * Whether the operations in this bulk write should be executed in the order
    * in which they were specified. If false, writes will continue to be
    * executed if an individual write fails. If true, writes will stop executing
    * if an individual write fails.
    *
    * Defaults to false.
    */
   bool ordered;

   /**
    * If true, allows the writes to opt out of document-level validation.
    *
    * Defaults to false.
    *
    * This option is only sent if the caller explicitly provides a value.
    */
   mongoc_opt_bool_t bypassDocumentValidation;

   /**
    * A map of parameter names and values to apply to all operations within the
    * bulk write. Value must be constant or closed expressions that do not
    * reference document fields. Parameters can then be accessed as variables in
    * an aggregate expression context (e.g. "$$var").
    *
    * This option is only sent if the caller explicitly provides a value.
    */
   // May be NULL.
   bson_t *let;

   /**
    * The write concern to use for this bulk write.
    *
    * NOT REQUIRED TO IMPLEMENT. Drivers MUST expose this option if retrieving a
    * handle to a client with a different write concern configured than that of
    * the user's standard URI options is nontrivial. Drivers MAY omit this
    * option if they provide a way to retrieve a lightweight handle to a client
    * with a custom write concern configured, e.g. a
    * MongoClient.withWriteConcern() method.
    */
   // May be NULL.
   mongoc_write_concern_t *writeConcern;

   /**
    * Whether detailed results for each successful operation should be included
    * in the returned BulkWriteResult.
    *
    * Defaults to false.
    */
   bool verboseResults;

   // `extra` is appended to the bulkWrite command.
   // It is included to support future server options.
   void *extra;
};

typedef struct _mongoc_mapof_insertoneresult_t mongoc_mapof_insertoneresult_t;
typedef struct _mongoc_insertoneresult_t mongoc_insertoneresult_t;
BSON_EXPORT (const mongoc_insertoneresult_t *)
mongoc_mapof_insertoneresult_lookup (mongoc_mapof_insertoneresult_t *self,
                                     size_t idx);

typedef struct _mongoc_mapof_updateresult_t mongoc_mapof_updateresult_t;
typedef struct _mongoc_updateresult_t mongoc_updateresult_t;
BSON_EXPORT (const mongoc_updateresult_t *)
mongoc_mapof_updateresult_lookup (mongoc_mapof_updateresult_t *self,
                                  size_t idx);

typedef struct _mongoc_mapof_deleteresult_t mongoc_mapof_deleteresult_t;
typedef struct _mongoc_deleteresult_t mongoc_deleteresult_t;
BSON_EXPORT (const mongoc_deleteresult_t *)
mongoc_mapof_deleteresult_lookup (mongoc_mapof_deleteresult_t *self,
                                  size_t idx);


struct _mongoc_bulkwriteresult_t {
   /**
    * Indicates whether this write result was acknowledged. If not, then all
    * other members of this result will be undefined.
    *
    * NOT REQUIRED TO IMPLEMENT. See the CRUD specification for more guidance on
    * modeling unacknowledged results.
    */
   bool acknowledged;

   /**
    * Indicates whether the results are verbose. If false, the insertResults,
    * updateResults, and deleteResults fields in this result will be undefined.
    *
    * NOT REQUIRED TO IMPLEMENT. See below for other ways to differentiate
    * summary results from verbose results.
    */
   bool hasVerboseResults;

   /**
    * The total number of documents inserted across all insert operations.
    */
   int64_t insertedCount;

   /**
    * The total number of documents upserted across all update operations.
    */
   int64_t upsertedCount;

   /**
    * The total number of documents matched across all update operations.
    */
   int64_t matchedCount;

   /**
    * The total number of documents modified across all update operations.
    */
   int64_t modifiedCount;

   /**
    * The total number of documents deleted across all delete operations.
    */
   int64_t deletedCount;

   /**
    * The results of each individual insert operation that was successfully
    * performed.
    */
   // May be NULL if mongoc_bulkwriteoptions_t::verboseResults was false.
   mongoc_mapof_insertoneresult_t *insertResults;

   /**
    * The results of each individual update operation that was successfully
    * performed.
    */
   // May be NULL if mongoc_bulkwriteoptions_t::verboseResults was false.
   mongoc_mapof_updateresult_t *updateResult;

   /**
    * The results of each individual delete operation that was successfully
    * performed.
    */
   // May be NULL if mongoc_bulkwriteoptions_t::verboseResults was false.
   mongoc_mapof_deleteresult_t *deleteResults;

   // `internal` is reserved.
   void *internal;
};

struct _mongoc_insertoneresult_t {
   /**
    * The _id of the inserted document.
    */
   bson_value_t inserted_id;

   // `internal` is reserved.
   void *internal;
};

struct _mongoc_updateresult_t {
   /**
    * The number of documents that matched the filter.
    */
   int64_t matchedCount;

   /**
    * The number of documents that were modified.
    */
   int64_t modifiedCount;

   /**
    * The number of documents that were upserted.
    *
    * NOT REQUIRED TO IMPLEMENT. Drivers may choose not to provide this property
    * so long as it is always possible to discern whether an upsert took place.
    */
   int64_t upsertedCount;

   /**
    * The _id field of the upserted document if an upsert occurred.
    */
   // May be NULL.
   const bson_value_t *upsertedId;

   // `internal` is reserved.
   void *internal;
};

struct _mongoc_deleteresult_t {
   /**
    * The number of documents that were deleted.
    */
   int64_t deletedCount;

   // `internal` is reserved.
   void *internal;
};

typedef struct _mongoc_listof_writeconcernerror_t
   mongoc_listof_writeconcernerror_t;
typedef struct _mongoc_writeconcernerror_t mongoc_writeconcernerror_t;

BSON_EXPORT (const mongoc_writeconcernerror_t *)
mongoc_listof_writeconcernerror_at (mongoc_listof_writeconcernerror_t *self,
                                    size_t idx);

BSON_EXPORT (size_t)
mongoc_listof_writeconcernerror_len (mongoc_listof_writeconcernerror_t *self);

typedef struct _mongoc_mapof_writeerror_t mongoc_mapof_writeerror_t;
typedef struct _mongoc_writeerror_t mongoc_writeerror_t;

BSON_EXPORT (const mongoc_writeerror_t *)
mongoc_mapof_writeerror_lookup (mongoc_mapof_writeerror_t *self, size_t idx);

struct _mongoc_bulkwriteexception_t {
   /**
    * A top-level error that occurred when attempting to communicate with the
    * server or execute the bulk write. This value may not be populated if the
    * exception was thrown due to errors occurring on individual writes.
    */
   // May be unset.
   struct {
      bool isset;
      bson_error_t value;
      bson_t document;
   } error;

   /**
    * Write concern errors that occurred while executing the bulk write. This
    * list may have multiple items if more than one server command was required
    * to execute the bulk write.
    */
   mongoc_listof_writeconcernerror_t *writeConcernErrors;

   /**
    * Errors that occurred during the execution of individual write operations.
    * This map will contain at most one entry if the bulk write was ordered.
    */
   mongoc_mapof_writeerror_t *writeErrors;

   /**
    * The results of any successful operations that were performed before the
    * error was encountered.
    */
   // May be NULL.
   mongoc_bulkwriteresult_t *partialResult;
};

struct _mongoc_writeconcernerror_t {
   /**
    * An integer value identifying the write concern error. Corresponds to the
    * "writeConcernError.code" field in the command response.
    *
    * @see https://www.mongodb.com/docs/manual/reference/method/WriteResult/
    */
   int32_t code;

   /**
    * A document identifying the write concern setting related to the error.
    * Corresponds to the "writeConcernError.errInfo" field in the command
    * response.
    *
    * @see https://www.mongodb.com/docs/manual/reference/method/WriteResult/
    */
   // May be NULL.
   bson_t *details;

   /**
    * A description of the error. Corresponds to the
    * "writeConcernError.errmsg" field in the command response.
    *
    * @see https://www.mongodb.com/docs/manual/reference/method/WriteResult/
    */
   char *message;

   // `internal` is reserved.
   void *internal;
};


struct _mongoc_writeerror_t {
   /**
    * An integer value identifying the write error. Corresponds to the
    * "writeErrors[].code" field in the command response.
    *
    * @see https://www.mongodb.com/docs/manual/reference/method/WriteResult/
    */
   int32_t code;

   /**
    * A document providing more information about the write error (e.g. details
    * pertaining to document validation). Corresponds to the
    * "writeErrors[].errInfo" field in the command response.
    *
    * @see https://www.mongodb.com/docs/manual/reference/method/WriteResult/
    */
   bson_t *details;

   /**
    * A description of the error. Corresponds to the "writeErrors[].errmsg"
    * field in the command response.
    *
    * @see https://www.mongodb.com/docs/manual/reference/method/WriteResult/
    */
   char *message;

   // `internal` is reserved.
   void *internal;
};

BSON_END_DECLS
