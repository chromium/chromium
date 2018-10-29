#!/bin/bash
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Copies compiled feed MockServer test data to chrome/test/data/android/feed directory
# Run once per Feed Library Roll
cd third_party/feed/
if ! [[ `pwd` =~ .*third_party/feed$ ]]; then
  echo "Not in third_party/feed directory: `pwd`"
  exit 1
fi
cp ./src/src/main/java/com/google/android/libraries/feed/mocknetworkclient/test_data/*.gcl.bin ../../chrome/test/data/android/feed/
chmod a-x ../../chrome/test/data/android/feed/*.gcl.bin
