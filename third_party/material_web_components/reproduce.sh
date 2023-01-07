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
  echo 'These files appear to have been removed:'
  echo "${deleted}" | sed 's/^/  /'
fi

if [[ ! -z "${new}${deleted}" ]]; then
  echo
fi

cat > BUILD.gn << EOF
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import("//tools/typescript/ts_library.gni")
import("//ui/webui/resources/tools/generate_grd.gni")

generate_grd("build_grdp") {
  grd_prefix = "material_web_components"
  out_grd = "\${target_gen_dir}/\${grd_prefix}_resources.grdp"

  # TODO(b/229804752): Clean up and find the minimal set of necessary resources.
  input_files = [
EOF
for x in `find components-chromium/node_modules  | grep js$ | cut -f3- -d/`; do
  echo "    \"$x\"," >> BUILD.gn
done
cat >> BUILD.gn << EOF
  ]

  input_files_base_dir = rebase_path("components-chromium/node_modules", "//")
  resource_path_prefix = "mwc"
}

ts_library("library") {
  composite = true
  tsconfig_base = "tsconfig_base.json"

  # TODO(b/229804752): Clean up and find the minimal set of necessary resources.
  definitions = [
EOF
for x in `find components-chromium/node_modules -type f  | grep d.ts$`; do
  echo "    \"$x\"," >> BUILD.gn
done
cat >> BUILD.gn << EOF
  ]
}
EOF
popd > /dev/null
