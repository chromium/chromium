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

# Meson 0.62 and newer will require Python 3.7 and newer and we have only Python 3.6
python3 -m pip install meson==0.61.5 --prefix="$MESON_PATH"

export PKG_CONFIG_PATH="$DEPS_PREFIX/lib64/pkgconfig"

# # Disable alsa dependency
# sed 's@\(alsa_dep.*\)@#\1@' -i meson.build
# sed 's@alsa_dep,@@' -i src/meson.build

$MESON_PATH/bin/meson --buildtype=plain --prefix="$PREFIX" . build -D docs=disabled -D systemd=disabled -D with-module-sets=
$MESON_PATH/bin/meson compile -C build --verbose
$MESON_PATH/bin/meson install -C build --no-rebuild
