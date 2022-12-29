#!/bin/bash
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x

PREFIX="$1"
FILENAME=$(ls *.zip)

unzip "$FILENAME" -d "$PREFIX/"

mv "$PREFIX/kotlinc/"* "$PREFIX/"
rmdir "$PREFIX/kotlinc"
