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

#ifndef PERF_DEFINES_H
#define PERF_DEFINES_H

/* MONGODB_URI_ENV is the name of an optional environment variable to set a
 * custom URI. */
/* If it is not set, the default URI is "mongodb://localhost:27017". */
#define MONGODB_URI_ENV "MONGODB_URI"

/* MONGODB_ERROR_NOT_FOUND is a server error code for "ns not found" to ignore
 * if dropping an unknown collection. */
#define MONGODB_ERROR_NOT_FOUND 26

/* libmongoc uses a max client pool size of 100 by default. */
/* Only 100 clients can be checked out of a pool concurrently. */
#define MONGOC_DEFAULT_MAX_POOL_SIZE 100

#endif /* PERF_DEFINES_H */
