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
export PKG_CONFIG_PATH="$DEPS_PREFIX/lib64/pkgconfig:$PKG_CONFIG_PATH"
export LD_LIBRARY_PATH="$DEPS_PREFIX/lib64:$LD_LIBRARY_PATH"

meson --buildtype=plain --prefix="$PREFIX" . build  \
      -D system-lua=false -D doc=disabled -D systemd=disabled \
      -D systemd-user-service=false -D introspection=disabled \
      -D elogind=disabled

meson compile -C build --verbose
meson install -C build --no-rebuild

