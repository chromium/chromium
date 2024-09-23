#!/bin/bash

# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# - Downloads all dependencies listed in package.json
# - Makes Chromium specific modifications. To make further changes, see
#   /third_party/polymer/README.chromium.
# - Places the final output in components-chromium/

check_dep() {
  eval "$1" >/dev/null 2>&1
  if [ $? -ne 0 ]; then
    echo >&2 "This script requires $2."
    echo >&2 "Have you tried $3?"
    exit 1
  fi
}

set -e

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
find components-chromium/ -mindepth 2 -maxdepth 2 \
  \( -name "*.js" -or -name "*.d.ts" \) \
  -exec sed -i 's/@polymer\//..\//g' {} +
find components-chromium/ -mindepth 3 -maxdepth 3 \
  \( -name "*.js" -or -name "*.d.ts" \) \
  -exec sed -i 's/@polymer\//..\/..\//g' {} +

# Replace all occurrences of "@webcomponents/" with "../".
#find . -name '*.js' -exec sed -i 's/@webcomponents\///g' {} +
find components-chromium/polymer/ -mindepth 3 -maxdepth 3 -name '*.js' \
  -exec sed -i 's/@webcomponents\//..\/..\/..\//g' {} +

# Apply additional chrome specific patches.
patch -p1 --forward < chromium.patch
patch -p1 --forward < iron_icon.patch
patch -p1 --forward < iron_iconset_svg.patch
patch -p1 --forward < iron_list.patch
patch -p1 --forward < iron_overlay_backdrop.patch
patch -p1 --forward < paper_progress.patch
patch -p1 --forward < paper_spinner.patch
patch -p1 --forward < paper_tooltip.patch

echo 'Minifying Polymer 3, since it comes non-minified from NPM.'
./minify_polymer.sh

echo 'Copying TypeScript .d.ts files to the final Polymer directory.'
# Copy all .d.ts files to the final Polymer directory. Note that the order of
# include and exclude flags matters.
rsync -c --delete -r -v --prune-empty-dirs \
    --include="*/" --include="*.d.ts" --exclude="*" \
    "node_modules/@polymer/polymer/" "components-chromium/polymer/"

echo 'Generating polymer.d.ts file for Polymer bundle.'
cp polymer.js components-chromium/polymer/polymer.d.ts

# Apply additional chrome specific patches for the .d.ts files.
patch -p1 --forward -r - < chromium_dts.patch

echo 'Updating paper/iron elements to point to the minified file.'
# Replace all paths that point to within polymer/ to point to the bundle.
find components-chromium/ -name '*.js' -exec sed -i \
  's/\/polymer\/[a-zA-Z\/\.-]\+/\/polymer\/polymer_bundled.min.js/' {} +

# Undo any changes in paper-ripple, since Chromium's implementation is a fork of
# the original paper-ripple.
echo 'Undo changes in paper-ripple and PaperRippleMixin'
git checkout -- components-chromium/paper-ripple/
git checkout -- components-chromium/paper-behaviors/paper-ripple-mixin.js
git checkout -- components-chromium/paper-behaviors/paper-ripple-mixin.d.ts

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
python css_strip_prefixes.py --file_extension=js

# TODO create components summary

echo 'Creating GN files for interfaces and externs...'
./generate_gn.sh
