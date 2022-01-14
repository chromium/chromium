#!/bin/bash
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

# An auto-created directory whose content will ultimately be uploaded to CIPD.
# The commands below should output the built product to this directory.
PREFIX="$1"

if [ -f "updater/UpdaterSetup_test.exe" ]; then
  mv updater/UpdaterSetup_test.exe "$PREFIX"
elif [ -f "updater/UpdaterSetup.exe" ]; then
  mv updater/UpdaterSetup.exe "$PREFIX"
fi
