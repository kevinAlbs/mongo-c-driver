function m {
    cd cmake-build-debug---openssl
    make -j8 test-libmongoc
    cd ../
}

function t {
    ./cmake-build-debug---openssl/src/libmongoc/test-libmongoc --no-fork -l /crypt
}
