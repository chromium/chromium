#!/usr/bin/env bash

set -euox pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/../.."

cd "$SCRIPT_DIR/src"
git fetch origin main
REVISION="$(git rev-parse origin/main)"

cd "$SRC_DIR"
roll-dep src/third_party/fontconfig/src --roll-to "$REVISION" "$@"

cd "$SCRIPT_DIR/src"
meson build -Ddoc=disabled --prefix=/usr
ninja -C build
find build -name '*.h' -printf '%P\n' |
  rsync -R --files-from=- build/ ../include/
# config.h has "#define _GNU_SOURCE" which would conflict with
# our "#define _GNU_SOURCE 1".
sed -i 's/_GNU_SOURCE$/_GNU_SOURCE 1/' ../include/config.h
# Use libxml2 instead of libexpat.  Currently, there's no way
# to configure this with meson options.
echo '#define ENABLE_LIBXML2 1' >>../include/config.h

# Update the README.chromium version.
sed -i "s/^Version: .*/Version: $REVISION/" ../README.chromium

# Update the README.chromium CPE prefix.
cd "$SCRIPT_DIR"
VERSION="$(sed -n "s/^ *version: *'\([0-9.]\+\)'.*/\1/p" src/meson.build)"
CPE="cpe:\/a:fontconfig_project:fontconfig:$VERSION"
sed -i "s/^CPEPrefix: .*/CPEPrefix: $CPE/" README.chromium

# Add the changes to the commit created by roll-dep.
git add include README.chromium
git commit --amend --no-edit
