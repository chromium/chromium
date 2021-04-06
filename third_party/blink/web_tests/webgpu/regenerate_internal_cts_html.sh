#!/bin/bash
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This tool regenerates `wpt_internal/webgpu/cts.html` using the currently
# checked-out version of the WebGPU CTS. It does the following:
#
#   - Extracts the "name" part of every expectation from WebGPUExpectations.
#   - Transpiles the WebGPU checkout into a tmp directory to run the
#     gen_wpt_cts_html tool.
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
out_dir=$(mktemp -d)
out_cts_html=$(realpath $(dirname "$0")/../wpt_internal/webgpu/cts.html)

echo "Expectations names: $expectations"
echo "JS output dir: $out_dir"
echo "Output CTS html: $out_cts_html"

function check_output() {
  if [ ! -f $out_cts_html ]; then
    echo "Failed to generate $out_cts_html"
    exit 1
  fi
}

function cleanup() {
  rm -f $expectations
  rm -rf $out_dir
  check_output
}

trap cleanup EXIT

rm -f $out_cts_html

echo 'Extracting expectation names...'
third_party/webgpu-cts/scripts/extract_expectation_names_for_variant_generation.py \
    third_party/blink/web_tests/WebGPUExpectations > $expectations

cat third_party/blink/web_tests/webgpu/internal_cts_test_splits.txt >> $expectations

pushd third_party/webgpu-cts/src > /dev/null

  echo 'Transpiling WebGPU CTS...'
  ../scripts/tsc_ignore_errors.py \
    --project node.tsconfig.json \
    --outDir $out_dir \
    --noEmit false \
    --declaration false \
    --sourceMap false

  echo 'Regenerating cts.html...'
  ../../node/node.py $out_dir/common/tools/gen_wpt_cts_html.js \
    $out_cts_html \
    ../../blink/web_tests/webgpu/ctshtml-template.txt \
    ../../blink/web_tests/webgpu/argsprefixes.txt \
    $expectations \
    'wpt_internal/webgpu/cts.html' webgpu

popd > /dev/null

check_output
