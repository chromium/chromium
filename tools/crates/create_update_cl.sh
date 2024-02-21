#!/bin/bash
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script will use the `gnrt` tool to create a draft of a Gerrit CL that
# updates third-party libraries under //third_party/rust.  For more details
# please see `tools/crates/create_update_cl.md`.

set -eu

log () {
  echo -n "### $(date +%X%Z): "
  echo $*
}

log "Changing directory into the root of Chromium..."
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
CHROMIUM_DIR="$SCRIPT_DIR/../.."
cd "$CHROMIUM_DIR"

log "Creating a new git branch..."
BRANCH_NAME="rust-crates-update-$(date +%Y%m%d-%H%M%S)"
if [[ `git status --porcelain` ]]
then
  log "ERROR: Found unexpected git changes before taking any action."
  exit -1
else
  git checkout origin/main -b "$BRANCH_NAME"
  git branch --set-upstream-to=origin/main
fi

log "Downloading Rust toolchain..."
tools/rust/update_rust.py

log "Checking for new crate versions..."
UPDATE_LOG_FILE=$(mktemp --tmpdir=out/gnrt tmp.update_log.$BRANCH_NAME.XXXXXX)
tools/crates/run_gnrt.py update 2>&1 | tee "$UPDATE_LOG_FILE"
if [[ `git status --porcelain` ]]
then
  log "Detected new crate versions."
else
  log "No new crate versions found - no further action needed."
  exit 0
fi

log "Creating a commit message..."
COMMIT_MSG_FILE=$(mktemp --tmpdir=out/gnrt tmp.commit_msg.$BRANCH_NAME.XXXXXX)
cat >"$COMMIT_MSG_FILE" <<EOF
Updating //third_party/rust libraries.

This CL has been created semi-automatically.  The expected review
process and other details can be found at
//tools/crates/create_update_cl.md

Update log:

EOF
cat "$UPDATE_LOG_FILE" | \
      grep 'Updating.* -> ' | \
      sed -e 's/^[[:space:]]*/- /g' \
          >>"$COMMIT_MSG_FILE"
# CL footer:
# * Explicitly asking to test the CL on all Rust bots
# * Explicitly opting out of automatic Regression Test Selection (i.e. asking to
#   run all the tests)
cat >>"$COMMIT_MSG_FILE" <<EOF

Bug: None
Cq-Include-Trybots: chromium/try:android-rust-arm32-rel
Cq-Include-Trybots: chromium/try:android-rust-arm64-dbg
Cq-Include-Trybots: chromium/try:android-rust-arm64-rel
Cq-Include-Trybots: chromium/try:linux-rust-x64-dbg
Cq-Include-Trybots: chromium/try:linux-rust-x64-rel
Cq-Include-Trybots: chromium/try:win-rust-x64-dbg
Cq-Include-Trybots: chromium/try:win-rust-x64-rel
Disable-Rts: True
EOF
COMMIT_MSG=$(cat "$COMMIT_MSG_FILE")

log "Creating a local git commit..."
git add third_party/rust
git commit -m "$COMMIT_MSG"

log "Uploading the CL to Gerrit..."
git cl upload \
  --bypass-hooks --squash --force \
  --hashtag=cratesio-autoupdate \
  --cc=chrome-rust-experiments+autoupdate@google.com
