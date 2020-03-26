#include "mongoc-prelude.h"

#ifndef MONGOC_POC_AWAITER
#define MONGOC_POC_AWAITER

#include "mongoc.h"

/* The callback to handle an ismaster reply. */
typedef void (*ismaster_callback_t) (
   uint32_t id,
   const bson_t *bson,
   int64_t rtt,
   void *data,
   const bson_error_t *error /* IN */);

typedef struct _mongoc_awaiter_t mongoc_awaiter_t;

mongoc_awaiter_t * mongoc_awaiter_new (ismaster_callback_t ismaster_cb);

/* Called upon initialization, and when an ismaster reply is handled.
 * Consequently, this may be called in the middle of mongoc_awaiter_check if the topology callback. 
 * 
 * Preconditions: caller must have the lock for the topology description.
 * Side-effects: may modify the awaiter's set of nodes.
 */
void mongoc_awaiter_reconcile_w_lock (mongoc_awaiter_t *awaiter, const mongoc_topology_description_t *description);

/* Called every time for server selection. Polls all streams. */
void mongoc_awaiter_check (mongoc_awaiter_t *awaiter);

void mongoc_awaiter_destroy (mongoc_awaiter_t *awaiter);

/* Dump debug info. */
void mongoc_awaiter_dump (mongoc_awaiter_t *awaiter);

#endif