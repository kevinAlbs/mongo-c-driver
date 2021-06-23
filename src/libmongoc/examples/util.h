#ifndef EXAMPLES_UTIL
#define EXAMPLES_UTIL

#include <mongoc/mongoc.h>

#define DIE(...) MONGOC_ERROR(__VA_ARGS__); abort()

mongoc_apm_callbacks_t * util_log_callbacks_new (void);

/* If a config.json file exists in the working directory, reads JSON.
 * Parses command line args. */
bson_t *util_args_parse (int argc, char** argv);

bool util_args_eq (bson_t* args, const char* key, const char* val);

const char* util_args_get (bson_t* args, const char* key);

#endif /* EXAMPLES_UTIL */
