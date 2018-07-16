/*
 * Copyright 2018 MongoDB, Inc.
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

#ifndef BSON_MD5_PRIVATE_H
#define BSON_MD5_PRIVATE_H


#if !defined(BSON_INSIDE) && !defined(BSON_COMPILATION)
#error "Only <bson.h> can be included directly."
#endif


#include "bson-endian.h"


BSON_BEGIN_DECLS

typedef struct {
   uint32_t count[2]; /* message length in bits, lsw first */
   uint32_t abcd[4];  /* digest buffer */
   uint8_t buf[64];   /* accumulate block */
} _bson_md5_t;

void
_bson_md5_init (_bson_md5_t *pms);
void
_bson_md5_append (_bson_md5_t *pms,
                 const uint8_t *data,
                 uint32_t nbytes);
void
_bson_md5_finish (_bson_md5_t *pms, uint8_t digest[16]);


BSON_END_DECLS


#endif /* BSON_MD5_PRIVATE_H */
