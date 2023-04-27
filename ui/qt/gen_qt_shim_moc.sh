#!/usr/bin/env bash
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o nounset
set -o errexit

URL5="http://archive.debian.org/debian/pool/main/q/qtbase-opensource-src"
PACKAGE5="qtbase5-dev-tools_5.3.2+dfsg-4+deb8u2_amd64.deb"
SHA256_5="7703754f2c230ce6b8b6030b6c1e7e030899aa9f45a415498df04bd5ec061a76"

URL6="http://archive.ubuntu.com/ubuntu/pool/universe/q/qt6-base"
PACKAGE6="qt6-base-dev-tools_6.2.4+dfsg-2ubuntu1_amd64.deb"
SHA256_6="8dddfc79e7743185b07c478ca0f96a4ccc13d48ecccc42f44d2578c33c7d9b8b"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMP_DIR=$(mktemp -d -p "$SCRIPT_DIR")
function cleanup {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

cd "$TMP_DIR"
wget "$URL5/$PACKAGE5"
echo "$SHA256_5  $PACKAGE5" | shasum -a 256 -c
dpkg -x "$PACKAGE5" .
wget "$URL6/$PACKAGE6"
echo "$SHA256_6  $PACKAGE6" | shasum -a 256 -c
dpkg -x "$PACKAGE6" .
cat > ../qt5_shim_moc.cc <<EOF
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

EOF
cd "$SCRIPT_DIR/../.."
cp ui/qt/qt5_shim_moc.cc ui/qt/qt6_shim_moc.cc
"$TMP_DIR/usr/lib/x86_64-linux-gnu/qt5/bin/moc" ui/qt/qt_shim.h \
    >> ui/qt/qt5_shim_moc.cc
"$TMP_DIR//usr/lib/qt6/libexec/moc" ui/qt/qt_shim.h \
    >> ui/qt/qt6_shim_moc.cc
git cl format ui/qt/qt5_shim_moc.cc ui/qt/qt6_shim_moc.cc
