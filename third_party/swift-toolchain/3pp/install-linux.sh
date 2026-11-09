#!/bin/bash
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

# The leaf filename of the downloaded package, sans extension.
PACKAGE_NAME="$1"

# An auto-created directory whose content will ultimately be uploaded to CIPD.
# So the commands below should output the built product to this directory.
PREFIX="$2"

tar -xzf raw_source_0.tar.gz
cp -R "$PACKAGE_NAME"/* "$PREFIX"

# Drop parts of the toolchain that Chromium does not use, to keep the
# package small: the Embedded Swift stdlib (~725 MB) and LLDB (~365 MB).
rm -rf "$PREFIX/usr/lib/swift/embedded"
rm -f "$PREFIX"/usr/bin/lldb* "$PREFIX"/usr/lib/liblldb.so*
