#!/bin/bash
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generate DEPS's missing include rules for given directory.  Use this for
# ash-chrome only directories under chrome/browser. Files in the directory
# listed in `NO_DIR` array are listed separately to avoid adding dependencies
# that are too generic and broad. See go/ash-chrome-refactor and b/332805865 for
# more information.
#
# How to use: (e.g. chrome/browser/ash/foo/)
# 1. Add "-chrome" to the `include_rules` in chrome/browser/ash/foo/DEPS.
#   (create it if it does not exist)
# 2. Run this script with the directory name.
#      $ ./tools/chromeos/gen_deps.sh chrome/browser/ash/foo
#    It will generate the list of directory and files like
#       "+chrome/browser/profiles",
#       "+chrome/browser/browser_process.h",
# 3. Copy the list to the `include_rules` in DEPS.
# 4. Run checkdeps.py without argument.
#      $ ./buildtools/checkdeps/checkdeps.py
#      Checking: <path-to-git-dir>/src
#      SUCCESS

# Usage: in_dirs <dir> <dir-array>
# Test if 'dir' is in 'dir-array'.
function in_dirs {
  local dir=$1
  shift;
  local dirs=("$@")
  parent=$(dirname $dir)
  parents=()
  while [ "$parent" != "." ]; do
    parents+=($parent)
    parent=$(dirname $parent)
  done
  for d in "${dirs[@]}"; do
    for p in "${parents[@]}"; do
      if [ "$d" == "$p" ]; then
        return 0
      fi
    done
  done
  return -1
}

# use new line when echoing the array
IFS=$'\n'

if [ ${#} -ne 1 ]; then
  echo "Wrong number of arguments".
  echo "`basename $0` <directory>"
  exit
fi

readonly TARGET=${1}
if ! [ -d $TARGET ]; then
  if ! [ -f $TARGET ]; then
    echo "${TARGET} does not exist."
  else
    echo "${TARGET} is not a directory."
  fi
  exit
fi

# Directories whose files should be listed separately.
readonly NO_DIR=("chrome" "chrome/browser" "chrome/browser/ash"
                 "chrome/browser/extensions" "chrome/browser/ui"
                 "chrome/browser/ui/views" "chrome/browser/ui/webui"
                 "chrome/browser/web_applications" "chrome/common")

# Check above directories exist.
for d in ${NO_DIR[@]}; do
  if [ ! -d $d ]; then
    echo "Warning: Directory $d does not exist. " \
      "Please update the directory list NO_DIR in $(basename $0)."
  fi
done

readonly ALL_FILES=( $(./buildtools/checkdeps/checkdeps.py ${TARGET} \
     | grep "Illegal include" | cut -d "\"" -f 2 | sort | uniq) )

# Create directory list.
dirs=(`echo "${ALL_FILES[*]}" | sed "s/\/[^\/]*\.h//" | sort | uniq`)

# Remove directory where the individual files should be listed.
for nd in "${NO_DIR[@]}"; do
  for i in "${!dirs[@]}"; do
    if [ "${dirs[i]}" == "$nd" ]; then
      unset dirs[i]
    fi
  done
done

# List individual files in NO_DIR.
files=()
for d in "${NO_DIR[@]}"; do
  files+=(`echo "${ALL_FILES[*]}" | grep "$d/[^\/]*\.h"`)
done

# Remove sub directories.
for i in "${!dirs[@]}"; do
  if in_dirs "${dirs[i]}" "${dirs[@]}" ; then
    unset dirs[i]
  fi
done

# Combine arrays and sort alphabetically.
files_and_dirs=( "${dirs[@]}" "${files[@]}" )
sorted=($(printf '%s\n' "${files_and_dirs[@]}" | sort))

# Print in DEPS format.
for i in "${sorted[@]}"; do
  echo "  \"+$i\","
done
