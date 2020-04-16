set -o xtrace

export PATH="$PATH:/cygdrive/c/bin/libmongocrypt/bin:/cygdrive/c/code/mongo-c-driver/cmake-build/src/libmongoc/Debug:/cygdrive/c/code/mongo-c-driver/cmake-build/src/libbson/Debug"
export MONGOC_TEST_AWS_SECRET_ACCESS_KEY=""
export MONGOC_TEST_AWS_ACCESS_KEY_ID=""
export MONGOC_TEST_MONITORING_VERBOSE=on
export MONGOC_TEST_MONGOCRYPTD_BYPASS_SPAWN=on

# /cygdrive/c/code/mongo-c-driver/cmake-build/src/libmongoc/Debug/example-client.exe
/cygdrive/c/code/mongo-c-driver/cmake-build/src/libmongoc/Debug/test-libmongoc.exe --no-fork -l "/client_side_encryption/basic" -d 

# for i in $(seq 1 1);
# do
#     if ! /cygdrive/c/code/mongo-c-driver/cmake-build/src/libmongoc/Debug/test-libmongoc.exe --no-fork -l "/client_side_encryption/basic" -d ; then
#         echo "FAILURE DETECTED"
#         break
#     fi
# done
