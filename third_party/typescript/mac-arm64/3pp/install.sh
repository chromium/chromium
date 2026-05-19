#!/bin/bash
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x

PREFIX="$1"

# Ensure operating within the expected folder.
if [[ ! -f "package.json" ]]; then
    echo "Missing package.json file."
    exit 1
fi

# Copy relevant parts to output.
cp -a . "$PREFIX"
