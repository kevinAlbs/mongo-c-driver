#!/bin/bash
# Build and sign the libmongoac alpha deliverables into the dist/ directory:
#
# - mongoac-<version>-<date>-<sha>-x86_64-linux-gnu.tar.xz : the RHEL 9
#   prebuilt package (mongoac with bundled libbson).
# - hello-mongoac-<version>-<date>-<sha>.tar.xz : the hello-mongoac example
#   project.
# - A detached .asc signature for each tarball.
#
# Usage:
#   ./mongoac-alpha.sh
#
# Must be executed from the mongo-c-driver project root directory.
# Signing requires Garasign credentials and authentication to the DevProd
# ECR registry. Refer to the "Earthly" page of the dev docs (docs/dev/earthly.rst).

set -euo pipefail

VERSION=$(cat src/libmongoac/VERSION_CURRENT)
DATE=$(date +%Y%m%d)
SHA=$(git rev-parse --short HEAD)
PLATFORM=x86_64-linux-gnu # The dagger module pins linux/amd64.

DIST=dist # Output directory (gitignored).

MONGOAC_TARBALL="mongoac-$VERSION-$DATE-$SHA-$PLATFORM.tar.xz"
EXAMPLE_DIR="hello-mongoac-$VERSION-$DATE-$SHA"
EXAMPLE_TARBALL="$EXAMPLE_DIR.tar.xz"

mkdir -p "$DIST"

# 1. Build the RHEL 9 prebuilt package if it is not already built.
if [[ ! -f "$DIST/$MONGOAC_TARBALL" ]]; then
  echo ">>> Building $MONGOAC_TARBALL"
  dagger -M mongoac-almalinux9.dagger
  mv "$MONGOAC_TARBALL" "$DIST/"
fi

# 2. Pack the example project. --no-xattrs keeps macOS extended attributes
# (e.g. com.apple.provenance) out of the archive, which GNU tar on Linux
# would otherwise warn about when extracting.
echo ">>> Packing $EXAMPLE_TARBALL"
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/$EXAMPLE_DIR"
cp src/libmongoac/examples/* "$STAGE/$EXAMPLE_DIR/"
tar --no-xattrs -cJf "$DIST/$EXAMPLE_TARBALL" -C "$STAGE" "$EXAMPLE_DIR"

# 3. Sign both tarballs. earthly exports the signature as
# dist/signature.asc, which is renamed to <file>.asc. The +sign-file target
# verifies the signature against the c-driver public key before saving.
sign() {
  local file=$1
  echo ">>> Signing $file"
  # The trailing slash is required: earthly treats the dest without one as
  # the output file path itself (and fails trying to remove the directory).
  ./tools/earthly.sh --artifact +sign-file/signature.asc "$DIST/" --file="$DIST/$file"
  mv "$DIST/signature.asc" "$DIST/$file.asc"
}

sign "$MONGOAC_TARBALL"
sign "$EXAMPLE_TARBALL"

echo ">>> Done:"
ls -l "$DIST/$MONGOAC_TARBALL" "$DIST/$MONGOAC_TARBALL.asc" "$DIST/$EXAMPLE_TARBALL" "$DIST/$EXAMPLE_TARBALL.asc"
