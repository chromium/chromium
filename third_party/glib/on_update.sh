#!/usr/bin/env bash
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -euox pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

pushd "$SCRIPT_DIR/src"

TMP_DIR=$(mktemp -d)

meson setup -Dsysprof=disabled -Dtests=false -Dlibmount=disabled --prefix=/usr "$TMP_DIR"
meson compile -C "$TMP_DIR" glib-2.0 gobject-2.0 gio-2.0 gmodule-2.0

find "$TMP_DIR" -name '*.h' -printf '%P\n' | while read -r file; do
  sed -i 's|"[^"]*third_party/glib/src/|"|' "$TMP_DIR/$file"
  echo $file
done | rsync -R --files-from=- "$TMP_DIR/" ../include/

find "$TMP_DIR" -path "$TMP_DIR/meson-private" -prune -o -name '*.c' \
 -printf '%P\n' | while read -r file; do
  sed -i 's|"[^"]*third_party/glib/src/|"|' "$TMP_DIR/$file"
  echo $file
done | rsync -R --files-from=- "$TMP_DIR/" ../

# Disable code paths that require dependencies not available in the sysroot by
# modifying the config.h directly as these don't have corresponding meson
# options. This list may need to be revised if defines for new dependencies
# that are not in sysroot are added when updating glib.
sed -Ei \
  's/#define (HAVE_STRLCPY|HAVE_CLOSE_RANGE|HAVE_STATX|HAVE_COPY_FILE_RANGE).*/#undef \1/'\
  ../include/config.h

rm -rf "$TMP_DIR"

popd
