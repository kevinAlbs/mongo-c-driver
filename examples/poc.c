#include "mongoc.h"
#include "mongoc-write-concern-private.h"
#include <stdalign.h>
#include <arpa/inet.h>

int main () {
    mongoc_uri_t* uri;
    mongoc_init ();

#ifdef BSON_EXTRA_ALIGN
    printf("BSON_EXTRA_ALIGN defined\n");
#else
    printf("BSON_EXTRA_ALIGN *not* defined\n");
#endif 

#ifdef BSON_HAVE_REALLOCF
    printf("BSON_HAVE_REALLOCF defined\n");
#else
    printf("BSON_HAVE_REALLOCF *not* defined\n");
#endif 

    /* try to repro CDRIVER-2575 */
    uint8_t data[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    uint16_t port;
    port = ntohs (*(short *) (data + 4));

    printf("alignof(bson_t)=%lu\n", alignof(bson_t));
    printf("alignof(mongoc_write_concern_t)=%lu\n", alignof(mongoc_write_concern_t));
    uri = mongoc_uri_new ("mongodb://localhost:27017");
    mongoc_uri_destroy (uri);
    mongoc_cleanup ();
}
