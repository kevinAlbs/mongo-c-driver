/*
 * Copyright 2021 MongoDB, Inc.
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

#ifndef MONGOC_OPTIONAL_H
#define MONGOC_OPTIONAL_H

#include <bson/bson.h>

#include "mongoc-macros.h"

BSON_BEGIN_DECLS

#define MONGOC_BULKWRITEOPTIONSV2_NONE \
   (mongoc_bulkwriteoptionsv2_t)       \
   {                                   \
      0                                \
   }

typedef struct {
   bool value;
   bool isset;
} mongoc_opt_boolv2_t;

#define MONGOC_OPT_BOOLV2_TRUE     \
   (mongoc_opt_boolv2_t)           \
   {                               \
      .value = true, .isset = true \
   }
#define MONGOC_OPT_BOOLV2_FALSE     \
   (mongoc_opt_boolv2_t)            \
   {                                \
      .value = false, .isset = true \
   }
#define MONGOC_OPT_BOOLV2_UNSET \
   (mongoc_opt_boolv2_t)        \
   {                            \
      .isset = false            \
   }

typedef struct {
   bson_validate_flags_t value;
   bool isset;
} mongoc_opt_validate_flagsv2_t;

#define MONGOC_OPT_VALIDATE_FLAGSV2_VAL(val) \
   (mongoc_opt_validate_flagsv2_t)           \
   {                                         \
      .value = val, .isset = true            \
   }
#define MONGOC_OPT_VALIDATE_FLAGSV2_UNSET \
   (mongoc_opt_validate_flagsv2_t)        \
   {                                      \
      .isset = false                      \
   }


typedef struct {
   bool value;
   bool is_set;
} mongoc_optional_t;

MONGOC_EXPORT (void)
mongoc_optional_init (mongoc_optional_t *opt);

MONGOC_EXPORT (bool)
mongoc_optional_is_set (const mongoc_optional_t *opt);

MONGOC_EXPORT (bool)
mongoc_optional_value (const mongoc_optional_t *opt);

MONGOC_EXPORT (void)
mongoc_optional_set_value (mongoc_optional_t *opt, bool val);

MONGOC_EXPORT (void)
mongoc_optional_copy (const mongoc_optional_t *source, mongoc_optional_t *copy);

BSON_END_DECLS

#endif /* MONGOC_OPTIONAL_H */
