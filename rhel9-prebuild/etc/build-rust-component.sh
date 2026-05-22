# Run this on RHEL9 host to create taball

set -euo pipefail

TMPDIR=$(mktemp -d)

git clone --depth=1 --branch=async-rust-poc https://github.com/kevinAlbs/mongo-c-driver "$TMPDIR"


RUST_DIR="$(cd "$TMPDIR/src/rust" && pwd)"
OUT_DIR="$(pwd)/mongoc-rust-rhel9-x86_64"

echo "==> Building libmongoc_rust.a (release)..."
(cd "$RUST_DIR" && cargo build --release)

echo "==> Generating mongoc-rust-private.h..."
mkdir -p "$OUT_DIR/lib" "$OUT_DIR/include/mongoc"
(cd "$RUST_DIR" && cbindgen --config cbindgen.toml \
    --output "$OUT_DIR/include/mongoc/mongoc-rust-private.h")

echo "==> Copying library..."
cp "$RUST_DIR/target/release/libmongoc_rust.a" "$OUT_DIR/lib/"

echo "==> Packaging..."
tar czf mongoc-rust-rhel9-x86_64.tar.gz mongoc-rust-rhel9-x86_64/

echo ""
echo "Done. Archive: $(pwd)/mongoc-rust-rhel9-x86_64.tar.gz"
echo "  lib/libmongoc_rust.a:               $(du -sh "$OUT_DIR/lib/libmongoc_rust.a" | cut -f1)"
echo "  include/mongoc/mongoc-rust-private.h"

rm -rf $TMPDIR

