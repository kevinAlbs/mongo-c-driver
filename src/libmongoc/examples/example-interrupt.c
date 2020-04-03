/*
An example of a call to "poll" being interrupted by another thread.
*/

#include <mongoc/mongoc.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

void print_events (int revents) {
    if (revents & POLLERR) printf ("POLLERR ");
    if (revents & POLLHUP) printf ("POLLHUP ");
    if (revents & POLLIN) printf ("POLLIN ");
    if (revents & POLLOUT) printf ("POLLOUT ");
    printf ("\n");
}

typedef struct {
    int sock;
    int pipe_read_fd;
} thread_ctx_t;

int create_socket (void) {
    struct sockaddr_in addr;
    int sock;

    memset (&addr, 0, sizeof (struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons (27017);
    inet_pton(AF_INET, "127.0.0.1", &(addr.sin_addr));

    /* Open a socket to 127.0.0.1. */
    sock = socket (PF_INET, SOCK_STREAM, 0 /* protocol. */);
    if (sock == -1) {
        MONGOC_DEBUG ("creating socket failed");
        return -1;
    }

    if (-1 == connect (sock, (struct sockaddr *) &addr, sizeof (struct sockaddr_in))) {
        MONGOC_DEBUG ("connect failed");
        return -1;
    }

    /* Set the read timeout of 30 seconds. */
#ifdef _WIN32
    DWORD timeout = 30;
    setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof (timeout));
#else
    struct timeval tv;
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof (struct timeval));
#endif

    return sock;
}

void * background_thread_fn (void* ctx) {
    thread_ctx_t *thread_ctx = (thread_ctx_t*) ctx;
    uint8_t buf[1];

    /* Don't send anything, just poll for reading / error to wait indefinitely. */
    #define NFDS 2
    struct pollfd pfd[NFDS];
    for (int i = 0; i < NFDS; i++) {
        pfd[i].events = POLLERR | POLLHUP | POLLIN;
        pfd[i].revents = 0;
    }
    pfd[0].fd = thread_ctx->sock;
    pfd[1].fd = thread_ctx->pipe_read_fd;

    MONGOC_DEBUG ("recv begin");
    int recv_ret = recv (thread_ctx->sock, buf, 1, 0);
    MONGOC_DEBUG ("recv returned %d", recv_ret);
    MONGOC_DEBUG ("recv end");

    MONGOC_DEBUG ("poll begin, timeout of 30s");
    int poll_ret = poll (pfd, NFDS, 30000);
    MONGOC_DEBUG ("poll end");
    if (poll_ret == -1) {
        MONGOC_DEBUG ("poll failed");
        return NULL;
    }
    for (int i = 0; i < NFDS; i++) {
        if (pfd[i].revents) {
            MONGOC_DEBUG ("fd=%d has events", i);
            print_events (pfd[i].revents);
        }
    }
    return NULL;
}

int main (int argc, char** argv) {
    pthread_t background_thread;
    int pipe_fds[2];
    thread_ctx_t thread_ctx;

    mongoc_init ();

    pipe (pipe_fds);
    thread_ctx.pipe_read_fd = pipe_fds[0];
    thread_ctx.sock = create_socket();
    pthread_create (&background_thread, NULL /* attr */, background_thread_fn, &thread_ctx);

    printf ("Type a character to interrupt the background thread\n");
    /* read from stdin */
    fgetc (stdin);
    // write (pipe_fds[1], "x", 1);
    shutdown (thread_ctx.sock, SHUT_RDWR);

    pthread_join (background_thread, NULL);

    mongoc_cleanup ();
}