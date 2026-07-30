#!/bin/bash
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

PREFIX="$1"
mkdir -p "$PREFIX"

# Copy ktfmt.jar to the package.
cp ktfmt.jar "$PREFIX"

# Add README.chromium file to the package after substituting the version with $_3PP_VERSION.
THIS_DIR=$(dirname "$0")
sed 's/$_3PP_VERSION/'"$_3PP_VERSION"'/' "$THIS_DIR/README.chromium.template" > "$PREFIX/README.chromium"

# Add license file directly from upstream to the package.
curl -s -S -f "https://raw.githubusercontent.com/Kotlin/ktfmt/master/LICENSE" -o "$PREFIX/LICENSE"
