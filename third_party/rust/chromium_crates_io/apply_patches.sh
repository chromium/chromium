#!/bin/sh
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Applies patches from the `patches/` directory to the crates in `vendor/`.
#
# Run the script from the //third_party/rust/chromium_crates_io directory.

D=$PWD

cd $D/patches
for crate in *; do
  cd $D/patches/$crate
  for patch in *; do
    cd $D/vendor
    for dir in $crate-*; do
      cd $D/vendor/$dir
      echo In: $PWD
      patch -p6 < ../../patches/$crate/$patch
    done
  done
done
cd $D
