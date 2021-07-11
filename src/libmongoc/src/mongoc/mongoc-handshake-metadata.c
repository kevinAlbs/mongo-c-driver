/*
 * Copyright 2021-present MongoDB, Inc.
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

#include "mongoc-handshake-metadata-private.h"

mongoc_handshake_metadata_t* mongoc_handshake_metadata_new (const bson_t* hello_response) {
    return NULL;
}

void mongoc_handshake_metadata_destroy (mongoc_handshake_metadata_t* hm) {
    // LBTODO
}

mongoc_handshake_metadata_t* mongoc_handshake_metadata_copy (const mongoc_handshake_metadata_t* val) {
   // LBTODO
}