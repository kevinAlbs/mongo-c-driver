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

#ifndef PARALLEL_SINGLE_H
#define PARALLEL_SINGLE_H

#include <mongoc/mongoc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _parallel_single_fixture_t parallel_single_fixture_t;

parallel_single_fixture_t *
parallel_single_fixture_new (void);

void
parallel_single_fixture_destroy (parallel_single_fixture_t *fixture);

bool
parallel_single_fixture_setup (parallel_single_fixture_t *fixture);

bool
parallel_single_fixture_teardown (parallel_single_fixture_t* fixture);

const char*
parallel_single_fixture_get_error (parallel_single_fixture_t *fixture);

bool
parallel_single_fixture_ping (parallel_single_fixture_t *fixture, int thread_index);

#ifdef __cplusplus
}
#endif

#endif /* PARALLEL_SINGLE_H */
