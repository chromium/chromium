#!/bin/bash
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A script to roll sentencepiece dependency and apply Chromium-specific patches.

set -euo pipefail

if [ "$#" -lt 1 ]; then
  echo "Usage: $0 <upstream_dir> [revision]"
  echo "Example: $0 /tmp/sentencepiece_upstream 5e2fb0d93bf235c972548d0891c49191e9960e98"
  exit 1
fi

# Find script directory and paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET_DIR="$SCRIPT_DIR/src"
PATCHES_DIR="$SCRIPT_DIR/patches"

UPSTREAM_DIR="$1"

# 1. Clone upstream repository if it does not exist
if [ ! -d "$UPSTREAM_DIR" ]; then
  git clone https://github.com/google/sentencepiece.git "$UPSTREAM_DIR"
fi

if [ -n "${2:-}" ]; then
  echo "Checking out revision $2..."
  git -C "$UPSTREAM_DIR" checkout "$2"
fi

echo "Cleaning target directory $TARGET_DIR..."
# Remove everything except any files we want to preserve (none, we sync clean)
rm -rf "$TARGET_DIR"
mkdir -p "$TARGET_DIR"

echo "Copying upstream files..."
# Copy only the files we need.
rsync -av \
  --exclude="contrib/docker/" \
  --exclude="contrib/nlcodec/" \
  --exclude="data/" \
  --exclude="python/" \
  --exclude="third_party/absl/" \
  --exclude="third_party/protobuf-lite/" \
  --exclude=".github/" \
  --exclude=".git/" \
  --exclude=".bazelrc" \
  --exclude=".travis.yml" \
  --exclude="BUILD" \
  --exclude="WORKSPACE" \
  "$UPSTREAM_DIR/" "$TARGET_DIR/"

echo "Applying patches..."
# Patch file paths are relative to chromium/src.
cd "$SCRIPT_DIR/../.."

# Patches are sorted alphabetically by default shell expansion
for patch_file in "$PATCHES_DIR"/*.patch; do
  if [ -f "$patch_file" ]; then
    echo "Applying patch: $(basename "$patch_file")"
    git apply "$patch_file"
  fi
done

echo "Updating README.chromium..."
README_PATH="$SCRIPT_DIR/README.chromium"
REVISION=$(git -C "$UPSTREAM_DIR" rev-parse HEAD)
DATE=$(date +%Y-%m-%d)
sed -i "s/^Version: .*/Version: $REVISION/" "$README_PATH"
sed -i "s/^Revision: .*/Revision: $REVISION/" "$README_PATH"
sed -i "s/^Date: .*/Date: $DATE/" "$README_PATH"

echo "Successfully rolled sentencepiece!"
