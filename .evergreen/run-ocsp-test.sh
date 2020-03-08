#! /bin/bash
# Test runner for OCSP revocation checking.
#
# Closely models the tests described in the specification:
# https://github.com/mongodb/specifications/tree/master/source/ocsp-support/tests#integration-tests-permutations-to-be-tested.
# Based on the test case, this may start a mock responder process.
# Precondition: mongod is running with the correct configuration.
#
# Environment variables:
#
# TEST_COLUMN
#   Required. Corresponds to a column of the test matrix. Set to one of the following:
#   TEST_1, TEST_2, TEST_3, TEST_4, SOFT_FAIL_TEST, MALICIOUS_SERVER_TEST_1, MALICIOUS_SERVER_TEST_2
# TEST_ROW
#   Required. Corresponds to a row of the test matrix. Set to one of the following:
#   URI_1, URI_2, URI_3
# CERT_TYPE
#   Required. Set to either RSA or EDCSA.
# USE_DELEGATE
#   Optional. May be ON or OFF. If a test requires use of a responder, this decides whether
#   the responder uses a delegate certificate. Defaults to "OFF"
# CDRIVER_BUILD
#   Optional. The path to the build of mongo-c-driver (e.g. mongo-c-driver/cmake-build).
#   Defaults to $(pwd)
# CDRIVER_ROOT
#   Optional. The path to mongo-c-driver source (may be same as CDRIVER_BUILD).
#   Defaults to $(pwd)
# MONGODB_PORT
#   Optional. A custom port to connect to. Defaults to 27017.
# SKIP_PIP_INSTALL
#   Optional. Skip pip install for required packages for mock responder.
#
# Example:
# TEST_COLUMN=TEST_1 TEST_ROW=URI_1 CERT_TYPE=RSA ./run-ocsp-test.sh
#

# Fail on any command returning a non-zero exit status.
# set -o errexit

CDRIVER_ROOT=${CDRIVER_ROOT:-$(pwd)}
CDRIVER_BUILD=${CDRIVER_BUILD:-$(pwd)}
MONGODB_PORT=${MONGODB_PORT-"27017"}
USE_DELEGATE=${USE_DELEGATE:-OFF}

if [ -z "$TEST_COLUMN" -o -z "$TEST_ROW" -o -z "$CERT_TYPE" ]; then
    echo "Required environment variable unset. See file comments for help."
    exit 1;
fi
echo "TEST_COLUMN=$TEST_COLUMN"
echo "TEST_ROW=$TEST_ROW"
echo "CERT_TYPE=$CERT_TYPE"
echo "USE_DELEGATE=$USE_DELEGATE"
echo "CDRIVER_ROOT=$CDRIVER_ROOT"
echo "CDRIVER_BUILD=$CDRIVER_BUILD"
echo "MONGODB_PORT=$MONGODB_PORT"
echo "SKIP_PIP_INSTALL=$SKIP_PIP_INSTALL"

# Make paths absolute
CDRIVER_ROOT=$(cd "$CDRIVER_ROOT"; pwd)
CDRIVER_BUILD=$(cd "$CDRIVER_BUILD"; pwd)

OS=$(uname -s | tr '[:upper:]' '[:lower:]')
case "$(uname -s | tr '[:upper:]' '[:lower:]')" in
    cygwin*) OS="WINDOWS" ;;
    darwin) OS="MACOS" ;;
    *) OS="LINUX" ;;
esac

on_exit () {
    echo "Cleaning up"
    if [ -n "$RESPONDER_REQUIRED" ]; then
        echo "Responder logs:"
        cat $CDRIVER_BUILD/responder.logs
        pkill -f "ocsp_mock" || true
    fi
}
trap on_exit EXIT

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

# Start a mock responder if necessary.
if curl localhost:8100 > /dev/null 2>&1; then
    echo "Detected process listening on port 8100. Attempting to kill running mock responders.";
    pkill -f "ocsp_mock" || true
fi

# Same responder is used for both server and client. So even stapling tests require a responder.
if [ "TEST_1" = "$TEST_COLUMN" ]; then
    RESPONDER_REQUIRED="valid"
elif [ "TEST_2" = "$TEST_COLUMN" ]; then
    RESPONDER_REQUIRED="invalid"
elif [ "TEST_3" = "$TEST_COLUMN" ]; then
    RESPONDER_REQUIRED="valid"
elif [ "TEST_4" = "$TEST_COLUMN" ]; then
    RESPONDER_REQUIRED="invalid"
elif [ "MALICIOUS_SERVER_TEST_1" = "$TEST_COLUMN" ]; then
    RESPONDER_REQUIRED="invalid"
else
    RESPONDER_REQUIRED=""
fi

if [ "ON" = "$USE_DELEGATE" ]; then
    DELEGATE_TOKEN="delegate"
fi

if [ -n "$RESPONDER_REQUIRED" ]; then
    echo "Starting mock responder"
    if [ -z "$SKIP_PIP_INSTALL" ]; then
        echo "Installing python dependencies"
        # Installing dependencies.
        python3 -m venv ./venv
        . ./venv/bin/activate
        pip install oscrypto bottle
    fi
    cd "$CDRIVER_ROOT/.evergreen/ocsp/$CERT_TYPE"
    ./mock-$DELEGATE_TOKEN$RESPONDER_REQUIRED.sh > $CDRIVER_BUILD/responder.logs 2>&1 &
    cd -
fi

echo "Clearing OCSP cache"
if [ "$OS" = "MACOS" ]; then
    find ~/profile/Library/Keychains -name 'ocspcache.sqlite3' -exec sqlite3 "{}" 'DELETE FROM responses' \;
else
    exit 1
fi

# Construct the MONGODB_URI
if [ "URI_1" = "$TEST_ROW" ]; then
    URI_OPTS="tls=true"
elif [ "URI_2" = "$TEST_ROW" ]; then
    URI_OPTS="tls=true&tlsInsecure=true"
elif [ "URI_3" = "$TEST_ROW" ]; then
    URI_OPTS="tls=true&tlsAllowInvalidCertificates=true"
fi

# Always add the tlsCAFile
URI_OPTS="$URI_OPTS&tlsCAFile=$CDRIVER_ROOT/.evergreen/ocsp/$CERT_TYPE/ca.pem"

MONGODB_URI="mongodb://localhost:$MONGODB_PORT/?$URI_OPTS"
echo "Connecting with $MONGODB_URI"

# Only a handful of cases are expected to fail.
if [ "TEST_2" = "$TEST_COLUMN" -a "URI_1" = "$TEST_ROW" ]; then
    expect_failure
    exit 0
fi

expect_success
exit 0