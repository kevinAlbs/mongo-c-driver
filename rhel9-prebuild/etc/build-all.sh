#!/usr/bin/env bash
# Run on RHEL9. Produces mongoc-async-rhel9-x86_64.tar.gz in the current directory.
#
# Prerequisites:
#   sudo dnf install -y openssl-devel gcc cmake git perl-IPC-Cmd
#   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh   # then reload shell
#   cargo install cbindgen

set -euo pipefail

REPO_URL="https://github.com/kevinAlbs/mongo-c-driver"
BRANCH="async-rust-poc"
WORK=$(mktemp -d)
PREFIX="$WORK/install"
OUT_NAME="mongoc-async-rhel9-x86_64"

echo "==> Cloning $BRANCH ..."
git clone --depth=1 --branch="$BRANCH" "$REPO_URL" "$WORK/repo"
REPO="$WORK/repo"

echo "==> Building libmongoc_rust.a (release) ..."
(cd "$REPO/src/rust" && cargo build --release)
RUST_LIB="$REPO/src/rust/target/release/libmongoc_rust.a"

echo "==> Generating mongoc-rust-private.h ..."
RUST_INC="$WORK/rust-inc"
mkdir -p "$RUST_INC/mongoc"
(cd "$REPO/src/rust" && cbindgen --config cbindgen.toml \
    --output "$RUST_INC/mongoc/mongoc-rust-private.h")

echo "==> Configuring C driver (static, RelWithDebInfo, minimal deps) ..."
cmake -S "$REPO" -B "$WORK/build" \
  -DENABLE_RUST=ON \
  -DENABLE_RUST_SYSTEM=ON \
  -DMONGOC_RUST_LIBRARY="$RUST_LIB" \
  -DMONGOC_RUST_INCLUDE_DIR="$RUST_INC" \
  -DENABLE_STATIC=ON \
  -DENABLE_SHARED=OFF \
  -DENABLE_SASL=OFF \
  -DENABLE_SNAPPY=OFF \
  -DENABLE_ZSTD=OFF \
  -DENABLE_ZLIB=BUNDLED \
  -DENABLE_CLIENT_SIDE_ENCRYPTION=OFF \
  -DENABLE_TESTS=OFF \
  -DENABLE_EXAMPLES=OFF \
  -DENABLE_MAN_PAGES=OFF \
  -DENABLE_HTML_DOCS=OFF \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX="$PREFIX"

echo "==> Building ..."
cmake --build "$WORK/build" --parallel

echo "==> Installing ..."
cmake --install "$WORK/build"

echo "==> Copying libmongoc_rust.a ..."
# Locate the lib dir (lib64 on RHEL9, lib on some distros)
LIB_DIR=$(find "$PREFIX" -maxdepth 1 -type d -name 'lib*' | head -1)
cp "$RUST_LIB" "$LIB_DIR/"

echo "==> Packaging ..."
mv "$PREFIX" "$WORK/$OUT_NAME"
tar czf "$OUT_NAME.tar.gz" -C "$WORK" "$OUT_NAME"

echo ""
echo "Done: $(pwd)/$OUT_NAME.tar.gz"
echo "Libraries:"
tar tzf "$OUT_NAME.tar.gz" | grep '\.a$'
echo "Header dirs:"
tar tzf "$OUT_NAME.tar.gz" | grep 'include/' | grep -v '/' | head -5 || \
  tar tzf "$OUT_NAME.tar.gz" | grep 'include/[^/]*/$' | head -5

rm -rf "$WORK"
