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

#include <mongoc/mongoc-prelude.h>

#ifndef MONGOC_STREAM_TLS_SECURE_CHANNEL_PRIVATE_H
#define MONGOC_STREAM_TLS_SECURE_CHANNEL_PRIVATE_H

#include <mongoc/mongoc-stream-tls-private.h>

#ifdef MONGOC_ENABLE_SSL_SECURE_CHANNEL
#include <bson/bson.h>

/* Its mandatory to indicate to Windows who is compiling the code */
#define SECURITY_WIN32
#include <security.h>


BSON_BEGIN_DECLS


/* enum for the nonblocking SSL connection state machine */
typedef enum {
   ssl_connect_1,
   ssl_connect_2,
   ssl_connect_2_reading,
   ssl_connect_2_writing,
   ssl_connect_3,
   ssl_connect_done
} ssl_connect_state;

/* Structs to store Schannel handles */
typedef struct {
   CredHandle cred_handle;
   TimeStamp time_stamp;
   PCCERT_CONTEXT cert; /* Owning. Optional client cert. */
} mongoc_secure_channel_cred;

typedef struct {
   CtxtHandle ctxt_handle;
   TimeStamp time_stamp;
} mongoc_secure_channel_ctxt;

/**
 * mongoc_stream_tls_secure_channel_t:
 *
 * Private storage for Secure Channel Streams
 */
typedef struct {
   ssl_connect_state connecting_state;
   mongoc_secure_channel_cred *cred;
   mongoc_secure_channel_ctxt *ctxt;
   SecPkgContext_StreamSizes stream_sizes;
   size_t encdata_length, decdata_length;
   size_t encdata_offset, decdata_offset;
   unsigned char *encdata_buffer, *decdata_buffer;
   unsigned long req_flags, ret_flags;
   int recv_unrecoverable_err;  /* _mongoc_stream_tls_secure_channel_read had an
                                   unrecoverable err */
   bool recv_sspi_close_notify; /* true if connection closed by close_notify */
   bool recv_connection_closed; /* true if connection closed, regardless how */
   bool renegotiating;
   mongoc_stream_tls_t *tls;
   char* hostname;
} mongoc_stream_tls_secure_channel_t;

typedef struct {
    PCCERT_CONTEXT cert;
    bool imported_private_key;
    wchar_t key_name[39]; // Holds max-length GUID string.
    bool ok;
} mongoc_secure_channel_sharedcert_t;

mongoc_stream_t *
mongoc_stream_tls_secure_channel_new_with_PCERT_CONTEXT (mongoc_stream_t *base_stream, const char *host, mongoc_ssl_opt_t *opt, int client, PCCERT_CONTEXT cert);

mongoc_stream_t *
mongoc_stream_tls_secure_channel_new_with_sharedcert (mongoc_stream_t *base_stream, const char *host, mongoc_ssl_opt_t *opt, int client, mongoc_secure_channel_sharedcert_t* sharedcert);

mongoc_stream_t *
mongoc_stream_tls_new_with_hostname_and_secure_channel_sharedcert (
   mongoc_stream_t *base_stream, const char *host, mongoc_ssl_opt_t *opt, int client, mongoc_secure_channel_sharedcert_t *sharedcert);

BSON_END_DECLS

#endif /* MONGOC_ENABLE_SSL_SECURE_CHANNEL */
#endif /* MONGOC_STREAM_TLS_SECURE_CHANNEL_PRIVATE_H */
