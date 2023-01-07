#!/bin/bash
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -euxo pipefail

PREFIX="$1"
DEPS_PREFIX="$2"
PWD="$(pwd)"

mkdir -p meson
MESON_PATH="$PWD/meson"

# PipeWire can be only build using Meson. We need Meson only to build PipeWire
# so it doesn't have to be uploaded to CIPD.
export PYTHONPATH="$MESON_PATH/lib/python3.6/site-packages"

# Meson 0.62 and newer will require Python 3.7 and newer and Ubuntu 18.04 has
# only Python 3.6
python3 -m pip install meson==0.61.5 --prefix="$MESON_PATH"

# Do not build Alsa stuff for missing libudev
sed 's@if\ alsa_dep.found()@if\ false@' -i meson.build

export PKG_CONFIG_PATH="$DEPS_PREFIX/lib64/pkgconfig"

$MESON_PATH/bin/meson --buildtype=plain --prefix="$PREFIX" . build -D docs=disabled -D man=disabled -D gstreamer=disabled -D systemd=disabled -D gstreamer-device-provider=disabled -D sdl2=disabled -D audiotestsrc=disabled -D videotestsrc=disabled -D volume=disabled -D bluez5-codec-aptx=disabled -D roc=disabled -D bluez5-codec-lc3plus=disabled -D 'session-managers=[]' -D jack-devel=false -D vulkan=disabled -D libcamera=disabled -D libcanberra=disabled -D pipewire-alsa=disabled -D avb=disabled -D pipewire-v4l2=disabled
$MESON_PATH/bin/meson compile -C build --verbose
$MESON_PATH/bin/meson install -C build --no-rebuild

# We need pkgconfig file to point to location where it's going to be deployed
# and not where it was installed
sed "s@$PREFIX@$DEPS_PREFIX@" -i "$PREFIX/lib64/pkgconfig/libpipewire-0.3.pc"
sed "s@$PREFIX@$DEPS_PREFIX@" -i "$PREFIX/lib64/pkgconfig/libspa-0.2.pc"
