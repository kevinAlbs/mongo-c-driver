set -o xtrace
cmake \
    -G "Ninja" \
    -DCMAKE_BUILD_TYPE="Debug" \
    -DCMAKE_C_COMPILER="/Users/kevin.albertson/bin/llvm-11.0.0/bin/clang" \
    -DCMAKE_C_COMPILER_LAUNCHER="ccache" \
    -DENABLE_MAINTAINER_FLAGS=ON \
    -DENABLE_EXTRA_ALIGNMENT=OFF \
    -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF \
    -DCMAKE_C_FLAGS="-fsanitize=address -fsanitize=leak -DBSON_MEMCHECK -fno-omit-frame-pointer -Wall" \
    -DCMAKE_VERBOSE_MAKEFILE=OFF \
    -DCMAKE_INSTALL_PREFIX="/Users/kevin.albertson/code/mongo-c-driver-cse-asan/install" \
    -DCMAKE_PREFIX_PATH="/Users/kevin.albertson/code/libmongocrypt-cse-asan/install" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DENABLE_CLIENT_SIDE_ENCRYPTION=ON \
    -DENABLE_SSL=DARWIN \
    -DENABLE_TESTING=ON \
    -DENABLE_DEBUG_ASSERTIONS=ON \
    -DENABLE_BSON=ON \
    -DENABLE_HTML_DOCS=ON \
    -DENABLE_MAN_PAGES=ON \
    -DBUILD_VERSION=1.22.0-pre \
    -S$(pwd) -B$(pwd)/cmake-build
set +o xtrace