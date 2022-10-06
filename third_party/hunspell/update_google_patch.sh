#!/bin/bash

# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Creates an updated google.patch that reflects all checked-in changes in the
# current branch. To do this, it
# 1) Checks out the baseline from CVS into a separate /tmp directory.
# 2) Applies all changes third_party/hunspell had since baseline to the CVS dir.
#    2a) Applying google.patch at current branches upstream revision
#    2b) Applying all changes made in the current branch since upstream
# 3) Diffs the updated CVS dir against the checkout made in 1)
#
# No files in the current branch except google.patch will be modified, and
# applying google.patch to the CVS baseline catches baseline up to
# third_party/hunspell.

cvs_dir=""
tempfiles=( )
tmplate="/tmp/`basename $0`.XXXXXX"

# Cleanup function to be executed whenever the script exits.
function cleanup() {
  if [[ $cvs_dir ]]; then
    rm -r "${cvs_dir}"
  fi

  if [[ ${tempfiles[@]} ]]; then
    rm "${tempfiles[@]}"
  fi
  cd ${starting_dir}
}
trap cleanup 0

# Generate a temp file and register it for cleanup
function tempfile() {
  local result=$1
  local tmpfile=$(mktemp ${tmplate}) || exit 1
  tempfiles+=( "${tmpfile}" )
  eval $result="'$tmpfile'"
}

starting_dir=$(pwd)
hunspell_dir=$(dirname $(readlink -e $0))

# Temp file with a list of all excluded files
tempfile filter_file
cat << EOF > ${filter_file}
google.patch
update_google_patch.sh
README.chromium
EOF

# List of all files changed relative to upstream in current branch.
changed_files=$(git --no-pager diff @{u} --name-status | grep -vf ${filter_file} )

# Check we don't actually have files that are added or deleted, because
# that can't be handled by the read-only CVS checkout.
added_files=$( echo "${changed_files}" | grep "^A")
if [[ ${added_files} ]] ; then
  echo "Script cannot handle added files"
  exit 1
fi
deleted_files=$( echo "${changed_files}" | grep "^D")
if [[ ${deleted_files} ]] ; then
  echo "Script cannot handle deleted files"
  exit 1
fi

# Generate patch between branch point from upstream and current HEAD.
diff_files=$( echo "${changed_files}" | grep "^M" | cut -f1 --complement )
tempfile local_patch_file
echo "${diff_files}" | xargs -IXX git --no-pager diff --no-prefix @{u} -- XX > ${local_patch_file}

# Create copy of google.patch at branch point version.
tempfile google_patch_file
git show @{u}:google.patch > ${google_patch_file}

# Create a temporary checkout for CVS hunspell's baseline. All further work
# will happen in this temp directory.
cvs_dir=$(mktemp -d ${tmplate}) || exit 1

# Get CVS hunspell baseline.
cd ${cvs_dir}
echo Checking out CVS version.
cvs -z3 \
  -qd:pserver:anonymous@hunspell.cvs.sourceforge.net:/cvsroot/hunspell \
  co -D "23 Mar 2012" -P hunspell

# Apply google.patch and changes in current branch to CVS hunspell baseline.
cd hunspell
echo Applying google.patch.
patch -p0 -i ${google_patch_file}
echo Applying local patch.
patch -p0 -i ${local_patch_file}

# And generate a new google.patch by diffing modified CVS hunspell against CVS
# hunspell baseline.
echo Updating google.patch.
cvs -q diff -u > ${hunspell_dir}/google.patch

