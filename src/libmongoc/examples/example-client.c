/* gcc example-client.c -o example-client $(pkg-config --cflags --libs
 * libmongoc-1.0) */

/* ./example-client [CONNECTION_STRING [COLLECTION_NAME]] */

#include <mongoc/mongoc.h>
#define MONGOC_INSIDE
#include <mongoc/mongoc-host-list-private.h>
#include <mongoc/mongoc-stream-private.h>
#include <mongoc/mongoc-stream-socket.h>
#include <stdio.h>
#include <stdlib.h>
static mongoc_stream_t *
_get_stream (const char *endpoint,
             int32_t connecttimeoutms,
             bson_error_t *error)
{
   mongoc_stream_socket_t *base_stream = NULL;
   mongoc_stream_t *tls_stream = NULL;
   bool ret = false;
   mongoc_ssl_opt_t ssl_opts = {0};
   mongoc_host_list_t host;
   char *host_and_port = NULL;

   if (!strchr (endpoint, ':')) {
      host_and_port = bson_strdup_printf ("%s:443", endpoint);
   } else {
      host_and_port = (char *) endpoint; /* we promise not to modify */
   }

   if (!_mongoc_host_list_from_string_with_err (&host, host_and_port, error)) {
      goto fail;
   }

   MONGOC_DEBUG ("KMS connecting to %s", host.host_and_port);


   base_stream = (mongoc_stream_socket_t*) mongoc_client_connect_tcp (connecttimeoutms, &host, error);
   
   if (!base_stream) {
      goto fail;
   }

   mongoc_socket_t *sock = mongoc_stream_socket_get_socket (base_stream);

   MONGOC_DEBUG ("TCP connection successful, used timeout of %d", (int)connecttimeoutms);

   /* Wrap in a tls_stream. */
   memcpy (&ssl_opts, mongoc_ssl_opt_get_default (), sizeof ssl_opts);
   tls_stream = mongoc_stream_tls_new_with_hostname (
      (mongoc_stream_t*) base_stream, host.host, &ssl_opts, 1 /* client */);

   MONGOC_DEBUG ("TCP performing handshake");

   if (!mongoc_stream_tls_handshake_block (
          tls_stream, host.host, connecttimeoutms, error)) {
      MONGOC_DEBUG ("Handshake failed: %s", error->message);
      goto fail;
   }

   ret = true;
fail:
   if (host_and_port != endpoint) {
      bson_free (host_and_port);
   }
   if (!ret) {
      if (tls_stream) {
         /* destroys base_stream too */
         mongoc_stream_destroy (tls_stream);
      } else if (base_stream) {
         mongoc_stream_destroy (base_stream);
      }
      return NULL;
   }
   return tls_stream;
}

int
main (int argc, char *argv[])
{
   bson_error_t error;
   mongoc_init ();

   mongoc_stream_t *stream = _get_stream ("kms.us-east-1.amazonaws.com", 50000, &error);
   
   if (!stream) {
      MONGOC_ERROR ("error: %s", error.message);
      return EXIT_FAILURE;
   }

   mongoc_stream_close (stream);
   mongoc_cleanup ();

   return EXIT_SUCCESS;
}
