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

#include "mongoc-prelude.h"

#ifndef MONGOC_BULKWRITE_H
#define MONGOC_BULKWRITE_H

#include <mongoc-client.h>
#include <mongoc-write-concern.h>

BSON_BEGIN_DECLS

typedef struct _mongoc_bulkwriteopts_t mongoc_bulkwriteopts_t;
BSON_EXPORT (mongoc_bulkwriteopts_t *)
mongoc_bulkwriteopts_new (void);
BSON_EXPORT (void)
mongoc_bulkwriteopts_set_ordered (mongoc_bulkwriteopts_t *self, bool ordered);
BSON_EXPORT (void)
mongoc_bulkwriteopts_set_bypassdocumentvalidation (mongoc_bulkwriteopts_t *self, bool bypassdocumentvalidation);
BSON_EXPORT (void)
mongoc_bulkwriteopts_set_let (mongoc_bulkwriteopts_t *self, const bson_t *let);
BSON_EXPORT (void)
mongoc_bulkwriteopts_set_writeconcern (mongoc_bulkwriteopts_t *self, const mongoc_write_concern_t *writeconcern);
BSON_EXPORT (void)
mongoc_bulkwriteopts_set_verboseresults (mongoc_bulkwriteopts_t *self, bool verboseresults);
BSON_EXPORT (void)
mongoc_bulkwriteopts_set_comment (mongoc_bulkwriteopts_t *self, const bson_t *comment);
// `mongoc_bulkwriteopts_set_session` does not copy `session`. The caller must ensure `session` outlives `self`.
BSON_EXPORT (void)
mongoc_bulkwriteopts_set_session (mongoc_bulkwriteopts_t *self, mongoc_client_session_t *session);
// `mongoc_bulkwriteopts_set_extra` appends `extra` to bulkWrite command.
// It is intended to support future server options.
BSON_EXPORT (void)
mongoc_bulkwriteopts_set_extra (mongoc_bulkwriteopts_t *self, const bson_t *extra);
// `mongoc_bulkwriteopts_set_serverid` identifies which server to perform the operation. This is intended for use by
// wrapping drivers that select a server before running the operation.
BSON_EXPORT (void)
mongoc_bulkwriteopts_set_serverid (mongoc_bulkwriteopts_t *self, uint32_t serverid);
BSON_EXPORT (void)
mongoc_bulkwriteopts_destroy (mongoc_bulkwriteopts_t *self);

typedef struct _mongoc_bulkwriteresult_t mongoc_bulkwriteresult_t;
BSON_EXPORT (bool)
mongoc_bulkwriteresult_acknowledged (const mongoc_bulkwriteresult_t *self);
BSON_EXPORT (int64_t)
mongoc_bulkwriteresult_insertedcount (const mongoc_bulkwriteresult_t *self);
BSON_EXPORT (int64_t)
mongoc_bulkwriteresult_upsertedcount (const mongoc_bulkwriteresult_t *self);
BSON_EXPORT (int64_t)
mongoc_bulkwriteresult_matchedcount (const mongoc_bulkwriteresult_t *self);
BSON_EXPORT (int64_t)
mongoc_bulkwriteresult_modifiedcount (const mongoc_bulkwriteresult_t *self);
BSON_EXPORT (int64_t)
mongoc_bulkwriteresult_deletedcount (const mongoc_bulkwriteresult_t *self);
// `mongoc_bulkwriteresult_insertresults` returns a BSON document mapping model indexes to insert results.
BSON_EXPORT (const bson_t *)
mongoc_bulkwriteresult_insertresults (const mongoc_bulkwriteresult_t *self);
// `mongoc_bulkwriteresult_updateresults` returns a BSON document mapping model indexes to update results.
BSON_EXPORT (const bson_t *)
mongoc_bulkwriteresult_updateresults (const mongoc_bulkwriteresult_t *self);
// `mongoc_bulkwriteresult_deleteresults` returns a BSON document mapping model indexes to delete results.
BSON_EXPORT (const bson_t *)
mongoc_bulkwriteresult_deleteresults (const mongoc_bulkwriteresult_t *self);
// `mongoc_bulkwriteresult_get_serverid` identifies which server to performed the operation. This may differ from a
// previously set serverid if a retry occurred. This is intended for use by wrapping drivers that select a server before
// running the operation.
BSON_EXPORT (uint32_t)
mongoc_bulkwriteresult_serverid (const mongoc_bulkwriteresult_t *self);
BSON_EXPORT (void)
mongoc_bulkwriteresult_destroy (mongoc_bulkwriteresult_t *self);

typedef struct _mongoc_bulkwriteexception_t mongoc_bulkwriteexception_t;
// Returns true if there was a top-level error.
BSON_EXPORT (bool)
mongoc_bulkwriteexception_error (const mongoc_bulkwriteexception_t *self, bson_error_t *error);
// `mongoc_bulkwriteexception_writeerrors` returns a BSON document mapping model indexes to write errors.
BSON_EXPORT (const bson_t *)
mongoc_bulkwriteexception_writeerrors (const mongoc_bulkwriteexception_t *self);
// `mongoc_bulkwriteexception_writeconcernerrors` returns a BSON array of write concern errors.
BSON_EXPORT (const bson_t *)
mongoc_bulkwriteexception_writeconcernerrors (const mongoc_bulkwriteexception_t *self);
// `mongoc_bulkwriteexception_errorreply` returns a possible server reply related to the error, or an empty document.
BSON_EXPORT (const bson_t *)
mongoc_bulkwriteexception_errorreply (const mongoc_bulkwriteexception_t *self);
BSON_EXPORT (void)
mongoc_bulkwriteexception_destroy (mongoc_bulkwriteexception_t *self);

typedef struct _mongoc_bulkwrite_t mongoc_bulkwrite_t;
BSON_EXPORT (mongoc_bulkwrite_t *)
mongoc_client_bulkwrite_new (mongoc_client_t *self);
typedef struct _mongoc_insertoneopts_t mongoc_insertoneopts_t;
BSON_EXPORT (mongoc_insertoneopts_t *)
mongoc_insertoneopts_new (void);
BSON_EXPORT (void)
mongoc_insertoneopts_destroy (mongoc_insertoneopts_t *self);
BSON_EXPORT (bool)
mongoc_bulkwrite_append_insertone (mongoc_bulkwrite_t *self,
                                   const char *ns,
                                   int ns_len,
                                   const bson_t *document,
                                   mongoc_insertoneopts_t *opts /* May be NULL */,
                                   bson_error_t *error);

typedef struct _mongoc_updateoneopts_t mongoc_updateoneopts_t;
BSON_EXPORT (mongoc_updateoneopts_t *)
mongoc_updateoneopts_new (void);
BSON_EXPORT (void)
mongoc_updateoneopts_set_arrayfilters (mongoc_updateoneopts_t *self, const bson_t *arrayfilters);
BSON_EXPORT (void)
mongoc_updateoneopts_set_collation (mongoc_updateoneopts_t *self, const bson_t *collation);
BSON_EXPORT (void)
mongoc_updateoneopts_set_hint (mongoc_updateoneopts_t *self, const bson_value_t *hint);
BSON_EXPORT (void)
mongoc_updateoneopts_set_upsert (mongoc_updateoneopts_t *self, bool upsert);
BSON_EXPORT (void)
mongoc_updateoneopts_destroy (mongoc_updateoneopts_t *self);
BSON_EXPORT (bool)
mongoc_bulkwrite_append_updateone (mongoc_bulkwrite_t *self,
                                   const char *ns,
                                   int ns_len,
                                   const bson_t *filter,
                                   const bson_t *update,
                                   mongoc_updateoneopts_t *opts /* May be NULL */,
                                   bson_error_t *error);

typedef struct _mongoc_updatemanyopts_t mongoc_updatemanyopts_t;
BSON_EXPORT (mongoc_updatemanyopts_t *)
mongoc_updatemanyopts_new (void);
BSON_EXPORT (void)
mongoc_updatemanyopts_set_arrayfilters (mongoc_updatemanyopts_t *self, const bson_t *arrayfilters);
BSON_EXPORT (void)
mongoc_updatemanyopts_set_collation (mongoc_updatemanyopts_t *self, const bson_t *collation);
BSON_EXPORT (void)
mongoc_updatemanyopts_set_hint (mongoc_updatemanyopts_t *self, const bson_value_t *hint);
BSON_EXPORT (void)
mongoc_updatemanyopts_set_upsert (mongoc_updatemanyopts_t *self, bool upsert);
BSON_EXPORT (void)
mongoc_updatemanyopts_destroy (mongoc_updatemanyopts_t *self);
BSON_EXPORT (bool)
mongoc_bulkwrite_append_updatemany (mongoc_bulkwrite_t *self,
                                    const char *ns,
                                    int ns_len,
                                    const bson_t *filter,
                                    const bson_t *update,
                                    mongoc_updatemanyopts_t *opts /* May be NULL */,
                                    bson_error_t *error);

typedef struct _mongoc_replaceoneopts_t mongoc_replaceoneopts_t;
BSON_EXPORT (mongoc_replaceoneopts_t *)
mongoc_replaceoneopts_new (void);
BSON_EXPORT (void)
mongoc_replaceoneopts_set_arrayfilters (mongoc_replaceoneopts_t *self, const bson_t *arrayfilters);
BSON_EXPORT (void)
mongoc_replaceoneopts_set_collation (mongoc_replaceoneopts_t *self, const bson_t *collation);
BSON_EXPORT (void)
mongoc_replaceoneopts_set_hint (mongoc_replaceoneopts_t *self, const bson_value_t *hint);
BSON_EXPORT (void)
mongoc_replaceoneopts_set_upsert (mongoc_replaceoneopts_t *self, bool upsert);
BSON_EXPORT (void)
mongoc_replaceoneopts_destroy (mongoc_replaceoneopts_t *self);
BSON_EXPORT (bool)
mongoc_bulkwrite_append_replaceone (mongoc_bulkwrite_t *self,
                                    const char *ns,
                                    int ns_len,
                                    const bson_t *filter,
                                    const bson_t *replacement,
                                    mongoc_replaceoneopts_t *opts /* May be NULL */,
                                    bson_error_t *error);

typedef struct _mongoc_deleteoneopts_t mongoc_deleteoneopts_t;
BSON_EXPORT (mongoc_deleteoneopts_t *)
mongoc_deleteoneopts_new (void);
BSON_EXPORT (void)
mongoc_deleteoneopts_set_collation (mongoc_deleteoneopts_t *self, const bson_t *collation);
BSON_EXPORT (void)
mongoc_deleteoneopts_set_hint (mongoc_deleteoneopts_t *self, const bson_value_t *hint);
BSON_EXPORT (void)
mongoc_deleteoneopts_destroy (mongoc_deleteoneopts_t *self);
BSON_EXPORT (bool)
mongoc_bulkwrite_append_deleteone (mongoc_bulkwrite_t *self,
                                   const char *ns,
                                   int ns_len,
                                   const bson_t *filter,
                                   mongoc_deleteoneopts_t *opts /* May be NULL */,
                                   bson_error_t *error);

typedef struct _mongoc_deletemanyopts_t mongoc_deletemanyopts_t;
BSON_EXPORT (mongoc_deletemanyopts_t *)
mongoc_deletemanyopts_new (void);
BSON_EXPORT (void)
mongoc_deletemanyopts_set_collation (mongoc_deletemanyopts_t *self, const bson_t *collation);
BSON_EXPORT (void)
mongoc_deletemanyopts_set_hint (mongoc_deletemanyopts_t *self, const bson_value_t *hint);
BSON_EXPORT (void)
mongoc_deletemanyopts_destroy (mongoc_deletemanyopts_t *self);
BSON_EXPORT (bool)
mongoc_bulkwrite_append_deletemany (mongoc_bulkwrite_t *self,
                                    const char *ns,
                                    int ns_len,
                                    const bson_t *filter,
                                    mongoc_deletemanyopts_t *opts /* May be NULL */,
                                    bson_error_t *error);


// `mongoc_bulkwritereturn_t` may outlive `mongoc_bulkwrite_t`.
typedef struct {
   mongoc_bulkwriteresult_t *res;    // May be NULL
   mongoc_bulkwriteexception_t *exc; // May be NULL
} mongoc_bulkwritereturn_t;
BSON_EXPORT (mongoc_bulkwritereturn_t)
mongoc_bulkwrite_execute (mongoc_bulkwrite_t *self, mongoc_bulkwriteopts_t *opts);
BSON_EXPORT (void)
mongoc_bulkwrite_destroy (mongoc_bulkwrite_t *self);

BSON_END_DECLS

#endif // MONGOC_BULKWRITE_H
