#include "benchmark/benchmark.h"

#include <mongoc/mongoc.h>

#include <cstdlib>
#include <cstring>

#define MONGODB_URI_ENV "MONGODB_URI"

#define MONGODB_ERROR_NOT_FOUND 26
// libmongoc uses a max client pool size of 100 by default.
// Only 100 clients can be checked out of a pool concurrently.
#define MONGOC_DEFAULT_MAX_POOL_SIZE 100

class WorkloadFindFixture : public benchmark::Fixture {
public:
    /* BeforeLoop creates pool_, warms up all client connections, and drops db.coll.
     * May be called by any thread in the benchmark. Skips if not the main thread. */
    void BeforeLoop (benchmark::State& state) {
        bson_error_t error;
        mongoc_uri_t *uri;
        const char *uristr = std::getenv(MONGODB_URI_ENV);
        mongoc_client_t *clients[MONGOC_DEFAULT_MAX_POOL_SIZE];
        mongoc_collection_t *coll;
        int i;
        bson_t * logcmd = BCON_NEW ("setParameter", BCON_INT32(1), "logLevel", BCON_INT32(0));

        // Skip if not the main thread.
        if (state.thread_index() != 0) {
            return;
        }

        uri = uristr ? mongoc_uri_new(uristr) : mongoc_uri_new("mongodb://localhost:27017");
        pool_ = mongoc_client_pool_new(uri);
        mongoc_uri_destroy(uri);

        // Pop all clients and run one operation to open all application connections.
        for (i = 0; i < MONGOC_DEFAULT_MAX_POOL_SIZE; i++)
        {
            clients[i] = mongoc_client_pool_pop(pool_);
            if (!clients[i]) {
                state.SkipWithError("unable to pop client in mongoc_client_pool_pop");
                return; // TODO: do not leak clients.
            }
        }

        // Use one client to drop the db.coll collection.
        coll = mongoc_client_get_collection(clients[0], "db", "coll");
        if (!mongoc_collection_drop(coll, &error) && error.code != MONGODB_ERROR_NOT_FOUND) {
            state.SkipWithError("error in mongoc_collection_drop"); // TODO: include error.message.
            return; // TODO: do not leak clients or coll.
        }
        mongoc_collection_destroy (coll);

        // Disable verbose logging. Verbose logging increases server latency of a single "ping" or "find" operation.
        if (!mongoc_client_command_simple (clients[0], "admin", logcmd, NULL /* read prefs */, NULL /* reply */, &error)) {
            MONGOC_ERROR ("error disabling verbose logging in mongoc_client_command_simple: %s", error.message);
            state.SkipWithError("error disabling verbose logging in mongoc_client_command_simple");
            return; // TODO: do not leak clients or coll.
        }

        for (i = 0; i < MONGOC_DEFAULT_MAX_POOL_SIZE; i++)
        {
            bson_t *cmd = BCON_NEW("ping", BCON_INT32(1));

            if (!mongoc_client_command_simple(clients[i], "db", cmd, NULL /* read_prefs */, NULL /* reply */, &error)) {
                state.SkipWithError("error in mongoc_client_command_simple");
            }
            mongoc_client_pool_push(pool_, clients[i]);
            bson_destroy(cmd);
        }

        bson_destroy (logcmd);
    }
    
    void AfterLoop (benchmark::State& state) {
        if (state.thread_index() == 0) {
            mongoc_client_pool_destroy(pool_);
        }
    }

    mongoc_client_pool_t *pool_;
};

BENCHMARK_DEFINE_F (WorkloadFindFixture, WorkloadFind) (benchmark::State& state) {
    bson_t filter = BSON_INITIALIZER;
    bson_error_t error;
    BCON_APPEND (&filter, "_id", BCON_INT32(0));

    this->BeforeLoop (state);
    for (auto _ : state) {
        mongoc_client_t *client;
        mongoc_collection_t *coll;
        mongoc_cursor_t *cursor;
        const bson_t *doc;

        client = mongoc_client_pool_pop(pool_);
        coll = mongoc_client_get_collection(client, "db", "coll");
        cursor = mongoc_collection_find_with_opts(coll, &filter, NULL /* opts */, NULL /* read_prefs */);
        if (mongoc_cursor_next(cursor, &doc)) {
            state.SkipWithError("unexpected document returned from mongoc_cursor_next");
        }

        if (mongoc_cursor_error(cursor, &error)) {
            state.SkipWithError("error in mongoc_cursor_next"); // TODO: include error.message.
        }

        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(coll);
        mongoc_client_pool_push(pool_, client);
    }
    state.counters["ops_per_sec"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
    this->AfterLoop (state);
    bson_destroy (&filter);
}

BENCHMARK_REGISTER_F (WorkloadFindFixture, WorkloadFind)->
    Unit(benchmark::TimeUnit::kMicrosecond)->
    UseRealTime()->
    ThreadRange(1, 64);
    // ->MinTime(10); - may help with stability.

BENCHMARK_DEFINE_F (WorkloadFindFixture, WorkloadPing) (benchmark::State& state) {
    bson_t cmd = BSON_INITIALIZER;
    bson_error_t error;
    mongoc_read_prefs_t *prefs;

    BCON_APPEND (&cmd, "ping", BCON_INT32(1));
    prefs = mongoc_read_prefs_new (MONGOC_READ_NEAREST);

    this->BeforeLoop (state);
    for (auto _ : state) {
        mongoc_client_t *client;

        client = mongoc_client_pool_pop(pool_);

        if (!mongoc_client_command_simple (client, "db", &cmd, prefs, NULL /* reply */, &error)) {
            state.SkipWithError("error in mongoc_client_command_simple"); // TODO: include error.message.
        }

        mongoc_client_pool_push(pool_, client);
    }
    state.counters["ops_per_sec"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
    this->AfterLoop (state);
    bson_destroy (&cmd);
    mongoc_read_prefs_destroy (prefs);
}

BENCHMARK_REGISTER_F (WorkloadFindFixture, WorkloadPing)->
    Unit(benchmark::TimeUnit::kMicrosecond)->
    UseRealTime()->
    ThreadRange(1, 64);
    // ->MinTime(10); - may help with stability.

class WorkloadFindSingleFixture : public benchmark::Fixture {
public:
    /* BeforeLoop creates pool_, warms up all client connections, and drops db.coll.
     * May be called by any thread in the benchmark. Skips if not the main thread. */
    void BeforeLoop (benchmark::State& state) {
        bson_error_t error;
        mongoc_uri_t *uri;
        const char *uristr = std::getenv(MONGODB_URI_ENV);
        mongoc_collection_t *coll;
        int i;
        bson_t * logcmd = BCON_NEW ("setParameter", BCON_INT32(1), "logLevel", BCON_INT32(0));

        // Skip if not the main thread.
        if (state.thread_index() != 0) {
            return;
        }

        uri = uristr ? mongoc_uri_new(uristr) : mongoc_uri_new("mongodb://localhost:27017");

        // Pop all clients and run one operation to open all application connections.
        for (i = 0; i < MONGOC_DEFAULT_MAX_POOL_SIZE; i++)
        {
            clients_[i] = mongoc_client_new_from_uri(uri);
            if (!clients_[i]) {
                state.SkipWithError("unable to pop client in mongoc_client_pool_pop");
                return; // TODO: do not leak clients.
            }
        }

        // Use one client to drop the db.coll collection.
        coll = mongoc_client_get_collection(clients_[0], "db", "coll");
        if (!mongoc_collection_drop(coll, &error) && error.code != MONGODB_ERROR_NOT_FOUND) {
            state.SkipWithError("error in mongoc_collection_drop"); // TODO: include error.message.
            return; // TODO: do not leak clients or coll.
        }
        mongoc_collection_destroy (coll);

        // Disable verbose logging. Verbose logging increases server latency of a single "ping" or "find" operation.
        if (!mongoc_client_command_simple (clients_[0], "admin", logcmd, NULL /* read prefs */, NULL /* reply */, &error)) {
            MONGOC_ERROR ("error disabling verbose logging in mongoc_client_command_simple: %s", error.message);
            state.SkipWithError("error disabling verbose logging in mongoc_client_command_simple");
            return; // TODO: do not leak clients or coll.
        }

        for (i = 0; i < MONGOC_DEFAULT_MAX_POOL_SIZE; i++)
        {
            bson_t *cmd = BCON_NEW("ping", BCON_INT32(1));

            if (!mongoc_client_command_simple(clients_[i], "db", cmd, NULL /* read_prefs */, NULL /* reply */, &error)) {
                state.SkipWithError("error in mongoc_client_command_simple");
            }
            bson_destroy(cmd);
        }

        bson_destroy (logcmd);

        mongoc_uri_destroy(uri);
    }
    
    void AfterLoop (benchmark::State& state) {
        if (state.thread_index() == 0) {
            for (int i = 0; i < MONGOC_DEFAULT_MAX_POOL_SIZE; i++) {
                mongoc_client_destroy (clients_[i]);
            }
        }
    }

    mongoc_client_t *clients_[MONGOC_DEFAULT_MAX_POOL_SIZE];
};

BENCHMARK_DEFINE_F (WorkloadFindSingleFixture, WorkloadFind) (benchmark::State& state) {
    bson_t filter = BSON_INITIALIZER;
    bson_error_t error;
    mongoc_read_prefs_t *prefs;
    BCON_APPEND (&filter, "_id", BCON_INT32(0));

    prefs = mongoc_read_prefs_new (MONGOC_READ_NEAREST);

    this->BeforeLoop (state);
    for (auto _ : state) {
        mongoc_client_t *client;
        mongoc_collection_t *coll;
        mongoc_cursor_t *cursor;
        const bson_t *doc;

        client = this->clients_[state.thread_index()];
        coll = mongoc_client_get_collection(client, "db", "coll");
        cursor = mongoc_collection_find_with_opts(coll, &filter, NULL /* opts */, prefs);
        if (mongoc_cursor_next(cursor, &doc)) {
            state.SkipWithError("unexpected document returned from mongoc_cursor_next");
        }

        if (mongoc_cursor_error(cursor, &error)) {
            state.SkipWithError("error in mongoc_cursor_next"); // TODO: include error.message.
        }

        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(coll);
    }
    state.counters["ops_per_sec"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
    this->AfterLoop (state);
    bson_destroy (&filter);
    mongoc_read_prefs_destroy (prefs);
}

BENCHMARK_REGISTER_F (WorkloadFindSingleFixture, WorkloadFind)->
    Unit(benchmark::TimeUnit::kMicrosecond)->
    UseRealTime()->
    ThreadRange(1, 64);
    // ->MinTime(10); - may help with stability.

BENCHMARK_DEFINE_F (WorkloadFindSingleFixture, WorkloadPing) (benchmark::State& state) {
    bson_t cmd = BSON_INITIALIZER;
    bson_error_t error;
    mongoc_read_prefs_t *prefs;
    BCON_APPEND (&cmd, "ping", BCON_INT32(1));

    prefs = mongoc_read_prefs_new (MONGOC_READ_NEAREST);

    this->BeforeLoop (state);
    for (auto _ : state) {
        mongoc_client_t *client;

        client = this->clients_[state.thread_index()];
        if (!mongoc_client_command_simple (client, "db", &cmd, prefs, NULL /* reply */, &error)) {
            state.SkipWithError("error in mongoc_cursor_next"); // TODO: include error.message.
        }

    }
    state.counters["ops_per_sec"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
    this->AfterLoop (state);
    bson_destroy (&cmd);
    mongoc_read_prefs_destroy (prefs);
}

BENCHMARK_REGISTER_F (WorkloadFindSingleFixture, WorkloadPing)->
    Unit(benchmark::TimeUnit::kMicrosecond)->
    UseRealTime()->
    ThreadRange(1, 64);
    // ->MinTime(10); - may help with stability.

int main(int argc, char** argv) {                                     
    mongoc_init ();
    benchmark::Initialize(&argc, argv);                               
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 1; 
    benchmark::RunSpecifiedBenchmarks();                              
    benchmark::Shutdown();                                            
    mongoc_cleanup ();
    return 0;                                                           
}