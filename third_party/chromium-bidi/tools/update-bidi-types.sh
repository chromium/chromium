#!/bin/bash

# @license
# Copyright 2026 Google Inc.
# SPDX-License-Identifier: Apache-2.0

set -e

# Change directory to the package root so the script works from any directory
cd "$(cd "$(dirname "$0")/.." && pwd)"

# Default parameters (matching workflow inputs)
SPEC_REPO="w3c/webdriver-bidi"
SPEC_REF="main"

# Check cargo / cddlconv dependency
REQUIRED_CDDLCONV_VERSION="0.1.10"
if ! command -v cddlconv &> /dev/null; then
  echo "Error: 'cddlconv' is required but not installed." >&2
  echo "Please install it using cargo: 'cargo install cddlconv@$REQUIRED_CDDLCONV_VERSION'" >&2
  exit 1
fi

CDDLCONV_VERSION=$(cddlconv --version | awk '{print $2}')
if [ "$CDDLCONV_VERSION" != "$REQUIRED_CDDLCONV_VERSION" ]; then
  echo "Error: 'cddlconv' version $REQUIRED_CDDLCONV_VERSION is required, but found $CDDLCONV_VERSION." >&2
  echo "Please install it using cargo: 'cargo install cddlconv@$REQUIRED_CDDLCONV_VERSION'" >&2
  exit 1
fi

# Check parse5 dependency (required by webdriver-bidi cddl generator)
if command -v npm &> /dev/null; then
  GLOBAL_NODE_MODULES="$(npm root -g 2>/dev/null || true)"
  if [ -n "$GLOBAL_NODE_MODULES" ]; then
    export NODE_PATH="$GLOBAL_NODE_MODULES:${NODE_PATH:-}"
  fi
fi

if ! node -e "require('parse5')" &> /dev/null; then
  echo "Error: 'parse5' is required but not installed." >&2
  echo "Please install it using npm: 'npm install -g parse5'" >&2
  exit 1
fi

# Setup temporary build directory
BUILD_DIR=$(mktemp -d -t bidi-types-build-XXXXXX)
echo "Created temporary build directory at: $BUILD_DIR"

# Clean up on exit or error
cleanup() {
  echo "Cleaning up temporary build directory..."
  rm -rf "$BUILD_DIR"
}
trap cleanup EXIT

# 1. Build main spec cddl types
echo "Checking out $SPEC_REPO ($SPEC_REF)..."
git clone --depth 1 "https://github.com/$SPEC_REPO.git" "$BUILD_DIR/webdriver-bidi"
(
  cd "$BUILD_DIR/webdriver-bidi"
  git checkout "$SPEC_REF" 2>/dev/null || true
  node ./scripts/cddl/generate.js
)
cp "$BUILD_DIR/webdriver-bidi/all.cddl" ./all.cddl

# 2. Build Permissions CDDL
echo "Checking out w3c/permissions..."
git clone --depth 1 https://github.com/w3c/permissions.git "$BUILD_DIR/permissions"
(
  cd "$BUILD_DIR/permissions"
  node ../webdriver-bidi/scripts/cddl/generate.js ./index.html && mv all.cddl permissions.cddl
)
cp "$BUILD_DIR/permissions/permissions.cddl" ./permissions.cddl

# 3. Build Bluetooth CDDL
echo "Checking out WebBluetoothCG/web-bluetooth..."
git clone --depth 1 https://github.com/WebBluetoothCG/web-bluetooth.git "$BUILD_DIR/web-bluetooth"
(
  cd "$BUILD_DIR/web-bluetooth"
  node ../webdriver-bidi/scripts/cddl/generate.js ./index.bs && mv all.cddl web-bluetooth.cddl
)
cp "$BUILD_DIR/web-bluetooth/web-bluetooth.cddl" ./web-bluetooth.cddl

# 4. Build Speculation CDDL
echo "Checking out WICG/nav-speculation..."
git clone --depth 1 https://github.com/WICG/nav-speculation.git "$BUILD_DIR/nav-speculation"
(
  cd "$BUILD_DIR/nav-speculation"
  node ../webdriver-bidi/scripts/cddl/generate.js ./prefetch.bs && mv all.cddl nav-speculation.cddl
)
cp "$BUILD_DIR/nav-speculation/nav-speculation.cddl" ./nav-speculation.cddl

# 5. Build UA Client Hints CDDL
echo "Checking out WICG/ua-client-hints..."
git clone --depth 1 https://github.com/WICG/ua-client-hints.git "$BUILD_DIR/ua-client-hints"
(
  cd "$BUILD_DIR/ua-client-hints"
  node ../webdriver-bidi/scripts/cddl/generate.js ./index.bs && mv all.cddl ua-client-hints.cddl
)
cp "$BUILD_DIR/ua-client-hints/ua-client-hints.cddl" ./ua-client-hints.cddl

# 6. Build Digital Credentials CDDL
echo "Checking out w3c-fedid/digital-credentials..."
git clone --depth 1 https://github.com/w3c-fedid/digital-credentials.git "$BUILD_DIR/digital-credentials"
(
  cd "$BUILD_DIR/digital-credentials"
  node ../webdriver-bidi/scripts/cddl/generate.js ./index.html && mv all.cddl digital-credentials.cddl
)
cp "$BUILD_DIR/digital-credentials/digital-credentials.cddl" ./digital-credentials.cddl

# 7. Generate TypeScript and Zod types from CDDL files
echo "Generating types..."
node tools/generate-bidi-types.mjs --cddl-file all.cddl
node tools/generate-bidi-types.mjs --cddl-file permissions.cddl --ts-file src/protocol/generated/webdriver-bidi-permissions.ts --zod-file src/protocol-parser/generated/webdriver-bidi-permissions.ts
node tools/generate-bidi-types.mjs --cddl-file web-bluetooth.cddl --ts-file src/protocol/generated/webdriver-bidi-bluetooth.ts --zod-file src/protocol-parser/generated/webdriver-bidi-bluetooth.ts
node tools/generate-bidi-types.mjs --cddl-file nav-speculation.cddl --ts-file src/protocol/generated/webdriver-bidi-nav-speculation.ts --zod-file src/protocol-parser/generated/webdriver-bidi-nav-speculation.ts
node tools/generate-bidi-types.mjs --cddl-file ua-client-hints.cddl --ts-file src/protocol/generated/webdriver-bidi-ua-client-hints.ts --zod-file src/protocol-parser/generated/webdriver-bidi-ua-client-hints.ts
node tools/generate-bidi-types.mjs --cddl-file digital-credentials.cddl --ts-file src/protocol/generated/webdriver-bidi-digital-credentials.ts --zod-file src/protocol-parser/generated/webdriver-bidi-digital-credentials.ts

# Remove the temporary CDDL files that we copied to the root (as they are gitignored)
rm -f all.cddl permissions.cddl web-bluetooth.cddl nav-speculation.cddl ua-client-hints.cddl digital-credentials.cddl

# 8. Run formatters
echo "Running code formatting..."
./tools/node.py node_modules/prettier/bin/prettier.cjs --cache --write .
./tools/node.py node_modules/eslint/bin/eslint.js --cache --fix .
