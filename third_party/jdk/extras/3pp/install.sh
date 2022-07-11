#!/bin/bash
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

PREFIX="$1"
SUB_DIR="java_8/jre/lib"

# Prepare the directory
mkdir -p "$PREFIX/$SUB_DIR"
# Copy the rt.jar file to $SUB_DIR
cp ./jre/lib/rt.jar "$PREFIX/$SUB_DIR"
