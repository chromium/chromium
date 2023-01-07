#!/bin/bash
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

base_dir=$(dirname "$0")
abs_base_dir=$(python3 -c "import os; print os.path.abspath('$base_dir')")
toolchain_dir=$abs_base_dir/mock_toolchain
export PATH="$toolchain_dir:$PATH"
echo "Added to PATH: $toolchain_dir"
set -x
exec $base_dir/../main.py archive test.size \
    --map-file $base_dir/test.map \
    --no-output-directory

