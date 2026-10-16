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

# config.h and glibconfig.h are checked in as templates, with the values that
# depend on the glib version replaced by placeholders that are filled in at
# build time (see configure_version_header.py), so that they don't change when
# glib is rolled.
sed -E \
  -e 's/^(#define (GLIB_(MAJOR|MINOR|MICRO)_VERSION|GLIB_INTERFACE_AGE|GLIB_BINARY_AGE)) .*/\1 @\2@/' \
  -e 's/^(#define PACKAGE_STRING) "glib .*"/\1 "glib @VERSION@"/' \
  -e 's/^(#define PACKAGE_VERSION) ".*"/\1 "@VERSION@"/' \
  ../include/config.h > ../include/config.h.in
sed -E 's/^(#define GLIB_(MAJOR|MINOR|MICRO)_VERSION) .*/\1 @GLIB_\2_VERSION@/' \
  ../include/glib/glibconfig.h > ../include/glib/glibconfig.h.in

# Make sure that filling in the templates reproduces meson's output exactly, in
# case upstream changes how it derives these values from the version or adds
# new version-dependent values.
for file in config.h glib/glibconfig.h; do
  python3 ../configure_version_header.py --template "../include/$file.in" \
    --output "$TMP_DIR/filled.h"
  if ! cmp -s "../include/$file" "$TMP_DIR/filled.h"; then
    echo "Filling in include/$file.in doesn't reproduce meson's $file." \
      "Update configure_version_header.py or the templating above." >&2
    diff -u "../include/$file" "$TMP_DIR/filled.h" >&2 || true
    exit 1
  fi
done
rm ../include/config.h ../include/glib/glibconfig.h

rm -rf "$TMP_DIR"

popd
