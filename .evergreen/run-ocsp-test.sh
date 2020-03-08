# Test runner for OCSP revocation checking.
#
# Closely models the tests described in the specification:
# https://github.com/mongodb/specifications/tree/master/source/ocsp-support/tests#integration-tests-permutations-to-be-tested.
# Pass the test case as the required first argument, one of the following:
# "TEST_1", "TEST_2", "TEST_3", "TEST_4", "SOFT_FAIL_TEST", "MALICIOUS_SERVER_TEST_1",
# "MALICIOUS_SERVER_TEST_2"
# Ensure mongod is running with the correct configuration.
# 
# Example:
# run-ocsp-test.sh TEST_1
#
# Optional environment variables:
#
# CDRIVER_BUILD
#   Optional. The path to the build of mongo-c-driver (e.g. mongo-c-driver/cmake-build).
#   Defaults to $(pwd)
# REVOCATION_DISABLED
#   Optional. Default "OFF". Set to "ON" if the MONGODB_URI includes tlsInsecure or tlsAllowInvalidCertificates.
# MONGODB_URI
#   Optional. Defaults to mongodb://localhost:27017/?tls=true.
#

# Fail on any command returning a non-zero exit status.
set -o errexit

TESTCASE=$1
CDRIVER_BUILD=${CDRIVER_BUILD:-$(pwd)}
MONGODB_URI=${MONGODB_URI-"mongodb://localhost:27017/?tls=true"}
REVOCATION_DISABLED=${REVOCATION_DISABLED-"OFF"}

echo "TESTCASE=$TESTCASE"
echo "CDRIVER_BUILD=$CDRIVER_BUILD"
echo "MONGODB_URI=$MONGODB_URI"
echo "REVOCATION_DISABLED=$REVOCATION_DISABLED"

OS=$(uname -s | tr '[:upper:]' '[:lower:]')
case "$(uname -s | tr '[:upper:]' '[:lower:]')" in
    cygwin*) OS="WINDOWS" ;;
    darwin) OS="MACOS" ;;
    *) OS="LINUX" ;;
esac

MONGOC_PING=$CDRIVER_BUILD/src/libmongoc/mongoc-ping

expect_success () {
    echo "Should succeed:"
    if ! $MONGOC_PING $MONGODB_URI; then
        echo "Unexpected failure"
        exit 1
    fi
}

expect_failure () {
    echo "Should fail:"
    if $MONGOC_PING $MONGODB_URI >output.txt 2>&1; then
        echo "Unexpected - succeeded but it should not have"
        exit 1
    else
        echo "failed as expected"
    fi

    # libmongoc really should give a better error message for a revocation failure...
    # It is not at all obvious what went wrong. 
    if ! grep "No suitable servers found" output.txt >/dev/null; then
        echo "Unexpected error, expecting TLS handshake failure"
        cat output.txt
        exit 1
    fi
}

echo "Clearing OCSP cache"
if [ "$OS" = "MACOS" ]; then
    find ~/profile/Library/Keychains -name 'ocspcache.sqlite3' -exec sqlite3 "{}" 'DELETE FROM responses' \;
else
    exit 1
fi

# Only a handful of cases are expected to fail.
if [ "$REVOCATION_DISABLED" = "ON" ]; then
    expect_success
    exit 0
fi

if [ "TEST_1" = "$TESTCASE" ]; then
    expect_success
    exit 0
fi

if [ "TEST_2" = "$TESTCASE" ]; then
    expect_failure
    exit 0
fi

echo "Unexpected testcase '$TESTCASE'"
exit 1