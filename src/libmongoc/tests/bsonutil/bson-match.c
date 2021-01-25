/*
 * Copyright 2020-present MongoDB, Inc.
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

#include "bsonutil/bson-match.h"
#include "test-conveniences.h"
#include "TestSuite.h"
#include "unified/util.h"

#define MATCH_ERR(format, ...) \
   test_set_error (error, "match error at '%s': " format, path, __VA_ARGS__)

static const char *
get_first_key (const bson_t *bson)
{
   bson_iter_t iter;

   bson_iter_init (&iter, bson);
   if (!bson_iter_next (&iter)) {
      return "";
   }

   return bson_iter_key (&iter);
}

static bool
is_special_match (const bson_t *bson)
{
   const char *first_key = get_first_key (bson);
   if (strstr (first_key, "$$") != first_key) {
      return false;
   }
   if (bson_count_keys (bson) != 1) {
      return false;
   }
   return true;
}

/* actual may be NULL */
static bool
evaluate_special_match (const bson_t *assertion,
                        bson_val_t *actual,
                        special_fn hook,
                        void *hook_ctx,
                        const char *path,
                        bson_error_t *error)
{
   bson_iter_t iter;
   const char *assertion_key;
   bool ret = false;

   bson_iter_init (&iter, assertion);
   BSON_ASSERT (bson_iter_next (&iter));
   assertion_key = bson_iter_key (&iter);

   if (0 == strcmp (assertion_key, "$$exists")) {
      bool should_exist;

      if (!BSON_ITER_HOLDS_BOOL (&iter)) {
         MATCH_ERR ("%s", "unexpected non-bool $$exists assertion");
      }
      should_exist = bson_iter_bool (&iter);

      if (should_exist && NULL == actual) {
         MATCH_ERR ("%s", "should exist but does not");
         goto done;
      }

      if (!should_exist && NULL != actual) {
         MATCH_ERR ("%s", "should not exist but does");
         goto done;
      }

      ret = true;
      goto done;
   }

   if (0 == strcmp (assertion_key, "$$type")) {
      if (!actual) {
         MATCH_ERR ("%s", "does not exist but should");
         goto done;
      }

      if (BSON_ITER_HOLDS_UTF8 (&iter)) {
         bson_type_t expected_type =
            bson_type_from_string (bson_iter_utf8 (&iter, NULL));
         if (expected_type != bson_val_type (actual)) {
            MATCH_ERR ("expected type: %s, got: %s",
                       bson_type_to_string (expected_type),
                       bson_type_to_string (bson_val_type (actual)));
            goto done;
         }
      }

      if (BSON_ITER_HOLDS_ARRAY (&iter)) {
         bson_t arr;
         bson_iter_t arriter;
         bool found = false;

         bson_iter_bson (&iter, &arr);
         BSON_FOREACH (&arr, arriter)
         {
            bson_type_t expected_type;

            if (!BSON_ITER_HOLDS_UTF8 (&arriter)) {
               MATCH_ERR ("%s", "unexpected non-UTF8 $$type assertion");
               goto done;
            }

            expected_type =
               bson_type_from_string (bson_iter_utf8 (&arriter, NULL));
            if (expected_type == bson_val_type (actual)) {
               found = true;
               break;
            }
         }
         if (!found) {
            MATCH_ERR ("expected one of type: %s, got %s",
                       tmp_json (&arr),
                       bson_type_to_string (bson_val_type (actual)));
            goto done;
         }
      }

      ret = true;
      goto done;
   }

   if (0 == strcmp (assertion_key, "$$unsetOrMatches")) {
      bson_val_t *assertion_val = NULL;

      if (actual == NULL) {
         ret = true;
         goto done;
      }

      assertion_val = bson_val_from_iter (&iter);
      ret = bson_match_with_path (
         assertion_val, actual, hook, hook_ctx, path, error);
      bson_val_destroy (assertion_val);
      goto done;
   }

   if (0 == strcmp (assertion_key, "$$matchesHexBytes")) {
      uint8_t *expected_bytes;
      uint32_t expected_bytes_len;
      uint8_t *actual_bytes;
      uint32_t actual_bytes_len;
      char *expected_bytes_string = NULL;
      char *actual_bytes_string = NULL;

      if (!actual) {
         MATCH_ERR ("%s", "does not exist but should");
         goto done;
      }

      if (!BSON_ITER_HOLDS_UTF8 (&iter)) {
         MATCH_ERR ("%s", "$$matchesHexBytes does not contain utf8");
         goto done;
      }

      if (bson_val_type (actual) != BSON_TYPE_BINARY) {
         MATCH_ERR ("%s", "value does not contain binary");
         goto done;
      }

      expected_bytes =
         hex_to_bin (bson_iter_utf8 (&iter, NULL), &expected_bytes_len);
      actual_bytes = bson_val_to_binary (actual, &actual_bytes_len);
      expected_bytes_string = bin_to_hex (expected_bytes, expected_bytes_len);
      actual_bytes_string = bin_to_hex (actual_bytes, actual_bytes_len);

      if (expected_bytes_len != actual_bytes_len) {
         MATCH_ERR ("expected %" PRIu32 " (%s) but got %" PRIu32 " (%s) bytes",
                    expected_bytes_len,
                    expected_bytes_string,
                    actual_bytes_len,
                    actual_bytes_string);
         bson_free (expected_bytes);
         bson_free (expected_bytes_string);
         bson_free (actual_bytes_string);
         goto done;
      }

      if (0 != memcmp (expected_bytes, actual_bytes, expected_bytes_len)) {
         MATCH_ERR ("expected %s, but got %s",
                    expected_bytes_string,
                    actual_bytes_string);
         bson_free (expected_bytes);
         bson_free (expected_bytes_string);
         bson_free (actual_bytes_string);
         goto done;
      }

      bson_free (expected_bytes);
      bson_free (expected_bytes_string);
      bson_free (actual_bytes_string);

      ret = true;
      goto done;
   }

   if (hook) {
      ret = hook (assertion, actual, hook_ctx, path, error);
      goto done;
   }

   MATCH_ERR ("unrecognized special operator: %s", assertion_key);
   goto done;

   ret = true;
done:
   return ret;
}

bool
bson_match_with_path (bson_val_t *expected,
                      bson_val_t *actual,
                      special_fn hook,
                      void *hook_ctx,
                      const char *path,
                      bson_error_t *error)
{
   bool ret = false;
   bool is_root = (0 == strcmp (path, ""));

   if (bson_val_type (expected) == BSON_TYPE_DOCUMENT) {
      bson_iter_t expected_iter;
      bson_t *expected_bson = bson_val_to_document (expected);
      bson_t *actual_bson = NULL;

      /* handle special operators (e.g. $$type) */
      if (is_special_match (expected_bson)) {
         ret = evaluate_special_match (
            expected_bson, actual, hook, hook_ctx, path, error);
         goto done;
      }

      if (bson_val_type (actual) != BSON_TYPE_DOCUMENT) {
         MATCH_ERR ("expected type document, got %s",
                    _mongoc_bson_type_to_str (bson_val_type (actual)));
         goto done;
      }

      actual_bson = bson_val_to_document (actual);

      BSON_FOREACH (expected_bson, expected_iter)
      {
         const char *key;
         bson_val_t *expected_val = NULL;
         bson_val_t *actual_val = NULL;
         bson_iter_t actual_iter;
         char *path_child = NULL;

         key = bson_iter_key (&expected_iter);
         expected_val = bson_val_from_iter (&expected_iter);

         if (bson_iter_init_find (&actual_iter, actual_bson, key)) {
            actual_val = bson_val_from_iter (&actual_iter);
         }

         if (bson_val_type (expected_val) == BSON_TYPE_DOCUMENT &&
             is_special_match (bson_val_to_document (expected_val))) {
            bool special_ret;
            path_child = bson_strdup_printf ("%s.%s", path, key);
            special_ret =
               evaluate_special_match (bson_val_to_document (expected_val),
                                       actual_val,
                                       hook,
                                       hook_ctx,
                                       path,
                                       error);
            bson_free (path_child);
            bson_val_destroy (expected_val);
            bson_val_destroy (actual_val);
            if (!special_ret) {
               goto done;
            }
            continue;
         }

         if (NULL == actual_val) {
            MATCH_ERR ("key %s is not present", key);
            bson_val_destroy (expected_val);
            bson_val_destroy (actual_val);
            goto done;
         }

         path_child = bson_strdup_printf ("%s.%s", path, key);
         if (!bson_match_with_path (
                expected_val, actual_val, hook, hook_ctx, path_child, error)) {
            bson_val_destroy (expected_val);
            bson_val_destroy (actual_val);
            bson_free (path_child);
            goto done;
         }
         bson_val_destroy (expected_val);
         bson_val_destroy (actual_val);
         bson_free (path_child);
      }

      if (!is_root) {
         if (bson_count_keys (expected_bson) < bson_count_keys (actual_bson)) {
            MATCH_ERR ("expected %" PRIu32 " keys in document, got: %" PRIu32,
                       bson_count_keys (expected_bson),
                       bson_count_keys (actual_bson));
            goto done;
         }
      }
      ret = true;
      goto done;
   }

   if (bson_val_type (expected) == BSON_TYPE_ARRAY) {
      bson_iter_t expected_iter;
      bson_t *expected_bson = bson_val_to_array (expected);
      bson_t *actual_bson = NULL;
      char *path_child = NULL;

      if (bson_val_type (actual) != BSON_TYPE_ARRAY) {
         MATCH_ERR ("expected array, but got: %s",
                    _mongoc_bson_type_to_str (bson_val_type (actual)));
         goto done;
      }

      actual_bson = bson_val_to_array (actual);
      if (bson_count_keys (expected_bson) != bson_count_keys (actual_bson)) {
         MATCH_ERR ("expected array of size %" PRIu32
                    ", but got array of size: %" PRIu32,
                    bson_count_keys (expected_bson),
                    bson_count_keys (actual_bson));
         goto done;
      }

      BSON_FOREACH (expected_bson, expected_iter)
      {
         bson_val_t *expected_val = bson_val_from_iter (&expected_iter);
         bson_val_t *actual_val = NULL;
         bson_iter_t actual_iter;
         const char *key;

         key = bson_iter_key (&expected_iter);
         if (!bson_iter_init_find (&actual_iter, actual_bson, key)) {
            MATCH_ERR ("expected array index: %s, but did not exist", key);
            bson_val_destroy (expected_val);
            bson_val_destroy (actual_val);
            goto done;
         }

         actual_val = bson_val_from_iter (&actual_iter);

         path_child = bson_strdup_printf ("%s.%s", path, key);
         if (!bson_match_with_path (
                expected_val, actual_val, hook, hook_ctx, path_child, error)) {
            bson_val_destroy (expected_val);
            bson_val_destroy (actual_val);
            bson_free (path_child);
            goto done;
         }
         bson_val_destroy (expected_val);
         bson_val_destroy (actual_val);
         bson_free (path_child);
      }
      ret = true;
      goto done;
   }

   if (!bson_val_eq (expected, actual, BSON_VAL_FLEXIBLE_NUMERICS)) {
      MATCH_ERR ("value %s != %s",
                 bson_val_to_json (expected),
                 bson_val_to_json (actual));
      goto done;
   }

   ret = true;
done:
   if (!ret && is_root) {
      /* Append the error with more info at the root. */
      bson_error_t tmp_error;

      memcpy (&tmp_error, error, sizeof (bson_error_t));
      test_set_error (error,
                      "BSON match failed: %s\nExpected: %s\nActual: %s",
                      tmp_error.message,
                      bson_val_to_json (expected),
                      bson_val_to_json (actual));
   }
   return ret;
}

bool
bson_match (bson_val_t *expected, bson_val_t *actual, bson_error_t *error)
{
   return bson_match_with_hook (expected, actual, NULL, NULL, error);
}

bool
bson_match_with_hook (bson_val_t *expected,
                      bson_val_t *actual,
                      special_fn hook,
                      void *hook_ctx,
                      bson_error_t *error)
{
   return bson_match_with_path (expected, actual, hook, hook_ctx, "", error);
}

typedef struct {
   const char *desc;
   const char *expected;
   const char *actual;
   bool expect_match;
} testcase_t;

static void
test_match (void)
{
   testcase_t tests[] = {
      {"int32 ==", "{'a': 1}", "{'a': 1}", true},
      {"int32 !=", "{'a': 1}", "{'a': 0}", false},
      {"$$exists", "{'a': {'$$exists': true}}", "{'a': 0}", true},
      {"$$exists fail", "{'a': {'$$exists': true}}", "{'b': 0}", false}};
   int i;

   for (i = 0; i < sizeof (tests) / sizeof (testcase_t); i++) {
      testcase_t *test = tests + i;
      bson_error_t error;
      bson_val_t *expected = bson_val_from_string (test->expected);
      bson_val_t *actual = bson_val_from_string (test->actual);
      bool ret;

      ret = bson_match (expected, actual, &error);
      if (test->expect_match) {
         if (!ret) {
            test_error ("%s: did not match with error: %s, but should have",
                        test->desc,
                        error.message);
         }
      } else {
         if (ret) {
            test_error ("%s: matched, but should not have", test->desc);
         }
      }
      bson_val_destroy (expected);
      bson_val_destroy (actual);
   }
}

void
test_bson_match_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/unified/selftest/bson/match", test_match);
}