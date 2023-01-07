#!/bin/bash

# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Fetch the test images into ./images if that directory does not exist.
[[ -d images ]] && exit 0

read -p "npm run test needs test images: install them now (y/n)? " ANSWER
case ${ANSWER} in
  [Yy]*) git clone https://github.com/noell/raw-test-images.git images
    exit $!;;
  [Nn]*|*) ;;
esac
