if [ "$(basename $(pwd))" != "mongo-c-driver" ]; then
    echo "Error: $0 must be run with mongo-c-driver as working directory."
    exit 1
fi

. ./.evergreen/find-cmake.sh

set -o xtrace # TODO: remove

MONGOC_INSTALL_PATH=${MONGOC_INSTALL_PATH:-$(pwd)/perf_install}
SKIP_MONGOC_INSTALL=${SKIP_MONGOC_INSTALL:-OFF}
GOOGLEBENCHMARK_PATH=${GOOGLEBENCHMARK_PATH:-$(pwd)/googlebenchmark}
SKIP_GOOGLEBENCHMARK_INSTALL=${SKIP_GOOGLEBENCHMARK_INSTALL:-OFF}

if [ "$SKIP_MONGOC_INSTALL" = "OFF" ]; then
    echo "Installing release C driver into $MONGOC_INSTALL_PATH"
    mkdir $MONGOC_INSTALL_PATH
    cd $MONGOC_INSTALL_PATH

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
