#!/bin/bash
#
# Copyright (c) 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Command usage:
# ui/file_manager/base/tools/modules.sh out/Release ui/file_manager/
#   file_manager/foreground/js/list_thumbnail_loader.js
# ui/file_manager/base/tools/modules.sh out/Release ui/file_manager/
#   file_manager/common/js/importer_common.js
# ui/file_manager/base/tools/modules.sh out/Release ui/file_manager/
#   file_manager/foreground/js/list_thumbnail_loader_unittest.js

# Input: js file to convert.
build_dir=$1;
file_path=$2;
dir=`dirname $file_path`;
compiler_output="$build_dir/gen/ui/file_manager/base/tools/compiler_output.txt";

# Create containing directory of `compiler_output` if doesn't exist.
mkdir -p `dirname $compiler_output`;

# Process files with Python.
ui/file_manager/base/tools/modules.py $file_path 'generate';

# Parse closure compiler output and update files until the message 'X error(s),
# Y warning(s), Z% typed' doesn't change.
prev_output=""
new_output="."
while [[ $prev_output != $new_output ]]; do
  prev_output=$new_output;

  # Run closure compiler and save output.
  ninja -C $build_dir $dir:closure_compile 2>&1 | tee $compiler_output;

  # Parse closure compiler output.
  ui/file_manager/base/tools/modules.py $file_path 'parse' $compiler_output;

  # Get number of errors from modules.txt.
  new_output=$(cat $compiler_output | grep 'error(s)' );
done;

# Format files.
git cl format --js;

if [[ $new_output == "" ]]; then
  echo "No closure compiler error found"
else
  echo "Final state: " $new_output;
fi;
