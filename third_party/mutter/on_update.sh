#!/usr/bin/env bash
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -euox pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

pushd "$SCRIPT_DIR/src"

TMP_DIR=$(mktemp -d)

# Only generated sources are needed from the meson build, so dependencies that
# Chromium provides in-tree (or via the sysroot) don't need to be installed on
# the host. Provide minimal pkg-config files for them instead.
PC_DIR=$(mktemp -d)
SYSROOT_PC_DIR="$SCRIPT_DIR/../../build/linux/debian_bullseye_amd64-sysroot/usr/lib/pkgconfig"
readme_version() {
  sed -n 's/^Version: //p' "$SCRIPT_DIR/../$1/README.chromium"
}
pc_shim() {
  local name="$1" version="$2"
  shift 2
  {
    printf '%s\n' "$@"
    echo "Name: $name"
    echo "Description: pkg-config shim for $name"
    echo "Version: $version"
  } > "$PC_DIR/$name.pc"
}
pc_shim gsettings-desktop-schemas \
  "$(readme_version gsettings-desktop-schemas)"
pc_shim libdisplay-info "$(readme_version libdisplay-info)"
pc_shim libei-1.0 "$(readme_version libei)"
pc_shim libeis-1.0 "$(readme_version libei)"
sysroot_pc_shim() {
  pc_shim "$1" "$(sed -n 's/^Version: //p' "$SYSROOT_PC_DIR/$1.pc")"
}
sysroot_pc_shim colord
# Some config.h defines depend on the xkbcommon version, so use the sysroot's.
sysroot_pc_shim xkbcommon
sysroot_pc_shim xkbregistry
# The in-tree libwayland and wayland-protocols may be newer than the host's.
WAYLAND_VERSION="$(sed -n "s/^\s*version: '\(.*\)',$/\1/p" \
  "$SCRIPT_DIR/../wayland/src/meson.build" | head -n1)"
pc_shim wayland-server "$WAYLAND_VERSION"
pc_shim wayland-client "$WAYLAND_VERSION"
# Meson requires wayland-scanner to match the libwayland version. The
# generated protocol code isn't used (GN generates its own), so wrap the host's
# wayland-scanner and report the in-tree version.
mkdir -p "$PC_DIR/bin"
cat > "$PC_DIR/bin/wayland-scanner" <<EOF
#!/bin/sh
if [ "\$1" = --version ]; then
  echo "wayland-scanner $WAYLAND_VERSION" >&2
  exit 0
fi
exec $(command -v wayland-scanner) "\$@"
EOF
chmod +x "$PC_DIR/bin/wayland-scanner"
pc_shim wayland-scanner "$WAYLAND_VERSION" \
  "wayland_scanner=$PC_DIR/bin/wayland-scanner"
pc_shim wayland-protocols \
  "$(readme_version wayland-protocols | head -n1)" \
  "pkgdatadir=$SCRIPT_DIR/../wayland-protocols/src"
pc_shim udev 252 "udevdir=/usr/lib/udev"
export PKG_CONFIG_PATH="$PC_DIR${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

# Man pages aren't used, so don't require docutils on the host.
mkdir -p "$PC_DIR/bin"
printf '#!/bin/sh\n' > "$PC_DIR/bin/rst2man"
chmod +x "$PC_DIR/bin/rst2man"
export PATH="$PATH:$PC_DIR/bin"

meson setup \
    --prefix=/usr\
    --datadir=/usr/share\
    -Dxwayland=false\
    -Dremote_desktop=false\
    -Dlibgnome_desktop=false\
    -Dlibwacom=false\
    -Dsound_player=false\
    -Dintrospection=false\
    -Dcogl_tests=false\
    -Dclutter_tests=false\
    -Dmutter_tests=false\
    -Dtests=disabled\
    -Dprofiler=false\
    -Dinstalled_tests=false\
    -Dfonts=false\
    "$TMP_DIR"

# Only build the generated sources (meson custom targets). Compiling mutter
# itself would require all of its dependencies to be installed on the host.
meson introspect --targets "$TMP_DIR" | python3 -c '
import json, os, sys
build_dir = sys.argv[1]
for target in json.load(sys.stdin):
  if target["type"] == "custom":
    for f in target["filename"]:
      print(os.path.relpath(f, build_dir))
' "$TMP_DIR" | xargs ninja -C "$TMP_DIR"

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
rsync "$TMP_DIR/src/"meta-dbus* ../data/

rm -rf "$TMP_DIR" "$PC_DIR"

popd
