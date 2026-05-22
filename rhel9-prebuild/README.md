# Build Async C Driver with Pre-built Rust Component

These instructions are for building the async C driver **without a Rust toolchain** on RHEL9.
The Rust component (`libmongoc_rust.a` + `mongoc-rust-private.h`) is pre-built.

## Get prerequisites

- `gcc`, `cmake`, `tar`, `git`, `pkg-config`
- OpenSSL development headers: `sudo dnf install openssl-devel`

No Rust toolchain (`cargo`, `rustc`, `cbindgen`) is required.

## Get pre-built Rust component

Extract the RHEL9 archive:

```bash
# RHEL 9 x86_64
mkdir -p $HOME/mongoc-rust
tar xzf mongoc-rust-rhel9-x86_64.tar.gz --strip-components=1 -C $HOME/mongoc-rust
```

## Clone the driver

```bash
git clone https://github.com/kevinAlbs/mongo-c-driver --branch async-rust-poc
cd mongo-c-driver
```

## Configure

```bash
cmake -S . -B cmake-build \
  -DENABLE_RUST=ON \
  -DENABLE_RUST_SYSTEM=ON \
  -DMONGOC_RUST_LIBRARY=$HOME/mongoc-rust/lib/libmongoc_rust.a \
  -DMONGOC_RUST_INCLUDE_DIR=$HOME/mongoc-rust/include \
  -DCMAKE_INSTALL_PREFIX=$HOME/mongoc-async \
  -DENABLE_TESTS=OFF
```

Expect output to include:
```
-- Using pre-built mongoc-rust library [...]
```

## Install

```bash
cmake --build cmake-build --parallel --target install
```

## Build example

`example-ping.c` runs a "ping" command using the async future API. Compile it against the installed driver:

```bash
export PKG_CONFIG_PATH=$HOME/mongoc-async/lib64/pkgconfig/
gcc -o example-ping example-ping.c $(pkg-config --libs --cflags mongoc2)
```

Run:

```bash
export LD_LIBRARY_PATH=$HOME/mongoc-async/lib64

# Set URI to MongoDB cluster
MONGODB_URI="mongodb://localhost:27017" ./example-ping
```
