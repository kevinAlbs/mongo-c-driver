#include <mongoc/mongoc.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#define MONGOC_INSIDE
#include <mongoc/mongoc-client-private.h>
#include <mongoc/mongoc-host-list-private.h>

int
main (int argc, char *argv[])
{
   mongoc_host_list_t host_list;
   mongoc_stream_t *stream;
   bson_error_t error;

   mongoc_init ();

   if (!_mongoc_host_list_from_string_with_err (&host_list, argv[1], &error)) {
      MONGOC_ERROR ("error = %s", error.message);
      return EXIT_FAILURE;
   }


   MONGOC_DEBUG ("connection start");
   stream = mongoc_client_connect_tcp (5000, &host_list, &error);
   MONGOC_DEBUG ("connection end");

   if (!stream) {
      MONGOC_DEBUG ("failed");
   } else {
      MONGOC_DEBUG ("succeeded");
   }
   mongoc_stream_destroy (stream);
   mongoc_cleanup ();

   return EXIT_SUCCESS;
}
