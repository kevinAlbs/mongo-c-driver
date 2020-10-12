#include <mongoc/mongoc.h>
#include <pthread.h>

#define NTHREADS 10

void* thread_fn (void* client_void) {
    mongoc_client_t *client;
    bson_t ping = BSON_INITIALIZER;
    bson_error_t error;
    int i;

    client = (mongoc_client_t*) client_void;
    BCON_APPEND (&ping, "ping", BCON_INT32(1));
    for (i = 0; i < 5; i++) {
        if (!mongoc_client_command_simple (client, "db", &ping, NULL, NULL, &error)) {
            printf ("error = %s\n", error.message);
        }
    }
    return NULL;
}

int main (int argc, char** argv) {
    int i;
    char* uri_str;
    mongoc_client_t* clients[NTHREADS];
    pthread_t threads[NTHREADS];

    if (argc < 2) {
        printf ("usage: connect <uri>\n");
        return 1;
    }
    uri_str = argv[1];

    mongoc_init ();
    
    for (i = 0; i < NTHREADS; i++) {
        clients[i] = mongoc_client_new (uri_str);
        pthread_create (&threads[i], NULL, thread_fn, clients[i]);
    }
    for (i = 0; i < NTHREADS; i++) {
        pthread_join (threads[i], NULL);
    }
}