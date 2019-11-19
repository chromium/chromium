#!/bin/bash

# Copyright 2019 The Chromium Authors. All rights reserved.
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
check_dep "which rsync" "rsync" "apt-get install rsync"
check_dep "sed --version | grep GNU" \
    "GNU sed as 'sed'" "'brew install gnu-sed --with-default-names'"

pushd "$(dirname "$0")" > /dev/null

rm -rf node_modules

# Note: The --production flag is omitted, such that devDependencies
# referenced later by this script are also downloaded.
npm install

rsync -c --delete --delete-excluded -r -v --prune-empty-dirs \
    --exclude-from="rsync_exclude.txt" \
    "node_modules/@polymer/" \
    "node_modules/@webcomponents/" \
    "components-chromium/"

# Replace all occurrences of "@polymer/" with "../" or # "../../".
find components-chromium/ -mindepth 2 -maxdepth 2 -name '*.js' \
  -exec sed -i 's/@polymer\//..\//g' {} +
find components-chromium/ -mindepth 3 -maxdepth 3 -name '*.js' \
  -exec sed -i 's/@polymer\//..\/..\//g' {} +

# Replace all occurrences of "@webcomponents/" with "../".
#find . -name '*.js' -exec sed -i 's/@webcomponents\///g' {} +
find components-chromium/polymer/ -mindepth 3 -maxdepth 3 -name '*.js' \
  -exec sed -i 's/@webcomponents\//..\/..\/..\//g' {} +

# Apply additional chrome specific patches.
patch -p1 --forward -r - < chromium.patch

echo 'Minifying Polymer 3, since it comes non-minified from NPM.'
python minify_polymer.py

echo 'Updating paper/iron elements to point to the minified file.'
# Replace all paths that point to within polymer/ to point to the bundle.
find components-chromium/ -name '*.js' -exec sed -i \
  's/\/polymer\/[a-zA-Z\/\.-]\+/\/polymer\/polymer_bundled.min.js/' {} +

# Undo any changes in paper-ripple, since Chromium's implementation is a fork of
# the original paper-ripple.
git checkout -- components-chromium/paper-ripple/*

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

echo 'Stripping unnecessary prefixed CSS rules...'
python ../v1_0/css_strip_prefixes.py --file_extension=js

echo 'Generating -rgb versions of --google-* vars in paper-style/colors.js...'
python ../v1_0/rgbify_hex_vars.py --filter-prefix=google --replace \
    components-chromium/paper-styles/color.js

# TODO create components summary

echo 'Creating GN files for interfaces and externs...'
../v1_0/generate_gn.sh 3 # polymer_version=3

# TODO find unused elements?
