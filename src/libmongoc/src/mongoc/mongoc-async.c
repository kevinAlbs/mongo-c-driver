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


#include <bson/bson.h>

#include "mongoc-async-private.h"
#include "mongoc-async-cmd-private.h"
#include "utlist.h"
#include "mongoc.h"
#include "mongoc-socket-private.h"
#include "mongoc-util-private.h"

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

   if (async->interruptible) {
      /* TODO: close sockets */
   }

   bson_free (async);
}

void
mongoc_async_iterate (mongoc_async_t *async)
{
   mongoc_async_cmd_t *acmd, *tmp;
   mongoc_async_cmd_t **acmds_polled = NULL;
   mongoc_stream_poll_t *poller = NULL;
   int nstreams, i;
   ssize_t nactive = 0;
   int64_t now;
   int64_t expire_at;
   int64_t poll_timeout_msec;
   size_t poll_size;

   now = bson_get_monotonic_time ();
   poll_size = 0;
   /* Store this off of async, so we don't need to alloc every iteration. */
   /* ncmds grows if we discover a replica & start calling ismaster on it */
   poll_size = async->ncmds;
   if (async->interruptible) {
      poll_size += 1;
   }
   poller = (mongoc_stream_poll_t *) bson_malloc (sizeof (*poller) * poll_size);
   acmds_polled = (mongoc_async_cmd_t **) bson_malloc (sizeof (*acmds_polled) *
                                                       async->ncmds);

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
               BSON_ASSERT (acmd->stream);
            } else {
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
         acmds_polled[nstreams] = acmd;
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
      return;
   }

   poll_timeout_msec = BSON_MAX (0, (expire_at - now) / 1000);
   BSON_ASSERT (poll_timeout_msec < INT32_MAX);

   /* If interruptible, add our special stream. */
   if (async->interruptible) {
      poller[nstreams].events = POLLIN;
      poller[nstreams].revents = 0;
      poller[nstreams].stream = async->interrupt_stream_read;
      ++nstreams;
   }

   if (nstreams > 0) {
      /* we need at least one stream to poll. */
      MONGOC_DEBUG ("poll begin");
      nactive =
         mongoc_stream_poll (poller, nstreams, (int32_t) poll_timeout_msec);
      MONGOC_DEBUG ("poll end");
   } else {
      /* currently this does not get hit. we always have at least one command
       * initialized with a stream. */
      _mongoc_usleep (poll_timeout_msec * 1000);
   }

   if (nactive > 0) {
      for (i = 0; i < nstreams; i++) {
         mongoc_async_cmd_t *iter;

         if (i == nstreams - 1 && async->interruptible) {
            /* Special case, this is the pipe. */
            if (poller[i].revents & POLLIN) {
#ifndef _WIN32
               char tmp_buf[1];
               /* read the one byte in a blocking fashion. */
               /* TODO timeout? */
               mongoc_stream_read (
                  async->interrupt_stream_read, (void *) tmp_buf, 1, 1, 1000);
               MONGOC_DEBUG ("interrupt read stream got %c", tmp_buf[0]);
#endif
            }
            break;
         }

         iter = acmds_polled[i];
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
            (void) mongoc_async_cmd_run (iter);
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

      /* TODO: I think the async loop should not be inspecting the state of the
       * async cmd. */
      /* check if an initiated cmd has passed the connection timeout.  */
      if (acmd->state != MONGOC_ASYNC_CMD_INITIATE &&
          now > acmd->connect_started + acmd->timeout_msec * 1000) {
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
         acmd->cb (acmd, result, NULL, (now - acmd->connect_started) / 1000);

         /* Remove acmd from the async->cmds doubly-linked list */
         mongoc_async_cmd_destroy (acmd);
      }
   }

   now = bson_get_monotonic_time ();

   bson_free (poller);
   bson_free (acmds_polled);
}

void
mongoc_async_run_to_completion (mongoc_async_t *async)
{
   int64_t now;
   mongoc_async_cmd_t *acmd;

   now = bson_get_monotonic_time ();

   /* TODO: I'll need to figure out how to fit this in. */
   /* CDRIVER-1571 reset start times in case a stream initiator was slow */
   DL_FOREACH (async->cmds, acmd)
   {
      acmd->connect_started = now;
   }

   while (async->ncmds) {
      mongoc_async_iterate (async);
   }
}

static mongoc_socket_t *
_create_udp_socket (struct sockaddr_in *out_addr)
{
   mongoc_socket_t *sock;
   mongoc_socklen_t out_len;
   int ret;

   out_len = sizeof (struct sockaddr_in);

   memset (out_addr, 0, sizeof (struct sockaddr_in));
   out_addr->sin_family = AF_INET;
   out_addr->sin_addr.s_addr = INADDR_ANY;
   out_addr->sin_port = 0;

   sock = mongoc_socket_new (AF_INET, SOCK_DGRAM, 0);
   ret = mongoc_socket_bind (
      sock, (struct sockaddr *) out_addr, sizeof (struct sockaddr_in));
   mongoc_socket_getsockname (sock, (struct sockaddr *) out_addr, &out_len);
   return sock;
}

bool
mongoc_async_make_interruptible (mongoc_async_t *async)
{
   async->interruptible = true;
   int ret;

   async->interrupt_socket_read =
      _create_udp_socket (&async->interrupt_read_addr);

   async->interrupt_socket_write =
      _create_udp_socket (&async->interrupt_write_addr);

   ret =
      mongoc_socket_connect (async->interrupt_socket_read,
                             (struct sockaddr *) &async->interrupt_write_addr,
                             sizeof (struct sockaddr_in),
                             1000);
   MONGOC_DEBUG ("connect read => write = %d", ret);
   ret = mongoc_socket_connect (async->interrupt_socket_write,
                                (struct sockaddr *) &async->interrupt_read_addr,
                                sizeof (struct sockaddr_in),
                                1000);
   MONGOC_DEBUG ("connect write => read = %d", ret);

   async->interrupt_stream_read =
      mongoc_stream_socket_new (async->interrupt_socket_read);
   async->interrupt_stream_write =
      mongoc_stream_socket_new (async->interrupt_socket_write);
   return true;
}

void
mongoc_async_interrupt (mongoc_async_t *async)
{
   BSON_ASSERT (async->interruptible);
   mongoc_stream_write (async->interrupt_stream_write, "x", 1, 1000);
}