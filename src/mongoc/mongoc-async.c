/*
 * Copyright 2014 MongoDB, Inc.
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


#include <bson.h>

#include "mongoc-async-private.h"
#include "mongoc-async-cmd-private.h"
#include "utlist.h"
#include "mongoc.h"
#include "mongoc-socket-private.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "async"


mongoc_async_t *
mongoc_async_new ()
{
   mongoc_async_t *async = (mongoc_async_t *) bson_malloc0 (sizeof (*async));

   return async;
}

void
mongoc_async_destroy (mongoc_async_t *async)
{
   mongoc_async_cmd_t *acmd, *tmp;

   DL_FOREACH_SAFE (async->cmds, acmd, tmp)
   {
      mongoc_async_cmd_destroy (acmd);
   }

   bson_free (async);
}

void
mongoc_async_run (mongoc_async_t *async)
{
   mongoc_async_cmd_t *acmd, *tmp;
   mongoc_stream_poll_t *poller = NULL;
   int nstreams, i;
   ssize_t nactive;
   int64_t now;
   int64_t expire_at;
   int64_t poll_timeout_msec;
   size_t poll_size;

   now = bson_get_monotonic_time ();
   poll_size = 0;

   /* CDRIVER-1571 reset start times in case a stream initiator was slow */
   DL_FOREACH (async->cmds, acmd)
   {
      acmd->connect_started = now;
   }

   while (async->ncmds) {
      /* ncmds grows if we discover a replica & start calling ismaster on it */
      if (poll_size < async->ncmds) {
         poller = (mongoc_stream_poll_t *) bson_realloc (
            poller, sizeof (*poller) * async->ncmds);

         poll_size = async->ncmds;
      }

      expire_at = INT64_MAX;
      nstreams = 0;

      /* check if any cmds are ready to be initiated. */
      DL_FOREACH_SAFE (async->cmds, acmd, tmp)
      {
         if (acmd->state == MONGOC_ASYNC_CMD_INITIATE) {
            BSON_ASSERT (!acmd->stream);
            if (now >= acmd->initiate_delay_ms * 1000 + acmd->connect_started) {
               /* time to initiate. */
               if (mongoc_async_cmd_run (acmd)) {
                  mongoc_socket_t *sock = mongoc_stream_socket_get_socket (acmd->stream);
                  char * names[] = { [AF_INET] = "AF_INET", [AF_INET6] = "AF_INET6"};
                  printf("initiated %s/%s\n", names[sock->domain], names[acmd->dns_result->ai_family]);
                  BSON_ASSERT (acmd->stream);
                  /* reset the connect started time after connection starts. */
                  /* TODO: does this break expectations? */
                  acmd->connect_started = bson_get_monotonic_time ();
               } else {
                  printf("failed to initiate\n");
                  /* this command was removed. */
                  continue;
               }
            } else {
               /* don't poll longer than the earliest cmd ready to init. */
               expire_at = BSON_MIN (
                  expire_at, acmd->connect_started + acmd->initiate_delay_ms);
            }
         }

         if (acmd->stream) {
            poller[nstreams].acmd = acmd;
            poller[nstreams].stream = acmd->stream;
            poller[nstreams].events = acmd->events;
            poller[nstreams].revents = 0;
            expire_at = BSON_MIN (
               expire_at, acmd->connect_started + acmd->timeout_msec * 1000);
            ++nstreams;
         }
      }

      if (async->ncmds == 0) {
         /* all cmds failed to initiate and removed themselves. */
         break;
      }

      poll_timeout_msec = BSON_MAX (0, (expire_at - now) / 1000);
      BSON_ASSERT (poll_timeout_msec < INT32_MAX);

      if (nstreams > 0) {
         printf("polling with %d streams\n", nstreams);
         /* we need at least one stream to poll. */
         nactive =
            mongoc_stream_poll (poller, nstreams, (int32_t) poll_timeout_msec);
      } else {
         /* TODO: I'm hesitant of this. */
         printf("sleeping for %dms\n", poll_timeout_msec);
         struct timespec delay;
         delay.tv_sec = poll_timeout_msec / 1000;
         delay.tv_nsec = (poll_timeout_msec % 1000) * 1000 * 1000;
         nanosleep (&delay, NULL);
      }

      if (nactive) {
         for (i = 0; i < nstreams; i++) {
            mongoc_async_cmd_t* iter = poller[i].acmd;
            if (poller[i].revents & (POLLERR | POLLHUP)) {
               int hup = poller[i].revents & POLLHUP;
               if (iter->state == MONGOC_ASYNC_CMD_SEND) {
                  bson_set_error (&iter->error,
                                  MONGOC_ERROR_STREAM,
                                  MONGOC_ERROR_STREAM_CONNECT,
                                  hup ? "connection refused"
                                      : "unknown connection error");
               } else {
                  bson_set_error (&iter->error,
                                  MONGOC_ERROR_STREAM,
                                  MONGOC_ERROR_STREAM_SOCKET,
                                  hup ? "connection closed"
                                      : "unknown socket error");
               }

               iter->state = MONGOC_ASYNC_CMD_ERROR_STATE;
            }

            if ((poller[i].revents & poller[i].events) ||
                iter->state == MONGOC_ASYNC_CMD_ERROR_STATE) {
               char * names[] = { [AF_INET] = "AF_INET", [AF_INET6] = "AF_INET6"};
               printf("running for stream %s\n", names[iter->dns_result->ai_family]);
               mongoc_async_cmd_run (iter);
               nactive--;
            }

            if (!nactive) {
               break;
            }
         }
      }

      DL_FOREACH_SAFE (async->cmds, acmd, tmp)
      {
         bool remove_cmd = false;
         mongoc_async_cmd_result_t result;

         /* check if an initiated cmd has passed the connection timeout.  */
         if (acmd->state != MONGOC_ASYNC_CMD_INITIATE &&
             now > acmd->connect_started + acmd->timeout_msec * 1000) {
            mongoc_socket_t *sock = mongoc_stream_socket_get_socket (acmd->stream);
            char * names[] = { [AF_INET] = "AF_INET", [AF_INET6] = "AF_INET6"};
            printf("timed out on stream for %s\n", names[sock->domain]);
            bson_set_error (&acmd->error,
                            MONGOC_ERROR_STREAM,
                            MONGOC_ERROR_STREAM_CONNECT,
                            acmd->state == MONGOC_ASYNC_CMD_SEND
                               ? "connection timeout"
                               : "socket timeout");

            remove_cmd = true;
            result = MONGOC_ASYNC_CMD_TIMEOUT;
         } else if (acmd->state == MONGOC_ASYNC_CMD_CANCELED_STATE) {
            char * names[] = { [AF_INET] = "AF_INET", [AF_INET6] = "AF_INET6"};
            printf("cancelled on stream for %s\n", names[acmd->dns_result->ai_family]);
            remove_cmd = true;
            result = MONGOC_ASYNC_CMD_ERROR;
         }

         if (remove_cmd) {
            acmd->cb (acmd, result, NULL, (now - acmd->connect_started) / 1000);

            /* Remove acmd from the async->cmds doubly-linked list */
            mongoc_async_cmd_destroy (acmd);
         }
      }

      now = bson_get_monotonic_time ();
   }

   if (poll_size) {
      bson_free (poller);
   }
}
