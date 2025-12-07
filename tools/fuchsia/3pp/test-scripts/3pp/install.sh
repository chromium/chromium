#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

if [[ -z "$1" ]]; then
  echo "Expect a prefix as the first argument."
  exit 1
fi

PREFIX="$1"

# The internal image version is only used in chromium, and shouldn't be
# published altogether with test scripts.
rm -f linux_internal.sdk.sha1

# Presubmit tests should be executed in chromium.
rm -f PRESUBMIT.py
rm -f test/PRESUBMIT.py

# Code coverage and coding style only apply to chromium.
rm -f test/.coveragerc
rm -f test/.style.yapf

# Use "--parents" to reserve the relative paths, e.g.
# common/py_utils -> $PREFIX/common/py_utils
cp -rf --parents * "$PREFIX"/

