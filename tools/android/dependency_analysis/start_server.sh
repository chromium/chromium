#!/usr/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SCRIPT='tools/android/dependency_analysis/generate_json_dependency_graph.py'

if [[ ! -f $SCRIPT  ]]; then
  echo "Run this script from the root of your chromium checkout."
  exit 1
fi

set -e  # Fail on errors.
set -x  # Print command to help with debugging.

# The --prefix flags can be modified locally to include more packages. If you
# feel that a package prefix would benefit more devs, please add more --prefix
# flags.
tools/android/dependency_analysis/generate_json_dependency_graph.py \
  --output="tools/android/dependency_analysis/js/src/json_graph.txt" \
  --prefix="org.chromium." \
  --prefix="com.google.android.apps.chrome." \
  --show-ninja

# Install packages if not already installed.
npm install --prefix tools/android/dependency_analysis/js

# Start the server, it should open a local webpage automatically.
npm run --prefix tools/android/dependency_analysis/js serve
