#!/bin/bash

# Copyright 2021 The Chromium Authors
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

replace_section() {
  start_tag="# TAG(reproduce.sh) START_$1"
  end_tag="# TAG(reproduce.sh) END_$1"
  new_text=$2
  file=$3

  sed -e "/${start_tag}/,/${end_tag}/c\\${start_tag}\n${new_text}${end_tag}" $file > /tmp/reproduce_replace_section_output
  mv /tmp/reproduce_replace_section_output $file
}

check_dep "which npm" "npm" "visiting https://nodejs.org/en/"
check_dep "which rsync" "rsync" "installing rsync"
check_dep "which egrep" "egrep" "installing egrep"

pushd "$(dirname "$0")" > /dev/null

rm -rf node_modules

npm install --only=prod

rsync -c --delete --delete-excluded -r -v --prune-empty-dirs \
    --exclude-from="rsync_exclude.txt" \
    "node_modules/" \
    "components-chromium/node_modules/"

npm install

# Replace tslib.js with its ES6 version.
mv components-chromium/node_modules/tslib/tslib.{es6.,}js

# Resolve imports as relative paths so we can load them in chrome://resources/.
find components-chromium/ \
   \( -name "*.js"  \) -type f \
   -exec node resolve_imports.js {} +

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
  echo 'INFO (no action needed): These files appear to have been removed:'
  echo "${deleted}" | sed 's/^/  /'
fi

if [[ ! -z "${new}${deleted}" ]]; then
  echo
fi

echo "Updating build.gn ..."

# In our BUILD file we have a ts_library rule which exposes all lit type
# definitions, update these.
new_text=""
for x in `find components-chromium/node_modules -type f  | grep d.ts$`; do
  new_text+="  \"$x\",\n"
done
replace_section DEFINITIONS "${new_text}" BUILD.gn

# Also update the list of material files.
new_text=""
cd components-chromium/node_modules
for x in `find @material/ \( -name "*.js"  \) -type f`; do
  new_text+="  \"$x\",\n"
done
cd ../..
replace_section MATERIAL_FILES "${new_text}" BUILD.gn

echo "Cleaning up ..."

rm -r "node_modules/"
popd > /dev/null

echo "Done! Thanks for using me :)"
