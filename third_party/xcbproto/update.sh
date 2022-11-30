#! /usr/bin/env bash
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o nounset
set -o errexit
set -o xtrace

# Enter third_party/xcbproto
cd "$( dirname "${BASH_SOURCE[0]}" )"

git clone https://gitlab.freedesktop.org/xorg/proto/xcbproto.git

# Generate the patch file.
git -C xcbproto checkout $(cat VERSION)
mv xcbproto/.git xcbproto_git
# diff returns exit code 1, so unset errexit.
set +o errexit
diff -ru xcbproto src > patch.diff
set -o errexit

# Apply the patch file on master.
mv xcbproto_git xcbproto/.git
pushd xcbproto
git checkout -b OldRevision
git apply ../patch.diff
git commit -am 'Apply Chromium patches'
git branch --set-upstream-to=master
git pull --rebase
git rev-parse HEAD~ > ../VERSION
rm -rf .git
popd

# Clean up and move the udpated directory into place.
rm -rf src
mv xcbproto src
