#include <mongoc.h>
#define MONGOC_INSIDE
#include <mongoc/mongoc-client-private.h>
#include <mongoc/mongoc-host-list-private.h>

static mongoc_stream_t *
_get_stream (const char *endpoint,
             int32_t connecttimeoutms,
             bson_error_t *error)
{
   mongoc_stream_t *base_stream = NULL;
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

   base_stream = mongoc_client_connect_tcp (connecttimeoutms, &host, error);
   if (!base_stream) {
      goto fail;
   }

   /* Wrap in a tls_stream. */
   memcpy (&ssl_opts, mongoc_ssl_opt_get_default (), sizeof ssl_opts);
   tls_stream = mongoc_stream_tls_new_with_hostname (
      base_stream, host.host, &ssl_opts, 1 /* client */);

   if (!mongoc_stream_tls_handshake_block (
          tls_stream, host.host, connecttimeoutms, error)) {
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

int main (int argc, char** argv) {
   mongoc_stream_t *stream;
   bson_error_t error;

   if (argc != 2) {
      fprintf (stderr, "usage: example-tls <url>\n");
      return 1;
   }

   printf ("creating a TLS stream to: %s\n", argv[1]);
   stream = _get_stream (argv[1], 10000, &error);
   if (!stream) {
      fprintf (stderr, "got error: %s", error.message);
      return 1;
   }
   printf ("stream ok\n");
   return 0;
}