/*
 * Copyright 2009-present MongoDB, Inc.
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

#ifndef STREAM_TRACKER_H
#define STREAM_TRACKER_H

#include <mongoc/mongoc-client-pool.h>
#include <mongoc/mongoc-client.h>

#include <stdint.h>

// stream_tracker_t is a test utility to count streams created to servers.
typedef struct stream_tracker_t stream_tracker_t;

stream_tracker_t *
stream_tracker_new (void);

// stream_tracker_track_client tracks streams in a single-threaded client.
void
stream_tracker_track_client (stream_tracker_t *st, mongoc_client_t *client);

// stream_tracker_track_pool tracks streams in a pool.
void
stream_tracker_track_pool (stream_tracker_t *st, mongoc_client_pool_t *pool);

unsigned
stream_tracker_count (stream_tracker_t *st, const char *host);

void
stream_tracker_destroy (stream_tracker_t *st);

#define stream_tracker_assert_count(st, host, expect)       \
   if (1) {                                                 \
      unsigned _got = stream_tracker_count (st, host);      \
      if (_got != expect) {                                 \
         test_error ("Got unexpected stream count to %s:\n" \
                     "  Expected %u, got %u",               \
                     host,                                  \
                     expect,                                \
                     _got);                                 \
      }                                                     \
   } else                                                   \
      (void) 0

#define stream_tracker_assert_eventual_count(st, host, expect)                 \
   if (1) {                                                                    \
      int64_t _start = bson_get_monotonic_time ();                             \
      while (true) {                                                           \
         unsigned _got = stream_tracker_count (st, host);                      \
         if (_got == expect) {                                                 \
            break;                                                             \
         }                                                                     \
         int64_t _now = bson_get_monotonic_time ();                            \
         if (_now - _start > 5 * 1000 * 1000 /* five seconds */) {             \
            test_error ("Timed out waiting for expected stream count to %s:\n" \
                        "  Expected %u, got %u",                               \
                        host,                                                  \
                        expect,                                                \
                        _got);                                                 \
         }                                                                     \
      }                                                                        \
   } else                                                                      \
      (void) 0

#endif // STREAM_TRACKER_H
