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

#ifndef MONGOC_BULKWRITE_H
#define MONGOC_BULKWRITE_H

#include <bson/bson.h>
#include <mongoc-client.h>
#include <mongoc-write-concern.h>

BSON_BEGIN_DECLS

// Model the API close to the specification to help review the specification.
// API patterns:
// Opaque types are pointers (e.g. `mongoc_listof_bulkwritemodel_t`)
// Optional struct values are pointers (e.g. `mongoc_bulkwriteoptions_t`)
// Return types (e.g. `mongoc_bulkwritereturn_t`) have cleanup functions.
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
   // `res` may contain a partial result if an error occurred.
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
   const bson_t *document;
} mongoc_insertone_model_t;

BSON_EXPORT (bool)
mongoc_listof_bulkwritemodel_append_insertone (mongoc_listof_bulkwritemodel_t *self,
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
   const bson_t *filter;
   /**
    * The update document or pipeline to apply to the selected document.
    */
   const bson_t *update;
   /**
    * A set of filters specifying to which array elements an update should
    * apply.
    *
    * This option is sent only if the caller explicitly provides a value.
    */
   const bson_t *arrayFilters; // may be NULL

   /**
    * Specifies a collation.
    *
    * This option is sent only if the caller explicitly provides a value.
    */
   const bson_t *collation; // may be NULL

   /**
    * The index to use. Specify either the index name as a string or the index
    * key pattern. If specified, then the query system will only consider plans
    * using the hinted index.
    *
    * This option is only sent if the caller explicitly provides a value.
    */
   const bson_value_t *hint; // may be NULL

   /**
    * When true, creates a new document if no document matches the query.
    * Defaults to false.
    *
    * This options is sent only if the caller explicitly provides a value.
    */
   mongoc_opt_bool_t upsert;
} mongoc_updateone_model_t;

BSON_EXPORT (bool)
mongoc_listof_bulkwritemodel_append_updateone (mongoc_listof_bulkwritemodel_t *self,
                                               const char *namespace,
                                               int namespace_len,
                                               mongoc_updateone_model_t model,
                                               bson_error_t *error);

typedef struct {
   /**
    * The filter to apply.
    */
   const bson_t *filter;
   /**
    * The update document or pipeline to apply to the selected document.
    */

   const bson_t *update;

   /**
    * A set of filters specifying to which array elements an update should
    * apply.
    *
    * This option is sent only if the caller explicitly provides a value.
    */
   const bson_t *arrayFilters; // may be NULL

   /**
    * Specifies a collation.
    *
    * This option is sent only if the caller explicitly provides a value.
    */
   const bson_t *collation; // may be NULL

   /**
    * The index to use. Specify either the index name as a string or the index
    * key pattern. If specified, then the query system will only consider plans
    * using the hinted index.
    *
    * This option is only sent if the caller explicitly provides a value.
    */
   const bson_value_t *hint; // may be NULL

   /**
    * When true, creates a new document if no document matches the query.
    * Defaults to false.
    *
    * This options is sent only if the caller explicitly provides a value.
    */
   mongoc_opt_bool_t upsert;
} mongoc_updatemany_model_t;

BSON_EXPORT (bool)
mongoc_listof_bulkwritemodel_append_updatemany (mongoc_listof_bulkwritemodel_t *self,
                                                const char *namespace,
                                                int namespace_len,
                                                mongoc_updatemany_model_t model,
                                                bson_error_t *error);

typedef struct {
   /**
    * The filter to apply.
    */
   const bson_t *filter;
   /**
    * The replacement document.
    */
   const bson_t *replacement;

   /**
    * A set of filters specifying to which array elements an update should
    * apply.
    *
    * This option is sent only if the caller explicitly provides a value.
    */
   const bson_t *arrayFilters; // may be NULL

   /**
    * Specifies a collation.
    *
    * This option is sent only if the caller explicitly provides a value.
    */
   const bson_t *collation; // may be NULL

   /**
    * The index to use. Specify either the index name as a string or the index
    * key pattern. If specified, then the query system will only consider plans
    * using the hinted index.
    *
    * This option is only sent if the caller explicitly provides a value.
    */
   const bson_value_t *hint; // may be NULL

   /**
    * When true, creates a new document if no document matches the query.
    * Defaults to false.
    *
    * This options is sent only if the caller explicitly provides a value.
    */
   mongoc_opt_bool_t upsert;
} mongoc_replaceone_model_t;

BSON_EXPORT (bool)
mongoc_listof_bulkwritemodel_append_replaceone (mongoc_listof_bulkwritemodel_t *self,
                                                const char *namespace,
                                                int namespace_len,
                                                mongoc_replaceone_model_t model,
                                                bson_error_t *error);

typedef struct {
   /**
    * The filter to apply.
    */
   const bson_t *filter;

   /**
    * Specifies a collation.
    *
    * This option is sent only if the caller explicitly provides a value.
    */
   const bson_t *collation; // may be NULL

   /**
    * The index to use. Specify either the index name as a string or the index
    * key pattern. If specified, then the query system will only consider plans
    * using the hinted index.
    *
    * This option is only sent if the caller explicitly provides a value.
    */
   const bson_value_t *hint; // may be NULL
} mongoc_deleteone_model_t;

BSON_EXPORT (bool)
mongoc_listof_bulkwritemodel_append_deleteone (mongoc_listof_bulkwritemodel_t *self,
                                               const char *namespace,
                                               int namespace_len,
                                               mongoc_deleteone_model_t model,
                                               bson_error_t *error);

typedef struct {
   /**
    * The filter to apply.
    */
   const bson_t *filter;

   /**
    * Specifies a collation.
    *
    * This option is sent only if the caller explicitly provides a value.
    */
   const bson_t *collation; // may be NULL

   /**
    * The index to use. Specify either the index name as a string or the index
    * key pattern. If specified, then the query system will only consider plans
    * using the hinted index.
    *
    * This option is only sent if the caller explicitly provides a value.
    */
   const bson_value_t *hint; // may be NULL
} mongoc_deletemany_model_t;

BSON_EXPORT (bool)
mongoc_listof_bulkwritemodel_append_deletemany (mongoc_listof_bulkwritemodel_t *self,
                                                const char *namespace,
                                                int namespace_len,
                                                mongoc_deletemany_model_t model,
                                                bson_error_t *error);

struct _mongoc_bulkwriteoptions_t {
   /**
    * Whether the operations in this bulk write should be executed in the order
    * in which they were specified. If false, writes will continue to be
    * executed if an individual write fails. If true, writes will stop executing
    * if an individual write fails.
    *
    * Defaults to false.
    */
   mongoc_opt_bool_t ordered;

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
   const bson_t *let;

   /**
    * The write concern to use for this bulk write.
    */
   // May be NULL.
   const mongoc_write_concern_t *writeConcern;

   /**
    * Whether detailed results for each successful operation should be included
    * in the returned BulkWriteResult.
    *
    * Defaults to false.
    */
   bool verboseResults;

   /**
    * Enables users to specify an arbitrary comment to help trace the operation
    * through the database profiler, currentOp and logs. The default is to not
    * send a value.
    */
   const bson_t *comment;

   // An optional session. May be NULL.
   mongoc_client_session_t *session;
};

typedef struct _mongoc_mapof_insertoneresult_t mongoc_mapof_insertoneresult_t;
typedef struct _mongoc_insertoneresult_t mongoc_insertoneresult_t;
BSON_EXPORT (const mongoc_insertoneresult_t *)
mongoc_mapof_insertoneresult_lookup (const mongoc_mapof_insertoneresult_t *self, size_t idx);

typedef struct _mongoc_mapof_updateresult_t mongoc_mapof_updateresult_t;
typedef struct _mongoc_updateresult_t mongoc_updateresult_t;
BSON_EXPORT (const mongoc_updateresult_t *)
mongoc_mapof_updateresult_lookup (const mongoc_mapof_updateresult_t *self, size_t idx);

typedef struct _mongoc_mapof_deleteresult_t mongoc_mapof_deleteresult_t;
typedef struct _mongoc_deleteresult_t mongoc_deleteresult_t;
BSON_EXPORT (const mongoc_deleteresult_t *)
mongoc_mapof_deleteresult_lookup (const mongoc_mapof_deleteresult_t *self, size_t idx);

/**
 * Indicates whether the results are verbose. If false, the insertResults,
 * updateResults, and deleteResults fields in this result will be undefined.
 */
BSON_EXPORT (bool)
mongoc_bulkwriteresult_hasVerboseResults (const mongoc_bulkwriteresult_t *self);

/**
 * The total number of documents inserted across all insert operations.
 */
BSON_EXPORT (int64_t)
mongoc_bulkwriteresult_insertedCount (const mongoc_bulkwriteresult_t *self);

/**
 * The total number of documents upserted across all update operations.
 */
BSON_EXPORT (int64_t)
mongoc_bulkwriteresult_upsertedCount (const mongoc_bulkwriteresult_t *self);

/**
 * The total number of documents matched across all update operations.
 */
BSON_EXPORT (int64_t)
mongoc_bulkwriteresult_matchedCount (const mongoc_bulkwriteresult_t *self);

/**
 * The total number of documents modified across all update operations.
 */
BSON_EXPORT (int64_t)
mongoc_bulkwriteresult_modifiedCount (const mongoc_bulkwriteresult_t *self);

/**
 * The total number of documents deleted across all delete operations.
 */
BSON_EXPORT (int64_t)
mongoc_bulkwriteresult_deletedCount (const mongoc_bulkwriteresult_t *self);

/**
 * The results of each individual insert operation that was successfully
 * performed.
 */
BSON_EXPORT (const mongoc_mapof_insertoneresult_t *)
mongoc_bulkwriteresult_insertResults (const mongoc_bulkwriteresult_t *self);

/**
 * The results of each individual update operation that was successfully
 * performed.
 */
BSON_EXPORT (const mongoc_mapof_updateresult_t *)
mongoc_bulkwriteresult_updateResult (const mongoc_bulkwriteresult_t *self);

/**
 * The results of each individual delete operation that was successfully
 * performed.
 */
BSON_EXPORT (const mongoc_mapof_deleteresult_t *)
mongoc_bulkwriteresult_deleteResults (const mongoc_bulkwriteresult_t *self);

/**
 * The _id of the inserted document.
 */
BSON_EXPORT (const bson_value_t *)
mongoc_insertoneresult_inserted_id (const mongoc_insertoneresult_t *self);

/**
 * The number of documents that matched the filter.
 */
BSON_EXPORT (int64_t)
mongoc_updateresult_matchedCount (const mongoc_updateresult_t *self);

/**
 * The number of documents that were modified.
 */
BSON_EXPORT (int64_t)
mongoc_updateresult_modifiedCount (const mongoc_updateresult_t *self);

/**
 * The _id field of the upserted document if an upsert occurred.
 */
// May be NULL if no upsert occurred.
BSON_EXPORT (const bson_value_t *)
mongoc_updateresult_upsertedId (const mongoc_updateresult_t *self);

/**
 * The number of documents that were deleted.
 */
BSON_EXPORT (int64_t)
mongoc_deleteresult_deletedCount (const mongoc_deleteresult_t *self);

typedef struct _mongoc_listof_writeconcernerror_t mongoc_listof_writeconcernerror_t;
typedef struct _mongoc_writeconcernerror_t mongoc_writeconcernerror_t;

BSON_EXPORT (const mongoc_writeconcernerror_t *)
mongoc_listof_writeconcernerror_at (const mongoc_listof_writeconcernerror_t *self, size_t idx);

BSON_EXPORT (size_t)
mongoc_listof_writeconcernerror_len (const mongoc_listof_writeconcernerror_t *self);

typedef struct _mongoc_mapof_writeerror_t mongoc_mapof_writeerror_t;
typedef struct _mongoc_writeerror_t mongoc_writeerror_t;

BSON_EXPORT (const mongoc_writeerror_t *)
mongoc_mapof_writeerror_lookup (const mongoc_mapof_writeerror_t *self, size_t idx);

/**
 * A top-level error that occurred when attempting to communicate with the
 * server or execute the bulk write. This value may not be populated if the
 * exception was thrown due to errors occurring on individual writes.
 */
// Returns false if no error is set.
BSON_EXPORT (bool)
mongoc_bulkwriteexception_error (const mongoc_bulkwriteexception_t *self,
                                 bson_error_t *error,
                                 const bson_t **error_document);

typedef struct _mongoc_listof_errorlabel_t mongoc_listof_errorlabel_t;

BSON_EXPORT (const mongoc_listof_errorlabel_t *)
mongoc_bulkwriteexception_errorLabels (const mongoc_bulkwriteexception_t *self);

BSON_EXPORT (const char *)
mongoc_listof_errorlabel_at (const mongoc_listof_errorlabel_t *self, size_t idx);

BSON_EXPORT (size_t)
mongoc_listof_errorlabel_len (const mongoc_listof_errorlabel_t *self);


/**
 * Write concern errors that occurred while executing the bulk write. This
 * list may have multiple items if more than one server command was required
 * to execute the bulk write.
 */
BSON_EXPORT (const mongoc_listof_writeconcernerror_t *)
mongoc_bulkwriteexception_writeConcernErrors (const mongoc_bulkwriteexception_t *self);

/**
 * Errors that occurred during the execution of individual write operations.
 * This map will contain at most one entry if the bulk write was ordered.
 */
BSON_EXPORT (const mongoc_mapof_writeerror_t *)
mongoc_bulkwriteexception_writeErrors (const mongoc_bulkwriteexception_t *self);

// Returns the server error reply for a command error (`ok: 0`)
BSON_EXPORT (const bson_t *)
mongoc_bulkwriteexception_errorReply (const mongoc_bulkwriteexception_t *self);

/**
 * An integer value identifying the write concern error. Corresponds to
 * the "writeConcernError.code" field in the command response.
 *
 * @see https://www.mongodb.com/docs/manual/reference/method/WriteResult/
 */
BSON_EXPORT (int32_t)
mongoc_writeconcernerror_code (const mongoc_writeconcernerror_t *self);

/**
 * A document identifying the write concern setting related to the error.
 * Corresponds to the "writeConcernError.errInfo" field in the command
 * response.
 *
 * @see https://www.mongodb.com/docs/manual/reference/method/WriteResult/
 */
// May be NULL.
BSON_EXPORT (const bson_t *)
mongoc_writeconcernerror_details (const mongoc_writeconcernerror_t *self);

/**
 * A description of the error. Corresponds to the
 * "writeConcernError.errmsg" field in the command response.
 *
 * @see https://www.mongodb.com/docs/manual/reference/method/WriteResult/
 */
BSON_EXPORT (const char *)
mongoc_writeconcernerror_message (const mongoc_writeconcernerror_t *self);

/**
 * An integer value identifying the write error. Corresponds to the
 * "writeErrors[].code" field in the command response.
 *
 * @see https://www.mongodb.com/docs/manual/reference/method/WriteResult/
 */
BSON_EXPORT (int32_t)
mongoc_writeerror_code (const mongoc_writeerror_t *self);

/**
 * A document providing more information about the write error (e.g.
 * details pertaining to document validation). Corresponds to the
 * "writeErrors[].errInfo" field in the command response.
 *
 * @see https://www.mongodb.com/docs/manual/reference/method/WriteResult/
 */
BSON_EXPORT (const bson_t *)
mongoc_writeerror_details (const mongoc_writeerror_t *self);

/**
 * A description of the error. Corresponds to the "writeErrors[].errmsg"
 * field in the command response.
 *
 * @see https://www.mongodb.com/docs/manual/reference/method/WriteResult/
 */
BSON_EXPORT (const char *)
mongoc_writeerror_message (const mongoc_writeerror_t *self);

BSON_END_DECLS

#endif // MONGOC_BULKWRITE_H
