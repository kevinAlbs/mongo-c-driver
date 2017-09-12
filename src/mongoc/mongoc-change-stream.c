#include <bson.h>

#include "mongoc-change-stream.h"
#include "mongoc-error.h"

struct _mongoc_change_stream_t {
   int x;
};

bool mongoc_change_stream_next(mongoc_change_stream_t* stream, const bson_t **bson) {
   return false;
}

bool mongoc_change_stream_error(const mongoc_change_stream_t* stream, bson_error_t* err) {
   return false;
}

void mongoc_change_stream_destroy(mongoc_change_stream_t* stream) {
   free(stream);
}

mongoc_change_stream_t* _mongoc_change_stream_new(mongoc_cursor_t* cursor, bson_t* pipeline, bson_t* opts)
{
   return (mongoc_change_stream_t*)malloc(sizeof(mongoc_change_stream_t));
}