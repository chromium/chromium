#!/usr/bin/env bash
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -euox pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

pushd "$SCRIPT_DIR/src"

TMP_DIR=$(mktemp -d)

meson setup -Dsysprof=disabled -Dtests=false -Dlibmount=disabled --prefix=/usr "$TMP_DIR"

# Only the headers from meson's host configuration checks are checked in. The
# rest of the generated code (enum types, D-Bus code, visibility and version
# macros) is generated at build time from the in-tree glib (see BUILD.gn).
for file in config.h glib/glibconfig.h gmodule/gmoduleconf.h \
    gio/gnetworking.h; do
  sed -i 's|"[^"]*third_party/glib/src/|"|' "$TMP_DIR/$file"
  echo "$file"
done | rsync -R --files-from=- "$TMP_DIR/" ../include/

# Disable code paths that require dependencies not available in the sysroot by
# modifying the config.h directly as these don't have corresponding meson
# options. This list may need to be revised if defines for new dependencies
# that are not in sysroot are added when updating glib.
sed -Ei \
  's/#define (HAVE_STRLCPY|HAVE_CLOSE_RANGE|HAVE_STATX|HAVE_COPY_FILE_RANGE|HAVE_SELINUX|HAVE_FREE_SIZED|HAVE_FREE_ALIGNED_SIZED|HAVE_LIBELF)( .*)?$/#undef \1/'\
  ../include/config.h
sed -Ei 's/#define (G_HAVE_FREE_SIZED)( .*)?$/#undef \1/' \
  ../include/glib/glibconfig.h

rm -rf "$TMP_DIR"

popd
