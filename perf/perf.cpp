#include "benchmark/benchmark.h"

#include <mongoc/mongoc.h>

#include <cstdlib>
#include <cstring>

// MONGODB_URI_ENV is the name of an optional environment variable to set a custom URI.
// If it is not set, the default URI is "mongodb://localhost:27017".
#define MONGODB_URI_ENV "MONGODB_URI"

// MONGODB_ERROR_NOT_FOUND is a server error code for "ns not found" to ignore if
// dropping an unknown collection.
#define MONGODB_ERROR_NOT_FOUND 26

// libmongoc uses a max client pool size of 100 by default.
// Only 100 clients can be checked out of a pool concurrently.
#define MONGOC_DEFAULT_MAX_POOL_SIZE 100

// ParallelPoolFixture creates a mongoc_client_pool_t for use in multi-threaded benchmarks.
// The benchmark thread count must not exceed MONGOC_DEFAULT_MAX_POOL_SIZE.
class ParallelPoolFixture : public benchmark::Fixture {
public:
    /* SetUp creates pool_, warms up all client connections, and drops db.coll.
     * May be called by any thread in the benchmark. Skips if not the main thread. */
    virtual void SetUp (benchmark::State& state) {
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
    
    virtual void TearDown (benchmark::State& state) {
        if (state.thread_index() == 0) {
            mongoc_client_pool_destroy(pool_);
        }
    }

    mongoc_client_pool_t *pool_;
};




BENCHMARK_DEFINE_F (ParallelPoolFixture, Ping) (benchmark::State& state) {
    bson_t cmd = BSON_INITIALIZER;
    bson_error_t error;

    BCON_APPEND (&cmd, "ping", BCON_INT32(1));

    for (auto _ : state) {
        mongoc_client_t *client;

        client = mongoc_client_pool_pop(pool_);

        if (!mongoc_client_command_simple (client, "db", &cmd, NULL /* read prefs */, NULL /* reply */, &error)) {
            state.SkipWithError("error in mongoc_client_command_simple"); // TODO: include error.message.
        }

        mongoc_client_pool_push(pool_, client);
    }
    state.counters["ops_per_sec"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
    bson_destroy (&cmd);
}

BENCHMARK_REGISTER_F (ParallelPoolFixture, Ping)->
    Unit(benchmark::TimeUnit::kMicrosecond)->
    UseRealTime()->
    ThreadRange(1, 64);
    // ->MinTime(10); - may help with stability.

// ParallelSingleFixture creates multiple single-threaded mongoc_client_t for use in multi-threaded benchmarks.
class ParallelSingleFixture : public benchmark::Fixture {
public:
    /* SetUp creates pool_, warms up all client connections, and drops db.coll.
     * May be called by any thread in the benchmark. Skips if not the main thread. */
    virtual void SetUp (benchmark::State& state) {
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
                state.SkipWithError("unable to create client");
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
    
    virtual void TearDown (benchmark::State& state) {
        if (state.thread_index() == 0) {
            for (int i = 0; i < MONGOC_DEFAULT_MAX_POOL_SIZE; i++) {
                mongoc_client_destroy (clients_[i]);
            }
        }
    }

    mongoc_client_t *clients_[MONGOC_DEFAULT_MAX_POOL_SIZE];
};




BENCHMARK_DEFINE_F (ParallelSingleFixture, Ping) (benchmark::State& state) {
    bson_t cmd = BSON_INITIALIZER;
    bson_error_t error;
    BCON_APPEND (&cmd, "ping", BCON_INT32(1));


    for (auto _ : state) {
        mongoc_client_t *client;

        client = this->clients_[state.thread_index()];
        if (!mongoc_client_command_simple (client, "db", &cmd, NULL /* read prefs */, NULL /* reply */, &error)) {
            state.SkipWithError("error in mongoc_cursor_next"); // TODO: include error.message.
        }

    }
    state.counters["ops_per_sec"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
    bson_destroy (&cmd);
}

BENCHMARK_REGISTER_F (ParallelSingleFixture, Ping)->
    Unit(benchmark::TimeUnit::kMicrosecond)->
    UseRealTime()->
    ThreadRange(1, 64);
    // ->MinTime(10); - may help with stability.

#include "parallel_pool.h"
class ParallelPoolCInteropFixture : public benchmark::Fixture {
public:
    virtual void SetUp (benchmark::State& state) {
        if (state.thread_index() != 0) {
            return;
        }
        this->fixture = parallel_pool_fixture_new ();
        if (!parallel_pool_fixture_setup (this->fixture)) {
            state.SkipWithError (parallel_pool_fixture_get_error (this->fixture));
        }
    }
    virtual void TearDown (benchmark::State& state) {
        if (state.thread_index() != 0) {
            return;
        }
        if (!parallel_pool_fixture_teardown (this->fixture)) {
            state.SkipWithError (parallel_pool_fixture_get_error (this->fixture));
        }
        parallel_pool_fixture_destroy (this->fixture);
    }
    parallel_pool_fixture_t *fixture;
};

BENCHMARK_DEFINE_F (ParallelPoolCInteropFixture, Ping) (benchmark::State& state) {
    for (auto _ : state) {
        if (!parallel_pool_fixture_ping (fixture, state.thread_index())) {
            state.SkipWithError (parallel_pool_fixture_get_error (this->fixture));
        }
    }
    state.counters["ops_per_sec"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}

BENCHMARK_REGISTER_F (ParallelPoolCInteropFixture, Ping)->
    Unit(benchmark::TimeUnit::kMicrosecond)->
    UseRealTime()->
    ThreadRange(1, 64);

#include "parallel_single.h"
class ParallelSingleCInteropFixture : public benchmark::Fixture {
public:
    virtual void SetUp (benchmark::State& state) {
        if (state.thread_index() != 0) {
            return;
        }
        this->fixture = parallel_single_fixture_new ();
        if (!parallel_single_fixture_setup (this->fixture)) {
            state.SkipWithError (parallel_single_fixture_get_error (this->fixture));
        }
    }
    virtual void TearDown (benchmark::State& state) {
        if (state.thread_index() != 0) {
            return;
        }
        if (!parallel_single_fixture_teardown (this->fixture)) {
            state.SkipWithError (parallel_single_fixture_get_error (this->fixture));
        }
        parallel_single_fixture_destroy (this->fixture);
    }
    parallel_single_fixture_t *fixture;
};

BENCHMARK_DEFINE_F (ParallelSingleCInteropFixture, Ping) (benchmark::State& state) {
    for (auto _ : state) {
        if (!parallel_single_fixture_ping (fixture, state.thread_index())) {
            state.SkipWithError (parallel_single_fixture_get_error (this->fixture));
        }
    }
    state.counters["ops_per_sec"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}

BENCHMARK_REGISTER_F (ParallelSingleCInteropFixture, Ping)->
    Unit(benchmark::TimeUnit::kMicrosecond)->
    UseRealTime()->
    ThreadRange(1, 64);

int main(int argc, char** argv) {                                     
    mongoc_init ();
    benchmark::Initialize(&argc, argv);                               
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 1; 
    benchmark::RunSpecifiedBenchmarks();                              
    benchmark::Shutdown();                                            
    mongoc_cleanup ();
    return 0;                                                           
}