
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

#include "mongoc-prelude.h"

#ifndef MONGOC_TOPOLOGY_BACKGROUND_MONITOR_PRIVATE_H
#define MONGOC_TOPOLOGY_BACKGROUND_MONITOR_PRIVATE_H

#include <mongoc/mongoc.h>

/* Functions related to background monitoring. */

bool
_mongoc_topology_background_monitor_start (mongoc_topology_t *topology);

void
mongoc_topology_background_monitor_reconcile (mongoc_topology_t *topology);

/* Server selection or something else needs immediate scan. */
void
mongoc_topology_background_monitor_request_scan (mongoc_topology_t *topology);

void
mongoc_topology_background_monitor_collect_errors (mongoc_topology_t *topology, bson_error_t *error);

void
mongoc_topology_background_monitor_shutdown (mongoc_topology_t *topology);

#endif