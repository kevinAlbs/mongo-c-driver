The `perf` directory defines C driver micro benchmarks.

See also: [mongodb/mongo-c-driver-performance](https://github.com/mongodb/mongo-c-driver-performance), which implements [MongoDB Driver Performance Benchmarking](https://github.com/mongodb/specifications/blob/master/source/benchmarking/benchmarking.rst).

# Building and running benchmarks

Install the C driver. Multiple installations on different commits are useful for comparing results.

```bash
cd /path/to/mongo-c-driver
mkdir cmake-build
cd cmake-build

cmake \
    -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF \
    -DENABLE_TESTS=OFF \
    -DENABLE_EXAMPLES=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/path/to/install \
    ..

cmake --build . --config Release --target install
```

Install [Google Benchmark](https://www.google.com/search?q=google+benchmark&rlz=1C5GCEM_enUS917US918&oq=google+benchmark&aqs=chrome..69i57j35i39j69i60l3j69i64l3.5593j0j7&sourceid=chrome&ie=UTF-8)

```bash
git clone https://github.com/google/benchmark.git
cd benchmark
mkdir build
cd build
cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DBENCHMARK_ENABLE_GTEST_TESTS=OFF \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_INSTALL_PREFIX=/path/to/install \
    ..
cmake --build . --config Release --target install
```

Build the `perf` executable.

```bash
cd /path/to/mongo-c-driver/perf
mkdir cmake-build
cd cmake-build

cmake \
    -DCMAKE_PREFIX_PATH="/path/to/install" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=17 \
    ..
cmake --build . --config Release --target perf
```

Run the `perf` executable.

```bash
./perf/cmake-build/perf
```

`perf` accepts CLI arguments from Google Benchmark.

```bash
./perf/cmake-build/perf \
    --benchmark_repetitions=3 \
    --benchmark_min_time=10
```

Pass `--help` to see more.

Use `LD_LIBRARY_PATH` on Linux and `DYLD_LIBRARY_PATH` on macOS to switch the version of the C install loaded. This is useful for comparing two different installations.

```bash
export DYLD_LIBRARY_PATH="$DYLD_LIBRARY_PATH:/path/to/install/1.19.2"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/path/to/install/1.19.2"

./perf/cmake-build/perf
# Outputs results from C 1.19.2.
```

```bash
export DYLD_LIBRARY_PATH="$DYLD_LIBRARY_PATH:/path/to/install/1.20.0"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/path/to/install/1.20.0"

./perf/cmake-build/perf
# Outputs results from C 1.20.0.
```