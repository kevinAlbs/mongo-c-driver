#ifndef MONGOC_TEST_SETTINGS_H
 #define MONGOC_TEST_SETTINGS_H

/* 
A place for settings that are shared between the test harness, tests, and mock server. 

TODO: rather than free globals, a simple symbol table API:

void *mongoc_test_settings_get(mongoc_test_settings_ctx *ctx, const char *test_name); 

Returns true if old settings were replaced:
bool mongoc_test_settings_register(mongoc_test_settings_ctx *ctx, const char *test_name, void **settings); 

*/

#include <stdbool.h>

extern bool mongoc_test_settings_legacy_hello_expect_ApiVersion;
extern bool mongoc_test_check_OPCODE_QUERY;

#endif
