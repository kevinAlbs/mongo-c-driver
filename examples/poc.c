#include "mongoc.h"

int main () {
   mongoc_init ();
   printf("UBSAN test\n");
   mongoc_cleanup ();
}