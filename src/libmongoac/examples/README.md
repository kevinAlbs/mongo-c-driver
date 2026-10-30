# hello-mongoac

A minimal example using the MongoDB Async C Driver (`libmongoac`).

Connects and runs `{ping: 1}`

```console
$ MONGODB_URI='mongodb://localhost:27017' ./hello-mongoac
Sending ping from libmongoac ... OK!
```

## Running with RHEL-9 prebuild

```bash
# Everything is contained in $HOME/mongoac-alpha.
# Delete that directory to completely reset and retry.
ALPHA="$HOME/mongoac-alpha"
mkdir -p "$ALPHA"
cd "$ALPHA"

# Download artifacts.
base=https://jira.mongodb.org/secure/attachment
curl -fLO $base/1436234/mongoac-0.1.0-dev-20261030-2dec3d5d8a-x86_64-linux-gnu.tar.xz
curl -fLO $base/1436235/mongoac-0.1.0-dev-20261030-2dec3d5d8a-x86_64-linux-gnu.tar.xz.asc
curl -fLO $base/1436232/hello-mongoac-0.1.0-dev-20261030-2dec3d5d8a.tar.xz
curl -fLO $base/1436233/hello-mongoac-0.1.0-dev-20261030-2dec3d5d8a.tar.xz.asc

# Verify signatures.
curl -fsS https://pgp.mongodb.com/c-driver.pub -o c-driver.pub
gpg --import c-driver.pub
gpg --verify mongoac-0.1.0-dev-20261030-2dec3d5d8a-x86_64-linux-gnu.tar.xz{.asc,}
gpg --verify hello-mongoac-0.1.0-dev-20261030-2dec3d5d8a.tar.xz{.asc,}

# Extract artifacts.
tar -xJf mongoac-0.1.0-dev-20261030-2dec3d5d8a-x86_64-linux-gnu.tar.xz
tar -xJf hello-mongoac-0.1.0-dev-20261030-2dec3d5d8a.tar.xz

# Build example.
cd "$ALPHA/hello-mongoac-0.1.0-dev-20261030-2dec3d5d8a"
cmake -B build -DCMAKE_PREFIX_PATH="$ALPHA/mongoac-0.1.0-dev"
cmake --build build

# Run.
MONGODB_URI='mongodb+srv://<user>:<password>@<cluster>/' ./build/hello-mongoac

# Expected output: Sending ping from libmongoac ... OK!
```
