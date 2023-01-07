#!/bin/bash

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cd $(dirname $0)
nyx_absolute_path=$(pwd)
custom_revision="$1"
latest_readme_revision=$(grep -oP '^Revision: \K[^"]+' README.chromium)

tmpdir=$(mktemp -d)
git clone https://chromium.googlesource.com/external/github.com/nyx-fuzz/packer $tmpdir
cd $tmpdir

if [ -z "$custom_revision" ]
  then
    git checkout --quiet $latest_readme_revision
else
    git checkout $custom_revision
fi

latest_remote_revision=$(git rev-parse --verify origin/upstream/main)
git diff --quiet --exit-code $latest_remote_revision -- nyx.h
file_has_changed=$?

if [[ $file_has_changed -eq "1" ]]; then
  read -p "New Nyx-Packer version. Update README (y/n)?" choice
  if [ "$choice" = "y" ]; then
    latest_revision_date=$(date -d @$(git log -n1 --format="%at" $latest_remote_revision) +%Y/%m/%d)
    git checkout --quiet $latest_remote_revision
    cd $nyx_absolute_path

    cp $tmpdir/nyx.h ./nyx.h
    sed -i -e "s@Date: .*@Date: $latest_revision_date@" README.chromium
    sed -i -e "s/Revision: .*/Revision: $latest_remote_revision/" README.chromium

    echo "Nyx-Packer version updated successfully."
  fi
elif [[ $file_has_changed -eq "0" ]]; then
  echo "Nyx-Packer is up to date."
fi

# Make sure the temporary directory gets removed on script exit.
trap 'rm -rf "$tmpdir"' EXIT