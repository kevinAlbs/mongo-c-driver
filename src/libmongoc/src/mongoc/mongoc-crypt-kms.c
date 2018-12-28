/*
 * Copyright 2018-present MongoDB, Inc.
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

#include "mongoc/mongoc.h"
#include "mongoc/mongoc-crypt-private.h"

#define ERRNO_IS_AGAIN(errno)                                          \
   ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK) || \
    (errno == EINPROGRESS))


static mongoc_stream_t *
_get_aws_stream (bson_error_t *error)
{
   int errcode;
   int r;
   struct sockaddr_in server_addr = {0};
   mongoc_socket_t *conn_sock = NULL;
   mongoc_stream_t *stream = NULL;
   mongoc_stream_t *tls_stream = NULL;
   mongoc_ssl_opt_t ssl_opts = {0};

   memcpy (&ssl_opts, mongoc_ssl_opt_get_default (), sizeof ssl_opts);
   conn_sock = mongoc_socket_new (AF_INET, SOCK_STREAM, 0);
   if (!conn_sock) {
      SET_CRYPT_ERR ("could not create socket to AWS");
      return NULL;
   }

   server_addr.sin_family = AF_INET;
   server_addr.sin_port = htons (443);
   /* TODO: actually do a DNS lookup. */
   /* 54.239.18.135, kms.us-east-1.amazonaws.com */
   server_addr.sin_addr.s_addr = htonl (0x36EF1287);
   r = mongoc_socket_connect (
      conn_sock, (struct sockaddr *) &server_addr, sizeof (server_addr), -1);

   errcode = mongoc_socket_errno (conn_sock);
   if (!(r == 0 || ERRNO_IS_AGAIN (errcode))) {
      mongoc_socket_destroy (conn_sock);
      SET_CRYPT_ERR (
         "mongoc_socket_connect unexpected return: %d (errno: %d)\n",
         r,
         errcode);
      return NULL;
   }

   stream = mongoc_stream_socket_new (conn_sock);
   tls_stream = mongoc_stream_tls_new_with_hostname (
      stream, "kms.us-east-1.amazonaws.com", &ssl_opts, 1 /* client */);

   if (!tls_stream) {
      SET_CRYPT_ERR ("could not create TLS stream on AWS");
      mongoc_stream_destroy (stream);
      return NULL;
   }

   if (!mongoc_stream_tls_handshake_block (
          tls_stream, "kms.us-east-1.amazonaws.com", 1000, error)) {
      mongoc_stream_destroy (tls_stream);
      return NULL;
   }

   return tls_stream;
}

typedef struct {
} kms_request_t;
typedef struct {
} kms_response_t;

static bool
_api_call (mongoc_crypt_t *crypt,
           const kms_request_t *request,
           kms_response_t *response,
           bson_error_t *error)
{
   bool ret = false;
   mongoc_stream_t *stream;

   stream = _get_aws_stream (error);
   if (!stream) {
      return false;
   }

   ret = true;
cleanup:
   mongoc_stream_destroy (stream);
   return ret;
}


bool
_mongoc_crypt_kms_decrypt (mongoc_crypt_t *crypt,
                           mongoc_crypt_key_t *key,
                           bson_error_t *error)
{
   return false;
}
