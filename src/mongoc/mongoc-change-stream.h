// TODO get a copy of the most recent license
#include "mongoc-macros.h"
#include "mongoc-cursor.h"

#ifndef LIBMONGOC_MONGOC_CHANGE_STREAM_H
#define LIBMONGOC_MONGOC_CHANGE_STREAM_H

typedef struct _mongoc_change_stream_t mongoc_change_stream_t;

MONGOC_EXPORT(void)
mongoc_change_stream_destroy(mongoc_change_stream_t*);

MONGOC_EXPORT(bool)
mongoc_change_stream_next(mongoc_change_stream_t*, const bson_t **bson);

MONGOC_EXPORT(bool)
mongoc_change_stream_error(const mongoc_change_stream_t*, bson_error_t*);

// Private, only used by mongoc_collection_watch
mongoc_change_stream_t* _mongoc_change_stream_new(mongoc_cursor_t* cursor, bson_t* pipeline, bson_t* opts);

#endif //LIBMONGOC_MONGOC_CHANGE_STREAM_H
