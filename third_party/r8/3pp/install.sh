#!/bin/bash
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

PREFIX="$1"
DEPS_PREFIX="$2"

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

# Download depot_tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git $SCRIPT_DIR/depot_tools

# Build
PATH=$SCRIPT_DIR/depot_tools:$PATH tools/gradle.py r8

# Shrink (improves r8/d8 launch time):
# Needs the -D flag to avoid compilation error, see http://b/311202383.
$DEPS_PREFIX/current/bin/java -Dcom.android.tools.r8.enableKeepAnnotations=1 \
    -jar build/libs/r8.jar --debug --classfile --output r8.jar \
    --lib $DEPS_PREFIX/current --pg-conf src/main/keep.txt \
    --no-minification --no-desugaring build/libs/r8.jar

mkdir -p "$PREFIX/lib"
cp r8.jar "$PREFIX/lib"
