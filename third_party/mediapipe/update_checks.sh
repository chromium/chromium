#!/bin/sh
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This files modifies all uses of CHECK/DCHECK to be prefixed with ABSL.

git grep --name-only " CHECK" > /tmp/to_change
git grep --name-only " DCHECK" >> /tmp/to_change
files=`sort -u /tmp/to_change`

if [ -z "$files" ]; then
  echo 'No files to rename'
  exit -1
fi

# mediapipe defines it's own CHECK_OK
# (in src/mediapipe/framework/deps/status.h), which means only some CHECK's need
# to be modified. `check_extensions` is the list to replace. X is replaced with
# the empty string.
check_extensions="_EQ _NE _LE _LT _GE _GT _"

for file in $files
do
  echo "Modifying ${file}"
  for extension in $check_extensions
  do
    if [ $extension = "_" ]; then
      extension=""
    fi
    sed -i -e "s| CHECK${extension}(| ABSL_CHECK${extension}(|g" $file
    sed -i -e "s| DCHECK${extension}(| ABSL_DCHECK${extension}(|g" $file
  done
  # Check to see if we modified the file. Only add the include if we did.
  git diff --quiet $file
  if [ $? -eq 1 ];  then
    check_include=`grep "^#include \"absl/log/absl_check.h\"" $file`
    if [ -z "$check_include" ]; then
      line_number=`grep -n "^#include" $file | tail -1 | awk -F: '{print $1}'`
      if [ ! -z "$line_number" ]; then
        ((line_number=line_number+1))
        echo "Adding include to ${file} at line ${line_number}"
        sed -i "${line_number} i #include \"absl/log/absl_check.h\"" $file
      fi
    fi
  fi
done

echo "WARNING: this script may have added includes to the wrong section (such as"
echo "shaders) be sure to sanity check results."
