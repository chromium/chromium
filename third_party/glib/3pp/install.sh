#!/bin/bash
# Copyright 2022-2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -euxo pipefail

echo "Building on the following system: "
uname -a
uname -r

PREFIX="$1"
DEPS_PREFIX="$2"
PWD="$(pwd)"

mkdir -p helper_pkgs
HELPER_PKGS_PATH="$PWD/helper_pkgs"

echo "Installing dependencies to build PipeWire"

# Will install into /work/tools_prefix, since that's where Python is.
python3 -m pip install --upgrade pip
python3 -m pip install meson

export PATH="$HELPER_PKGS_PATH/bin:$PATH"

meson --buildtype=plain --prefix="$PREFIX" . build  \
      -D glib_debug=disabled -D documentation=false -D installed_tests=false --default-library=both

meson compile -C build --verbose
meson install -C build --no-rebuild

# We need pkgconfig file to point to location where it's going to be deployed
# and not where it was installed
sed "s@$PREFIX@$DEPS_PREFIX@" -i "$PREFIX/lib64/pkgconfig/gobject-2.0.pc"
sed "s@$PREFIX@$DEPS_PREFIX@" -i "$PREFIX/lib64/pkgconfig/gthread-2.0.pc"
sed "s@$PREFIX@$DEPS_PREFIX@" -i "$PREFIX/lib64/pkgconfig/gio-2.0.pc"
sed "s@$PREFIX@$DEPS_PREFIX@" -i "$PREFIX/lib64/pkgconfig/gio-unix-2.0.pc"
sed "s@$PREFIX@$DEPS_PREFIX@" -i "$PREFIX/lib64/pkgconfig/glib-2.0.pc"
