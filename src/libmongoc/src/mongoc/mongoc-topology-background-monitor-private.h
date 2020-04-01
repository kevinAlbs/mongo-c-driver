
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

#ifndef MONGOC_TOPOLOGY_BACKGROUND_MONITOR_PRIVATE_H
#define MONGOC_TOPOLOGY_BACKGROUND_MONITOR_PRIVATE_H

#include <bson/bson.h>

struct _mongoc_background_monitor_t;

struct _mongoc_background_monitor_t * mongoc_topology_background_monitor_new (mongoc_topology_t *topology);

/* Topology has changed. */
void
mongoc_topology_background_monitor_reconcile (struct _mongoc_background_monitor_t *background_monitor);

/* Server selection or something else needs immediate scan. */
void
mongoc_topology_background_monitor_request_scan (struct _mongoc_background_monitor_t *background_monitor);

/* Grab all errors from the topology description. */
void
mongoc_topology_background_monitor_collect_errors (struct _mongoc_background_monitor_t *background_monitor, bson_error_t *error);

void
mongoc_topology_background_monitor_shutdown (struct _mongoc_background_monitor_t *background_monitor);

void mongoc_topology_background_monitor_destroy (struct _mongoc_background_monitor_t *background_monitor);

#endif