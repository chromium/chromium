#!/usr/bin/env bash
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o nounset
set -o errexit

URL="http://archive.debian.org/debian/pool/main/q/qtbase-opensource-src"
PACKAGE="qtbase5-dev-tools_5.3.2+dfsg-4+deb8u2_amd64.deb"
SHA256="7703754f2c230ce6b8b6030b6c1e7e030899aa9f45a415498df04bd5ec061a76"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

TMP_DIR=$(mktemp -d -p "$SCRIPT_DIR")
function cleanup {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

cd "$TMP_DIR"
wget "$URL/$PACKAGE"
echo "$SHA256  $PACKAGE" | shasum -a 256 -c
dpkg -x "$PACKAGE" .
cat > ../qt_shim_moc.cc <<EOF
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

EOF
cd "$SCRIPT_DIR/../.."
"$TMP_DIR/usr/lib/x86_64-linux-gnu/qt5/bin/moc" ui/qt/qt_shim.h \
    >> ui/qt/qt_shim_moc.cc
git cl format ui/qt/qt_shim_moc.cc
