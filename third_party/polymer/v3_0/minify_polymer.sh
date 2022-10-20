#!/bin/bash

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Minifies Polymer 3, since it does not come already minified from NPM. This
# script is meant to run as part of reproduce.sh.

# Without the local modifications below polymer_bundled.min.js is 109KB, as
# opposed to 89KB after the modifications.

# Patch a few local changes that are later leveraged by Rollup and Terser to
# prune out unnecessary code.
patch -p1 --forward < polymer.patch

# Make a few string replacements that also help reduce size slightly.

FOLDERS_TO_PROCESS="components-chromium/polymer components-chromium/shadycss"

# 1) Replace all `window['ShadyDOM']` with `window.ShadyDOM`
git grep -l --no-index "window\['ShadyDOM'\]" -- $FOLDERS_TO_PROCESS | \
    xargs sed -i "s/window\['ShadyDOM'\]/window.ShadyDOM/g"

# 2) Replace all `window['ShadyCSS']` with `window.ShadyCSS`
git grep -l --no-index "window\['ShadyCSS'\]" -- $FOLDERS_TO_PROCESS | \
    xargs sed -i "s/window\['ShadyCSS'\]/window.ShadyCSS/g"

# 3) Do the rest of the work using Python.
python minify_polymer.py
