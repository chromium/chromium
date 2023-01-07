#!/bin/bash

# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Extract preview thumbnails from the raw test images.
rm -f tests.result.txt tests.log
node tests tests.html $* | tee tests.log | \
  grep --text "^test: images/" > tests.result.txt

# Compare their properties to the golden file values.
if [[ $(cmp tests.result.txt images.golden.txt 2>&1) ]]; then
  echo "tests FAIL" || exit 1
else
  echo "tests PASS"
fi
