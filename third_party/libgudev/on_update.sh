#!/usr/bin/env bash
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -euox pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

pushd "$SCRIPT_DIR/src"

TMP_DIR=$(mktemp -d)

meson setup -Dtests=disabled -Dintrospection=disabled -Dvapi=disabled \
  -Dgtk_doc=false --prefix=/usr "$TMP_DIR"

meson compile -C "$TMP_DIR" gudev-1.0

find "$TMP_DIR" -name '*.h' -printf '%P\n' |
  rsync -R --files-from=- "$TMP_DIR/" ../include/

find "$TMP_DIR" -path "$TMP_DIR/meson-private" -prune -o -name '*.c' \
  -printf '%P\n' | rsync -R --files-from=- "$TMP_DIR/" ../

rm -rf "$TMP_DIR"

popd
