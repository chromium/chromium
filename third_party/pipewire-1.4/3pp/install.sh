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

meson --buildtype=plain --prefix="$PREFIX" . build  \
      -D docs=disabled -D man=disabled -D gstreamer=disabled -D systemd=disabled \
      -D gstreamer-device-provider=disabled -D sdl2=disabled -D audiotestsrc=disabled \
      -D videotestsrc=disabled -D volume=disabled -D bluez5-codec-aptx=disabled \
      -D roc=disabled -D bluez5-codec-lc3plus=disabled -D 'session-managers=[]' \
      -D jack-devel=false -D vulkan=disabled -D libcamera=disabled -D libcanberra=disabled \
      -D pipewire-alsa=disabled -D alsa=disabled -D avb=disabled -D pipewire-v4l2=disabled \
      -D v4l2=disabled -D pipewire-jack=disabled

meson compile -C build --verbose
meson install -C build --no-rebuild

# We need pkgconfig file to point to location where it's going to be deployed
# and not where it was installed
sed "s@$PREFIX@$DEPS_PREFIX@" -i "$PREFIX/lib64/pkgconfig/libpipewire-0.3.pc"
sed "s@$PREFIX@$DEPS_PREFIX@" -i "$PREFIX/lib64/pkgconfig/libspa-0.2.pc"
