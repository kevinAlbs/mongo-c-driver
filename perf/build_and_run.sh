if [ "$(basename $(pwd))" != "mongo-c-driver" ]; then
    echo "Error: $0 must be run with mongo-c-driver as working directory."
    exit 1
fi

. ./.evergreen/find-cmake.sh

set -o xtrace # TODO: remove

INSTALL_PATH=${INSTALL_PATH:-$(pwd)/perf_install}
SKIP_INSTALL=${SKIP_INSTALL:-OFF}

# Install a release build.
if [ "$SKIP_INSTALL" = "OFF" ]; then
    echo "Installing release C driver into $INSTALL_PATH"
    mkdir $INSTALL_PATH
    cd $INSTALL_PATH

    $CMAKE \
        -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF \
        -DENABLE_SHM_COUNTERS=OFF \
        -DENABLE_TESTS=OFF \
        -DENABLE_EXAMPLES=OFF \
        -DENABLE_STATIC=ON \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$INSTALL_PATH \
        ..

    $CMAKE --build . --target install
fi