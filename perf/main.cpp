/*
 * Copyright 2021-present MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "benchmark/benchmark.h"

#include "parallel_pool.h"
#include "parallel_single.h"

class ParallelPoolFixture : public benchmark::Fixture
{
 public:
   virtual void
   SetUp (benchmark::State &state)
   {
      if (state.thread_index () != 0) {
         return;
      }
      this->fixture = parallel_pool_fixture_new ();
      if (!parallel_pool_fixture_setup (this->fixture)) {
         state.SkipWithError (parallel_pool_fixture_get_error (this->fixture));
      }
   }
   virtual void
   TearDown (benchmark::State &state)
   {
      if (state.thread_index () != 0) {
         return;
      }
      if (!parallel_pool_fixture_teardown (this->fixture)) {
         state.SkipWithError (parallel_pool_fixture_get_error (this->fixture));
      }
      parallel_pool_fixture_destroy (this->fixture);
   }
   parallel_pool_fixture_t *fixture;
};

BENCHMARK_DEFINE_F (ParallelPoolFixture, Ping) (benchmark::State &state)
{
   for (auto _ : state) {
      if (!parallel_pool_fixture_ping (fixture, state.thread_index ())) {
         state.SkipWithError (parallel_pool_fixture_get_error (this->fixture));
      }
   }
   state.counters["ops_per_sec"] =
      benchmark::Counter (state.iterations (), benchmark::Counter::kIsRate);
}

BENCHMARK_REGISTER_F (ParallelPoolFixture, Ping)
   ->Unit (benchmark::TimeUnit::kMicrosecond)
   ->UseRealTime ()
   ->ThreadRange (1, 64);

class ParallelSingleFixture : public benchmark::Fixture
{
 public:
   virtual void
   SetUp (benchmark::State &state)
   {
      if (state.thread_index () != 0) {
         return;
      }
      this->fixture = parallel_single_fixture_new ();
      if (!parallel_single_fixture_setup (this->fixture)) {
         state.SkipWithError (
            parallel_single_fixture_get_error (this->fixture));
      }
   }
   virtual void
   TearDown (benchmark::State &state)
   {
      if (state.thread_index () != 0) {
         return;
      }
      if (!parallel_single_fixture_teardown (this->fixture)) {
         state.SkipWithError (
            parallel_single_fixture_get_error (this->fixture));
      }
      parallel_single_fixture_destroy (this->fixture);
   }
   parallel_single_fixture_t *fixture;
};

BENCHMARK_DEFINE_F (ParallelSingleFixture, Ping) (benchmark::State &state)
{
   for (auto _ : state) {
      if (!parallel_single_fixture_ping (fixture, state.thread_index ())) {
         state.SkipWithError (
            parallel_single_fixture_get_error (this->fixture));
      }
   }
   state.counters["ops_per_sec"] =
      benchmark::Counter (state.iterations (), benchmark::Counter::kIsRate);
}

BENCHMARK_REGISTER_F (ParallelSingleFixture, Ping)
   ->Unit (benchmark::TimeUnit::kMicrosecond)
   ->UseRealTime ()
   ->ThreadRange (1, 64);

int
main (int argc, char **argv)
{
   mongoc_init ();
   benchmark::Initialize (&argc, argv);
   if (benchmark::ReportUnrecognizedArguments (argc, argv))
      return 1;
   benchmark::RunSpecifiedBenchmarks ();
   benchmark::Shutdown ();
   mongoc_cleanup ();
   return 0;
}