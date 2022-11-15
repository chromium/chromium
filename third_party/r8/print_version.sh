#!/bin/bash
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cipd_hash=$1
if [[ -z "$cipd_hash" ]]; then
  cd "$(dirname $0)/../.."
  export DEPOT_TOOLS_UPDATE=0
  cipd_hash=$(gclient getdep -r src/third_party/r8:chromium/third_party/r8)
fi
echo "CIPD instance: $cipd_hash"
r8_commit=$(cipd describe chromium/third_party/r8 -version "$cipd_hash" | grep "version:" | grep -P --only-matching '(?<=@).*(?=-)')
echo "R8 commit: $r8_commit"
echo "Recent commits: https://r8.googlesource.com/r8/+log/$r8_commit"
