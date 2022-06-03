#!/bin/bash

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# - Downloads all dependencies listed in package.json
# - Makes Chromium specific modifications.
# - Places the final output in components-chromium/

check_dep() {
  eval "$1" >/dev/null 2>&1
  if [ $? -ne 0 ]; then
    echo >&2 "This script requires $2."
    echo >&2 "Have you tried $3?"
    exit 1
  fi
}

check_dep "which npm" "npm" "visiting https://nodejs.org/en/"
check_dep "which rsync" "rsync" "installing rsync"
check_dep "which egrep" "egrep" "installing egrep"

INSTALLED_PACKAGES=`npm ls -g`

check_npm_dep() {
  echo "$INSTALLED_PACKAGES" | grep " $1@" > /dev/null
  if [ $? -ne 0 ]; then
    echo >&2 "This script requires $1."
    echo >&2 "Have you tried sudo npm install -g $1?"
    echo >&2 "You will also need to set your NODE_PATH"
    exit 1
  fi
}

check_npm_dep "resolve"
check_npm_dep "argparse"

pushd "$(dirname "$0")" > /dev/null

rm -rf node_modules

npm install --only=prod

rsync -c --delete --delete-excluded -r -v --prune-empty-dirs \
    --include-from="rsync_include.txt" \
    --exclude-from="rsync_exclude.txt" \
    "node_modules/" \
    "components-chromium/node_modules/"

# Rewrite imports to relative paths for rollup.
find components-chromium/ \
   \( -name "*.js" -or -name "*.d.ts" \) \
   -exec node rewrite_imports.js {} +

new=$(git status --porcelain components-chromium | grep '^??' | \
      cut -d' ' -f2 | egrep '\.(js|css)$' || true)

if [[ ! -z "${new}" ]]; then
  echo
  echo 'These files appear to have been added:'
  echo "${new}" | sed 's/^/  /'
fi

deleted=$(git status --porcelain components-chromium | grep '^.D' | \
          sed 's/^.//' | cut -d' ' -f2 | egrep '\.(js|css)$' || true)

if [[ ! -z "${deleted}" ]]; then
  echo
  echo 'These files appear to have been removed:'
  echo "${deleted}" | sed 's/^/  /'
fi

if [[ ! -z "${new}${deleted}" ]]; then
  echo
fi

popd > /dev/null
