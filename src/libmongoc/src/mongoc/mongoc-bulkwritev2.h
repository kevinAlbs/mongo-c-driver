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

#ifndef MONGOC_BULKWRITE_V2_H
#define MONGOC_BULKWRITE_V2_H

#include <mongoc-optional.h>
#include <mongoc-client.h>
#include <mongoc-write-concern.h>

BSON_BEGIN_DECLS

typedef struct _mongoc_bulkwritev2_t mongoc_bulkwritev2_t;
typedef struct _mongoc_bulkwritereturnv2_t mongoc_bulkwritereturnv2_t;

typedef struct {
   mongoc_opt_boolv2_t ordered;
   mongoc_opt_boolv2_t bypassDocumentValidation;
   const bson_t *let;                          // May be NULL.
   const mongoc_write_concern_t *writeConcern; // May be NULL.
   mongoc_opt_boolv2_t verboseResults;
   const bson_t *comment;            // May be NULL.
   mongoc_client_session_t *session; // May be NULL.
   // `extra` is appended to the bulkWrite command.
   // It is included to support future server options.
   const bson_t *extra; // May be NULL.
} mongoc_bulkwriteoptionsv2_t;

mongoc_bulkwritev2_t *
mongoc_client_bulkwritev2_new (mongoc_client_t *self,
                               mongoc_bulkwriteoptionsv2_t opts);

typedef struct {
   const bson_t *document;
   // `extra` is appended to the insert operation.
   // It is included to support future server options.
   const bson_t *extra; // may be NULL.
   mongoc_opt_validate_flagsv2_t validate_flags;
} mongoc_insertone_modelv2_t;

bool
mongoc_client_bulkwritev2_append_insertone (mongoc_bulkwritev2_t *self,
                                            const char *namespace,
                                            int namespace_len,
                                            mongoc_insertone_modelv2_t model,
                                            bson_error_t *error);

mongoc_bulkwritereturnv2_t *
mongoc_bulkwritev2_execute (mongoc_bulkwritev2_t *self);

bool
mongoc_bulkwritereturnv2_acknowledged (const mongoc_bulkwritereturnv2_t *self);

int64_t
mongoc_bulkwritereturnv2_insertedCount (const mongoc_bulkwritereturnv2_t *self);

int64_t
mongoc_bulkwritereturnv2_upsertedCount (const mongoc_bulkwritereturnv2_t *self);

int64_t
mongoc_bulkwritereturnv2_matchedCount (const mongoc_bulkwritereturnv2_t *self);

int64_t
mongoc_bulkwritereturnv2_modifiedCount (const mongoc_bulkwritereturnv2_t *self);

int64_t
mongoc_bulkwritereturnv2_deletedCount (const mongoc_bulkwritereturnv2_t *self);

// `mongoc_bulkwritereturnv2_verboseResults` returns NULL if verbose results
// were not requested (default). Otherwise, returns a document with the
// fields: `insertResults`, `updateResult`, `deleteResults`
const bson_t *
mongoc_bulkwritereturnv2_verboseResults (
   const mongoc_bulkwritereturnv2_t *self);

// `mongoc_bulkwritereturnv2_error` returns true if an error occurred.
// If an error occurred, `error_document` is set to a document containing the
// fields: `errorLabels`, `writeConcernErrors`, `writeErrors`, `errorReplies`
bool
mongoc_bulkwritereturnv2_error (const mongoc_bulkwritereturnv2_t *self,
                                bson_error_t *error,
                                const bson_t **error_document);

void
mongoc_bulkwritereturnv2_destroy (mongoc_bulkwritereturnv2_t *self);


void
mongoc_bulkwritev2_destroy (mongoc_bulkwritev2_t *self);

BSON_END_DECLS

#endif // MONGOC_BULKWRITE_V2_H
