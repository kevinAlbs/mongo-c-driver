#include <mongoc/mongoc.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

typedef struct {
   bool verbose;
   int num_threads;
   int num_cmds;
   bool continuous;
   mongoc_client_pool_t *pool;
} ctx_t;

static void
print_help (void)
{
   printf ("Configure with the following optional environment variables\n");
   printf ("VERBOSE - print additional output (by default, only errors are "
           "printed)\n");
   printf ("NUM_THREADS - the number of threads to spawn (default 1)\n");
   printf ("NUM_CMDS - the number of commands to send on each thread (default "
           "100)\n");
   printf ("CONTINUOUS - set to ON to continuously repeat the process (default "
           "OFF)\n");
   printf ("Note: use the URI maxPoolSize to control the maximum client pool "
           "size. Clients on the pool reuse connections. A smaller maxPoolSize "
           "than NUM_THREADS will result in many more connection creations / "
           "auth handshakes.\n");
}

static void
print_ctx (ctx_t *ctx)
{
   MONGOC_DEBUG (
      "Configuration: VERBOSE=%d, NUM_THREADS=%d, NUM_CMDS=%d, CONTINUOUS=%d",
      (int) ctx->verbose,
      ctx->num_threads,
      ctx->num_cmds,
      (int) ctx->continuous);
}

static void
error_exit (ctx_t *ctx, bson_error_t *error)
{
   /* Print anything that may be remotely useful. */
   MONGOC_ERROR ("ERROR ENCOUNTERED - %s, %d, %d",
                 error->message,
                 error->code,
                 error->domain);
   MONGOC_ERROR ("process ID: %d", (int) getpid ());
   print_ctx (ctx);
   exit (1);
}

void *
worker (void *ctx_void)
{
   ctx_t *ctx;
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   bson_t ping;
   bson_t server_status;
   bson_error_t error;
   int i;
   mongoc_read_prefs_t *secondary_prefs;

   ctx = (ctx_t *) ctx_void;
   pool = (mongoc_client_pool_t *) ctx->pool;

   /* Vary command between 'ping' and 'serverStatus' */
   bson_init (&ping);
   BCON_APPEND (&ping, "ping", BCON_INT32 (1));
   bson_init (&server_status);
   BCON_APPEND (&server_status, "serverStatus", BCON_INT32 (1));

   /* Vary read preference between primary and secondary */
   secondary_prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);

   for (i = 0; i < ctx->num_cmds; i++) {
      client = mongoc_client_pool_pop (pool);
      if (!mongoc_client_command_simple (client,
                                         "db",
                                         i % 2 == 0 ? &ping : &server_status,
                                         i % 2 == 0 ? secondary_prefs : NULL,
                                         NULL,
                                         &error)) {
         error_exit (ctx, &error);
      }
      mongoc_client_pool_push (pool, client);
   }

   bson_destroy (&server_status);
   bson_destroy (&ping);
   mongoc_read_prefs_destroy (secondary_prefs);
   return NULL;
}

bool
getenv_bool (const char *name, bool default_value)
{
   char *value = getenv (name);
   if (!value) {
      return default_value;
   }
   return (0 == strcasecmp (value, "ON"));
}

int
getenv_int (const char *name, int default_value)
{
   char *value = getenv (name);
   if (!value) {
      return default_value;
   }

   return atoi (value);
}

int
main (int argc, char *argv[])
{
   int i;
   pthread_t *threads;
   char *uri_str;
   mongoc_uri_t *uri;
   mongoc_client_pool_t *pool;
   bson_error_t error;
   ctx_t ctx = {0};

   mongoc_init ();

   if (argc != 2) {
      fprintf (stderr, "Usage: ./example-client <uri>\n");
      print_help ();
      return EXIT_FAILURE;
   }

   ctx.verbose = getenv_bool ("VERBOSE", false);
   ctx.num_threads = getenv_int ("NUM_THREADS", 1);
   ctx.num_cmds = getenv_int ("NUM_CMDS", 10);
   ctx.continuous = getenv_bool ("CONTINUOUS", false);

   if (ctx.verbose) {
      print_ctx (&ctx);
   }

   uri_str = argv[1];
   uri = mongoc_uri_new_with_error (uri_str, &error);
   if (!uri) {
      MONGOC_ERROR ("uri error: %s", error.message);
      return EXIT_FAILURE;
   }

   threads = (pthread_t *) bson_malloc (sizeof (pthread_t) * ctx.num_threads);
   do {
      pool = mongoc_client_pool_new (uri);
      ctx.pool = pool;
      for (i = 0; i < ctx.num_threads; i++) {
         pthread_create (threads + i, NULL /* attr */, worker, &ctx);
      }

      for (i = 0; i < ctx.num_threads; i++) {
         pthread_join (threads[i], NULL);
      }

      if (ctx.verbose) {
         MONGOC_DEBUG ("done");
      }
      mongoc_client_pool_destroy (pool);
   } while (ctx.continuous);

   bson_free (threads);
   mongoc_uri_destroy (uri);
   mongoc_cleanup ();
}
