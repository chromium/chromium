#!/bin/bash
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This tool regenerates `wpt_internal/webgpu/cts.html` using the currently
# checked-out version of the WebGPU CTS. It does the following:
#
#   - Extracts the "name" part of every expectation from WebGPUExpectations.
#   - Prepares the WebGPU checkout to run the gen_wpt_cts_html tool.
#   - Runs gen_wpt_cts_html with the appropriate metadata and the extracted
#     expectations names. This generates a cts.html file with the necessary
#     variants to cover the expectations in WebGPUExpectations.
#
# Note: If it fails, it's likely that updates are needed to make
# WebGPUExpectations match the latest CTS (e.g. perhaps test/case names or
# parameters have changed). Consult the error message for details.

set -e

cd "$(dirname "$0")"/../../../..  # cd to [chromium]/src/

expectations=$(mktemp)
trap "{ rm -f $expectations; }" EXIT
echo $expectations

pushd third_party/blink > /dev/null

  echo 'Extracting expectation names...'
  tools/extract_expectation_names.py web_tests/WebGPUExpectations > $expectations

popd > /dev/null

pushd third_party/webgpu-cts/src > /dev/null

  echo 'Updating node for webgpu-cts...'
  npm install --frozen-lockfile
  npx grunt run:generate-listings

  echo 'Regenerating expectations...'
  npm run gen_wpt_cts_html \
    ../../blink/web_tests/wpt_internal/webgpu/cts.html \
    ../../blink/web_tests/webgpu/ctshtml-template.txt \
    ../../blink/web_tests/webgpu/argsprefixes.txt \
    $expectations \
    'wpt_internal/webgpu/cts.html' webgpu

popd > /dev/null
