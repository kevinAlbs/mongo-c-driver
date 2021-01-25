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

typedef bool (*special_fn) (const bson_t *assertion,
                            bson_val_t *actual,
                            void *hook_ctx,
                            const char *path,
                            bson_error_t *error);

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
