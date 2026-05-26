# Async C Driver — RHEL9 Pre-built Libraries

Pre-built static libraries for RHEL9 x86_64. No Rust toolchain or C driver build
required — unpack and link.

## Contents of the tarball

```
mongoc-async-rhel9-x86_64/
  lib64/
    libbson2.a
    libmongoc2.a
    libmongoc_rust.a
  include/
    bson-2.3.0/
    mongoc-2.3.0/
```

## Unpack

```bash
mkdir -p $HOME/mongoc-async
tar xzf mongoc-async-rhel9-x86_64.tar.gz --strip-components=1 -C $HOME/mongoc-async
```

## Compile an application

```bash
gcc -o example-ping example-ping.c \
  -I$HOME/mongoc-async/include/bson-2.3.0 \
  -I$HOME/mongoc-async/include/mongoc-2.3.0 \
  -Wl,--whole-archive $HOME/mongoc-async/lib64/libmongoc_rust.a -Wl,--no-whole-archive \
  $HOME/mongoc-async/lib64/libmongoc2.a \
  $HOME/mongoc-async/lib64/libbson2.a \
  -lssl -lcrypto -lpthread -ldl -lm -lresolv
```

`--whole-archive` is required around `libmongoc_rust.a` because the async entry points
(`mongoc_async_*`) are not called from within libmongoc itself, so the linker would
otherwise discard them.

## Run

```bash
MONGODB_URI="mongodb://localhost:27017" ./example-ping
# Ping reply: { "ok" : 1 }
```

## External link dependencies

| Library | Notes |
|---------|-------|
| `libssl`, `libcrypto` | OpenSSL — `sudo dnf install openssl` |
| `libpthread`, `libdl`, `libm`, `libresolv` | glibc — always present on RHEL9 |

## Rebuild from source

See `etc/build-all.sh` to rebuild the tarball on a RHEL9 host with a Rust toolchain.
