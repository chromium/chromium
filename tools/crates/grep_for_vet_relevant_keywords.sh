#!/bin/bash
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script will grep sources of a crate for keywords (e.g. `unsafe`,
# `fs`, "crypto", etc.).  This may be helpful when verifying `cargo vet`
# audit criteria for a crate.

set -eu

usage () {
  echo
  echo "USAGE: ./grep_for_vet_relevant_keywords.sh <crate name>"
  echo
  echo "Example: "
  echo "$ ./grep_for_vet_relevant_keywords.sh quote"
  echo "$ ./grep_for_vet_relevant_keywords.sh syn-2.0.55"
  exit -1
}

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
CHROMIUM_DIR="$SCRIPT_DIR/../.."
VENDOR_DIR="$CHROMIUM_DIR/third_party/rust/chromium_crates_io/vendor"

CRATE_NAME="$1"
if [[ -z "$CRATE_NAME" ]]; then
  echo "ERROR: No crate name has been specified..."
  usage
  exit -1
fi

CRATE_DIR=$( find "${VENDOR_DIR}" -maxdepth 1 \
                                  -type d \
                                  -name "${CRATE_NAME}-[0-9]*"
           )
if [[ -z "$CRATE_DIR" ]]; then
  CRATE_DIR="${VENDOR_DIR}/${CRATE_NAME}"
  if ! [[ -d "$CRATE_DIR" ]]; then
    echo "ERROR: No directories matching the \"$CRATE_NAME\" crate name..."
    usage
    exit -1
  fi
fi
if [[ `echo "$CRATE_DIR" | wc -l` -ne 1 ]]; then
  echo "ERROR: Multiple dirs matching the \"$CRATE_NAME\" crate name..."
  echo "$CRATE_DIR"
  usage
  exit -1
fi

CRATE_DIR=`realpath "$CRATE_DIR" --relative-to="$PWD"`

run_grep () {
  echo "### Grepping $CRATE_DIR for $*..."
  find "$CRATE_DIR" -type f -print0 | xargs -0 grep -n $* || true
  echo
}

run_grep -i cipher
run_grep -i crypto
run_grep '\bfs\b'
run_grep '\bnet\b'
run_grep '\bunsafe\b'
