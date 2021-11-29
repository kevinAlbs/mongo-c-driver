if [ "$(basename $(pwd))" != "mongo-c-driver" -a  "$(basename $(pwd))" != "mongoc" ]; then
    echo "Error: $0 must be run with mongo-c-driver or mongoc as working directory."
    exit 1
fi

. ./.evergreen/find-cmake.sh

set -o xtrace # TODO: remove

MONGOC_INSTALL_PATH=${MONGOC_INSTALL_PATH:-$(pwd)/perf_install}
SKIP_MONGOC_INSTALL=${SKIP_MONGOC_INSTALL:-OFF}
GOOGLEBENCHMARK_PATH=${GOOGLEBENCHMARK_PATH:-$(pwd)/googlebenchmark}
SKIP_GOOGLEBENCHMARK_INSTALL=${SKIP_GOOGLEBENCHMARK_INSTALL:-OFF}
SKIP_PERF_BUILD=${SKIP_PERF_BUILD:-OFF}
SKIP_PERF_RUN=${SKIP_PERF_RUN:-OFF}
SKIP_PERF_REPORT=${SKIP_PERF_REPORT:-OFF}

if [ "$SKIP_MONGOC_INSTALL" = "OFF" ]; then
    echo "Installing release C driver into $MONGOC_INSTALL_PATH"
    if [ ! -d $MONGOC_INSTALL_PATH ]; then
        mkdir $MONGOC_INSTALL_PATH
    fi
    pushd $MONGOC_INSTALL_PATH

    $CMAKE \
        -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF \
        -DENABLE_SHM_COUNTERS=OFF \
        -DENABLE_TESTS=OFF \
        -DENABLE_EXAMPLES=OFF \
        -DENABLE_STATIC=ON \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$MONGOC_INSTALL_PATH \
        ..

    $CMAKE --build . --target install
    popd # MONGOC_INSTALL_PATH
fi

if [ "$SKIP_GOOGLEBENCHMARK_INSTALL" = "OFF" ]; then
    echo "Cloning Google Benchmark to $GOOGLEBENCHMARK_PATH"
    git clone https://github.com/google/benchmark.git $GOOGLEBENCHMARK_PATH
    pushd $GOOGLEBENCHMARK_PATH
    mkdir build
    pushd build
    $CMAKE \
        -DCMAKE_BUILD_TYPE=Release \
        -DBENCHMARK_ENABLE_GTEST_TESTS=OFF \
        -DCMAKE_CXX_STANDARD=17 \
        ..
    popd # build
    $CMAKE --build "build" --config Release
    popd # $GOOGLEBENCHMARK_PATH
fi

if [ "$SKIP_PERF_BUILD" = "OFF" ]; then
    echo "Building perf"
    pushd perf
    if [ ! -d cmake-build ]; then
        mkdir cmake-build
    fi
    pushd cmake-build
    $CMAKE \
        -DCMAKE_PREFIX_PATH="$GOOGLEBENCHMARK_PATH/build;$MONGOC_INSTALL_PATH" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=17 \
        ..
    $CMAKE --build . --target perf
    popd # cmake-build
    popd # perf

fi

if [ "$SKIP_PERF_RUN" = "OFF" ]; then
    export DYLD_LIBRARY_PATH="$DYLD_LIBRARY_PATH:$MONGOC_INSTALL_PATH"
    export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$MONGOC_INSTALL_PATH"

    echo "Running perf"
    ./perf/cmake-build/perf \
        --benchmark_out=./perf/googlebenchmark_results.json \
        --benchmark_out_format=json \
        --benchmark_repetitions=3 \
        --benchmark_display_aggregates_only=true \
        --benchmark_counters_tabular=true \
        --benchmark_min_time=10
        # TODO: add min_time
fi

if [ "$SKIP_PERF_REPORT" = "OFF" ]; then
    python3 ./perf/googlebenchmark_to_perfsend.py ./perf/googlebenchmark_results.json > ./perf/perfsend_results.json
fi