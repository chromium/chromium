#!/bin/bash
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

# An auto-created directory whose content will ultimately be uploaded to CIPD.
# The commands below should output the built product to this directory.
PREFIX="$1"

if [ -f "updater/ChromiumUpdater_test.app" ]; then
  mv updater/ChromiumUpdater_test.app "$PREFIX"
elif [ -f "updater/ChromiumUpdater.app" ]; then
  mv updater/ChromiumUpdater.app "$PREFIX"
fi
