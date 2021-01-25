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

#ifndef UNIFIED_BSON_MATCH_H
#define UNIFIED_BSON_MATCH_H

#include "bsonutil/bson-val.h"

bool
bson_match (bson_val_t *expected, bson_val_t *actual, bson_error_t *error);

typedef struct _bson_matcher_t bson_matcher_t;

bson_matcher_t bson_matcher_new (bson_val_t *expected, bson_val_t *actual, char* path);

/* Add a hook function for matching a special $$ operator */
void bson_matcher_add_special_match (bson_matcher_t* matcher, char* keyword, special_fn special, void* ctx);

bool bson_matcher_match (bson_matcher_t* matcher, bson_error_t *error);

void bson_matcher_destroy (bson_matcher_t *matcher);

typedef bool (*special_fn) (void* ctx, bson_val_t *expected, bson_val_t *actual, char* path, bson_error_t *error);

bool
bson_match_with_path (bson_val_t *expected,
                      bson_val_t *actual,
                      special_fn hook,
                      void *hook_ctx,
                      const char *path,
                      bson_error_t *error);

bool
bson_match_with_hook (bson_val_t *expected,
                      bson_val_t *actual,
                      special_fn hook,
                      void *hook_ctx,
                      bson_error_t *error);

#endif /* UNIFIED_BSON_MATCH_H */