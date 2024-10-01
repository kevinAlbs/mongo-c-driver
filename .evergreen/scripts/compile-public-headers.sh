#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o xtrace

# shellcheck source=.evergreen/scripts/env-var-utils.sh
. "$(dirname "${BASH_SOURCE[0]}")/env-var-utils.sh"
. "$(dirname "${BASH_SOURCE[0]}")/use-tools.sh" paths

check_var_req CC

check_var_opt C_STD_VERSION
check_var_opt CFLAGS
check_var_opt MARCH

declare script_dir
script_dir="$(to_absolute "$(dirname "${BASH_SOURCE[0]}")")"

declare mongoc_dir
mongoc_dir="$(to_absolute "${script_dir}/../..")"

declare install_dir="${mongoc_dir}/install-dir"

declare -a configure_flags

DIR=$(dirname $0)
. $DIR/find-cmake-latest.sh
CMAKE=$(find_cmake_latest)
. $DIR/check-symlink.sh

# Get the kernel name, lowercased
OS=$(uname -s | tr '[:upper:]' '[:lower:]')
echo "OS: $OS"

if [ "$OS" = "darwin" ]; then
  SO=dylib
  LIB_SO=libmongoc-1.0.0.dylib
  LDD="otool -L"
else
  SO=so
  LIB_SO=libmongoc-1.0.so.0
  LDD=ldd
fi

SRCROOT=$(pwd)
SCRATCH_DIR=$(pwd)/.scratch
rm -rf "$SCRATCH_DIR"
mkdir -p "$SCRATCH_DIR"

cp -r -- "$SRCROOT"/* "$SCRATCH_DIR"

BUILD_DIR=$SCRATCH_DIR/build-dir
rm -rf $BUILD_DIR
mkdir $BUILD_DIR

INSTALL_DIR=$SCRATCH_DIR/install-dir
rm -rf $INSTALL_DIR
mkdir -p $INSTALL_DIR

cd $BUILD_DIR

SNAPPY_CMAKE_OPTION="-DENABLE_SNAPPY=OFF"
SSL_CMAKE_OPTION="-DENABLE_SSL:BOOL=OFF"
STATIC_CMAKE_OPTION="-DENABLE_STATIC=OFF"
TESTS_CMAKE_OPTION="-DENABLE_TESTS=OFF"
ZSTD_CMAKE_OPTION="-DENABLE_ZSTD=AUTO"

configure_flags_append() {
  configure_flags+=("${@:?}")
}

configure_flags_append_if_not_null() {
  declare var="${1:?}"
  if [[ -n "${!var:-}" ]]; then
    shift
    configure_flags+=("${@:?}")
  fi
}

configure_flags_append "-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}"
configure_flags_append "-DCMAKE_PREFIX_PATH=${INSTALL_DIR}/lib/cmake"
configure_flags_append "$SSL_CMAKE_OPTION" "$SNAPPY_CMAKE_OPTION" "$STATIC_CMAKE_OPTION" "$TESTS_CMAKE_OPTION" "$ZSTD_CMAKE_OPTION"

configure_flags_append_if_not_null C_STD_VERSION "-DCMAKE_C_STANDARD=${C_STD_VERSION}"

if [[ "${OSTYPE}" == darwin* && "${HOSTTYPE}" == "arm64" ]]; then
  configure_flags_append "-DCMAKE_OSX_ARCHITECTURES=arm64"
fi

if [[ "${CC}" =~ ^"Visual Studio " ]]; then
  configure_flags_append "-DCMAKE_SYSTEM_VERSION=10.0.20348.0"
fi

declare -a flags

if [[ ! "${CC}" =~ ^"Visual Studio " ]]; then
  case "${MARCH}" in
  i686)
    flags+=("-m32" "-march=i386")
    ;;
  esac

  case "${HOSTTYPE}" in
  s390x)
    flags+=("-march=z196" "-mtune=zEC12")
    ;;
  x86_64)
    flags+=("-m64" "-march=x86-64")
    ;;
  powerpc64le)
    flags+=("-mcpu=power8" "-mtune=power8" "-mcmodel=medium")
    ;;
  esac
fi

if [[ "${CC}" =~ ^"Visual Studio " ]]; then
  flags+=('/wd5105')
fi

# CMake and compiler environment variables.
export CC
export CXX
export CFLAGS
export CXXFLAGS

CFLAGS+=" ${flags+${flags[*]}}"
CXXFLAGS+=" ${flags+${flags[*]}}"

if [[ "${OSTYPE}" == darwin* ]]; then
  CFLAGS+=" -Wno-unknown-pragmas"
fi

case "${CC}" in
clang)
  CXX=clang++
  ;;
gcc)
  CXX=g++
  ;;
esac

# Ensure find-cmake-latest.sh is sourced *before* add-build-dirs-to-paths.sh
# to avoid interfering with potential CMake build configuration.
# shellcheck source=.evergreen/scripts/find-cmake-latest.sh
. "${script_dir}/find-cmake-latest.sh"
declare cmake_binary
cmake_binary="$(find_cmake_latest)"

# shellcheck source=.evergreen/scripts/add-build-dirs-to-paths.sh
. "${script_dir}/add-build-dirs-to-paths.sh"

export PKG_CONFIG_PATH
PKG_CONFIG_PATH="${install_dir}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

if [[ "${OSTYPE}" == darwin* ]]; then
  # MacOS does not have nproc.
  nproc() {
    sysctl -n hw.logicalcpu
  }
fi

echo "CFLAGS: ${CFLAGS}"
echo "configure_flags: ${configure_flags[*]}"

# Use ccache if able.
. "${script_dir:?}/find-ccache.sh"
find_ccache_and_export_vars "$(pwd)" || true

echo "installing..."
"${cmake_binary}" "${configure_flags[@]}" "$SCRATCH_DIR"
"${cmake_binary}" --build . --parallel
"${cmake_binary}" --build . --parallel --target install

echo "compiling..."
"${cmake_binary}" --build . --target test-public-headers -- VERBOSE=1