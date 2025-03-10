#!/usr/bin/env bash
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -euox pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

pushd "$SCRIPT_DIR/src"

TMP_DIR=$(mktemp -d)

meson setup -Dlibwacom=false -Ddebug-gui=false -Dtests=false \
 -Dinstall-tests=false -Ddocumentation=false -Dcoverity=false --prefix=/usr \
 "$TMP_DIR"

meson compile -C "$TMP_DIR" input

find "$TMP_DIR" -name '*.h' -printf '%P\n' | while read -r file; do
  sed -i 's|"[^"]*third_party/.*[^"]*"|""|' "$TMP_DIR/$file"
  sed -i 's|"/tmp/.*"|""|' "$TMP_DIR/$file"
  echo "$file"
done | rsync -R --files-from=- "$TMP_DIR/" ../include/

rm -rf "$TMP_DIR"

popd
