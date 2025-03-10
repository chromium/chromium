#!/usr/bin/env bash
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -euox pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

pushd "$SCRIPT_DIR/src"

TMP_DIR=$(mktemp -d)

meson setup \
    --prefix=/usr\
    --datadir=/usr/share\
    -Dxwayland=false\
    -Dx11=false\
    -Dremote_desktop=false\
    -Dlibgnome_desktop=false\
    -Dlibwacom=false\
    -Dsound_player=false\
    -Dsm=false\
    -Dintrospection=false\
    -Dcogl_tests=false\
    -Dclutter_tests=false\
    -Dmutter_tests=false\
    -Dtests=disabled\
    -Dprofiler=false\
    -Dinstalled_tests=false\
    -Dfonts=false\
    "$TMP_DIR"

meson compile -C "$TMP_DIR" mutter

# copy generated the toplevel config.h file
cp "$TMP_DIR/config.h" ../include/

# override the hard-coded absolute plugin path and set it to "." with the
# assumption that mutter will be run from the output directory where the plugin
# libdefault.so should be found
sed -i 's/#define MUTTER_PLUGIN_DIR.*/#define MUTTER_PLUGIN_DIR "."/'\
  ../include/config.h

# copy generated cogl headers
find "$TMP_DIR/cogl" -name '*.h' -printf '%P\n' |
  rsync --no-relative --dirs  --files-from=- "$TMP_DIR/cogl/" ../include/cogl/

# generate clutter headers
find "$TMP_DIR/clutter" -name '*.h' -printf '%P\n' | while read -r file; do
  # ensure no absolute path
  sed -i 's|(.*/third_party/mutter/src/\(.*\)|(\1|' "$TMP_DIR/clutter/$file"
  # replace references to clutter/clutter/ with clutter
  sed -i 's|clutter/clutter/|clutter/|' "$TMP_DIR/clutter/$file"
  echo "$file"
done | rsync --no-relative --dirs --files-from=- "$TMP_DIR/clutter/" \
  ../include/clutter/

# copy other headers excluding dbus
find "$TMP_DIR/src" -name 'meta-dbus*' -prune -o -name '*-protocol.*' \
  -prune -o -name '*.h' -printf '%P\n' |
  rsync -R --files-from=- "$TMP_DIR/src/" ../include/

# copy cogl genereated .c files
find "$TMP_DIR/cogl" -name '*.c' -printf '%P\n' | while read -r file; do
  # ensure no absolute path
  sed -i 's|"[^"]*/third_party/mutter/src/\([^"]*\)"|"\1"|' \
    "$TMP_DIR/cogl/$file"
  # replace references to cogl/cogl/ with cogl
  sed -i 's|cogl/cogl/|cogl/|' "$TMP_DIR/cogl/$file"
  echo "$file"
done | rsync --no-relative --dirs --files-from=- $TMP_DIR/cogl/ ../cogl/

# copy clutter genereated .c files
find $TMP_DIR/clutter -name '*.c' -printf '%P\n' | while read -r file; do
  # ensure no absolute path
  sed -i 's|"[^"]*/third_party/mutter/src/\([^"]*\)"|"\1"|' \
    "$TMP_DIR/clutter/$file"
  sed -i 's|(.*/third_party/mutter/src/\(.*\)|(\1|' "$TMP_DIR/clutter/$file"
  # replace references to clutter/clutter/ with clutter
  sed -i 's|clutter/clutter/|clutter/|' "$TMP_DIR/clutter/$file"
  echo "$file"
done | rsync --no-relative --dirs --files-from=- "$TMP_DIR/clutter/" ../clutter/

# copy other genereated .c files excluding dbus
find "$TMP_DIR/src" -name 'meta-dbus*' -prune -o -name '*-protocol.*' \
  -prune -o -name '*.c' -printf '%P\n' | while read -r file; do
  # ensure no absolute path
  sed -i 's|"[^"]*/src/\([^"]*\)"|"\1"|' "$TMP_DIR/src/$file"
  echo "$file"
done | rsync -R --files-from=- "$TMP_DIR/src/" ../

# copy generated headers from "meta"
find "$TMP_DIR/src/meta" -name '*.h' -printf '%P\n' |
  rsync --files-from=- "$TMP_DIR/src/meta/" ../include/meta/


# copy generated dbus files
find "$TMP_DIR/src" -name 'meta-dbus*' -printf '%P\n'

rm -rf "$TMP_DIR"

popd
