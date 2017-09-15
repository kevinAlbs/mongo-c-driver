// TODO get a copy of the most recent license
#include "mongoc-macros.h"
#include "mongoc-collection.h"

#ifndef LIBMONGOC_MONGOC_CHANGE_STREAM_H
#define LIBMONGOC_MONGOC_CHANGE_STREAM_H

typedef struct _mongoc_change_stream_t mongoc_change_stream_t;

MONGOC_EXPORT(void)
mongoc_change_stream_destroy(mongoc_change_stream_t*);

MONGOC_EXPORT(bool)
mongoc_change_stream_next(mongoc_change_stream_t*, const bson_t **bson);

MONGOC_EXPORT(bool)
mongoc_change_stream_error_document (const mongoc_change_stream_t *stream,
                                     bson_error_t* err,
                                     const bson_t** bson);

// Private, only used by mongoc_collection_watch
mongoc_change_stream_t* _mongoc_change_stream_new(const mongoc_collection_t* coll, const bson_t* pipeline, const bson_t* opts);

#endif //LIBMONGOC_MONGOC_CHANGE_STREAM_H
