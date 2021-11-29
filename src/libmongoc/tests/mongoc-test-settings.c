#include "mongoc-test-settings.h"

/* Should we test to see if we got back an apiVersion field in HELLO? */
bool mongoc_test_settings_legacy_hello_expect_ApiVersion = false;

/* Should we verify that the received opcode matched OPCODE_QUERY? */
bool mongoc_test_check_OPCODE_QUERY = true;
