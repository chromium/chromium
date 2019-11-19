#!/bin/bash

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Reproduces the content of 'components' and 'components-chromium' using the
# list of dependencies from 'bower.json'. Downloads needed packages and makes
# Chromium specific modifications. To launch the script you need 'bower' and
# 'crisper' installed on your system.

check_dep() {
  eval "$1" >/dev/null 2>&1
  if [ $? -ne 0 ]; then
    echo >&2 "This script requires $2."
    echo >&2 "Have you tried $3?"
    exit 1
  fi
}

check_dep "which npm" "npm" "visiting https://nodejs.org/en/"
check_dep "which bower" "bower" "npm install -g bower"
check_dep "which rsync" "rsync" "apt-get install rsync"
check_dep "sed --version | grep GNU" \
    "GNU sed as 'sed'" "'brew install gnu-sed --with-default-names'"

set -e

pushd "$(dirname "$0")" > /dev/null

rm -rf components
rm -rf ../../web-animations-js/sources

bower install --no-color --production

# Update third_party/web-animations-js/ folder.
mkdir -p ../../web-animations-js/sources/
mv components/web-animations-js/web-animations-next-lite.min.js \
  ../../web-animations-js/sources/
mv components/web-animations-js/COPYING \
  ../../web-animations-js/sources/
cp ../../web-animations-js/sources/COPYING ../../web-animations-js/LICENSE
rm -rf components/web-animations-js/

# Remove source mapping directives since we don't compile the maps.
sed -i 's/^\s*\/\/#\s*sourceMappingURL.*//' \
  ../../web-animations-js/sources/*.min.js

rsync -c --delete --delete-excluded -r -v --exclude-from="rsync_exclude.txt" \
    --prune-empty-dirs "components/" "components-chromium/"

find "components-chromium/" -name "*.html" \
  ! -path "components-chromium/polymer2*" | \
  xargs grep -l "<script>" | \
while read original_html_name
do
  echo "Crisping $original_html_name"
  python extract_inline_scripts.py $original_html_name
done

# Remove import of external resource in font-roboto (fonts.googleapis.com)
# and apply additional chrome specific patches. NOTE: Where possible create
# a Polymer issue and/or pull request to minimize these patches.
patch -p1 --forward -r - < chromium.patch

echo 'Minifying Polymer 2, since it comes non-minified from bower.'
python minify_polymer.py

# Undo any changes in paper-ripple, since Chromium's implementation is a fork of
# the original paper-ripple.
git checkout -- components-chromium/paper-ripple/*

# Remove iron-flex-layout-extracted.js since it only contains an unnecessary
# backwards compatibiilty code that was added at
# https://github.com/PolymerElements/iron-flex-layout/commit/f1c967fddbced2ecb5f78456b837fec5117dad14
rm components-chromium/iron-flex-layout/iron-flex-layout-extracted.js

new=$(git status --porcelain components-chromium | grep '^??' | \
      cut -d' ' -f2 | egrep '\.(html|js|css)$' || true)

if [[ ! -z "${new}" ]]; then
  echo
  echo 'These files appear to have been added:'
  echo "${new}" | sed 's/^/  /'
fi

deleted=$(git status --porcelain components-chromium | grep '^.D' | \
          sed 's/^.//' | cut -d' ' -f2 | egrep '\.(html|js|css)$' || true)

if [[ ! -z "${deleted}" ]]; then
  echo
  echo 'These files appear to have been removed:'
  echo "${deleted}" | sed 's/^/  /'
fi

if [[ ! -z "${new}${deleted}" ]]; then
  echo
fi

echo 'Stripping unnecessary prefixed CSS rules...'
python css_strip_prefixes.py --file_extension=html

echo 'Generating -rgb versions of --google-* vars in paper-style/colors.html...'
python rgbify_hex_vars.py --filter-prefix=google --replace \
    components-chromium/paper-styles/color.html

echo 'Creating a summary of components...'
python create_components_summary.py > components_summary.txt

echo 'Creating GN files for interfaces and externs...'
./generate_gn.sh 2 # polymer_version=2

popd > /dev/null

echo 'Searching for unused elements...'
python "$(dirname "$0")"/find_unused_elements.py
