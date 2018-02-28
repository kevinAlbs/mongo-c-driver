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
   int i;
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
      int nstreams = 0;
      /* check if any cmds are ready to be initiated. */
      DL_FOREACH_SAFE (async->cmds, acmd, tmp)
      {
         if (acmd->state == MONGOC_ASYNC_CMD_INITIATE) {
            if (now >= acmd->initiate_delay + acmd->connect_started) {
               printf ("initiating stream\n");
               BSON_ASSERT (!acmd->stream);
               if (mongoc_async_cmd_run (acmd)) {
                  BSON_ASSERT (acmd->stream);
                  char *names[] = { [AF_INET6] = "AF_INET6", [AF_INET] = "AF_INET" };
                  if (acmd->stream->type == 1) {
                     mongoc_socket_t *sock = mongoc_stream_socket_get_socket (
                        (mongoc_stream_socket_t *) acmd->stream);
                     printf ("got stream with domain: %s\n", names[sock->domain]);
                  }
                  /* reset the connect_started time this may have been delayed. */
                  acmd->connect_started = now;
               }
            } else {
               int64_t left =
                  acmd->initiate_delay - (now - acmd->connect_started);
               printf ("we got %d usecs left I think\n", left);
            }
         }
         if (acmd->stream) {
            ++nstreams;
         }
      }

      if (async->ncmds == 0) {
         /* all cmds failed to initiate and removed themselves. */
         break;
      }

      /* ncmds grows if we discover a replica & start calling ismaster on it */
      if (poll_size < nstreams) {
         poller = (mongoc_stream_poll_t *) bson_realloc (
            poller, sizeof (*poller) * nstreams);

         poll_size = nstreams;
      }

      i = 0;
      expire_at = INT64_MAX;
      DL_FOREACH (async->cmds, acmd)
      {
         if (acmd->state == MONGOC_ASYNC_CMD_INITIATE) {
            /* timeout the poll sooner if an initiate delay expires soon. */
            expire_at = BSON_MIN (expire_at, acmd->connect_started + acmd->initiate_delay);
         } else {
            BSON_ASSERT (i < nstreams);
            poller[i].stream = acmd->stream;
            poller[i].events = acmd->events;
            poller[i].revents = 0;
            BSON_ASSERT (acmd->connect_started > 0);
            expire_at = BSON_MIN (expire_at, acmd->connect_started +
                                             acmd->timeout_msec * 1000);
            i++;
         }
      }

      poll_timeout_msec = BSON_MAX (0, (expire_at - now) / 1000);
      BSON_ASSERT (poll_timeout_msec < INT32_MAX);

      if (nstreams > 0) {
         /* we need at least one stream to poll. */
         printf("polling\n");
         nactive = mongoc_stream_poll (poller, nstreams,
                                       (int32_t) poll_timeout_msec);
         printf("done polling\n");
      } else {
         /* TODO: I'm *very* hesitant of this. I don't know enough about the implications of what sleep does to a thread's scheduling vs poll. Should I instead pass the type of stream being used a different way? */
         printf("there are no streams to poll, sleeping instead\n");
         struct timespec delay;
         delay.tv_sec = poll_timeout_msec / 1000;
         delay.tv_nsec = (poll_timeout_msec % 1000) * 1000 * 1000;
         nanosleep(&delay, NULL);
         printf("done sleeping\n");
      }

      if (nactive) {
         i = 0;
         DL_FOREACH_SAFE (async->cmds, acmd, tmp)
         {
            if (poller[i].revents & (POLLERR | POLLHUP)) {
               int hup = poller[i].revents & POLLHUP;
               if (acmd->state == MONGOC_ASYNC_CMD_SEND) {
                  bson_set_error (&acmd->error,
                                  MONGOC_ERROR_STREAM,
                                  MONGOC_ERROR_STREAM_CONNECT,
                                  hup ? "connection refused"
                                      : "unknown connection error");
               } else {
                  bson_set_error (&acmd->error,
                                  MONGOC_ERROR_STREAM,
                                  MONGOC_ERROR_STREAM_SOCKET,
                                  hup ? "connection closed"
                                      : "unknown socket error");
               }

               acmd->state = MONGOC_ASYNC_CMD_ERROR_STATE;
            }

            if ((poller[i].revents & poller[i].events) ||
                acmd->state == MONGOC_ASYNC_CMD_ERROR_STATE) {
               mongoc_async_cmd_run (acmd);
               nactive--;
            }

            if (!nactive) {
               break;
            }

            i++;
         }
      }

      DL_FOREACH_SAFE (async->cmds, acmd, tmp)
      {
         bool remove_cmd = false;
         mongoc_async_cmd_result_t result;

         if (acmd->state != MONGOC_ASYNC_CMD_INITIATE && now > acmd->connect_started + acmd->timeout_msec * 1000) {
            bson_set_error (&acmd->error,
                            MONGOC_ERROR_STREAM,
                            MONGOC_ERROR_STREAM_CONNECT,
                            acmd->state == MONGOC_ASYNC_CMD_SEND
                               ? "connection timeout"
                               : "socket timeout");

            remove_cmd = true;
            result = MONGOC_ASYNC_CMD_TIMEOUT;
         } else if (acmd->state == MONGOC_ASYNC_CMD_CANCELED_STATE) {
            remove_cmd = true;
            result = MONGOC_ASYNC_CMD_ERROR;
         }

         if (remove_cmd) {
            printf("removing this command\n");
            acmd->cb (acmd, result, NULL, (now - acmd->connect_started) / 1000);

            /* Remove acmd from the async->cmds doubly-linked list */
            mongoc_async_cmd_destroy (acmd);
         }
      }

      now = bson_get_monotonic_time ();
   }

   printf("done with async loop\n");

   if (poll_size) {
      bson_free (poller);
   }
}
