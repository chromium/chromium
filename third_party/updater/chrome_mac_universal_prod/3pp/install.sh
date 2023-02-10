#!/bin/bash
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

# An auto-created directory whose content will ultimately be uploaded to CIPD.
# The commands below should output the built product to this directory.
PREFIX="$1"

mv GoogleUpdater.app "$PREFIX"

# Some of the files have nanosecond-resolution timestamps that cause problems
# later on in the build when copied elsewhere. Reset the modification times of
# each file to the current second.
for file in "$PREFIX/"**; do
  touch -m -d $(date -u +"%Y-%m-%dT%H:%M:%SZ") $file;
done;
