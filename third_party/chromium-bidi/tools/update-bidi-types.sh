#!/bin/bash

# @license
# Copyright 2026 Google Inc.
# SPDX-License-Identifier: Apache-2.0

set -e

# Change directory to the git repository root so the script works from any directory
cd "$(git rev-parse --show-toplevel)"

# Default parameters (matching workflow inputs)
SPEC_REPO="w3c/webdriver-bidi"
SPEC_REF="main"

# Check requirements (always required as we always create PR)
if ! command -v gh &> /dev/null; then
  echo "Error: 'gh' CLI is required but not installed." >&2
  echo "Please install it first (e.g. 'brew install gh')." >&2
  exit 1
fi

if ! gh auth status &> /dev/null; then
  echo "Error: You are not authenticated with 'gh' CLI." >&2
  echo "Please run 'gh auth login' first." >&2
  exit 1
fi

# Check cargo / cddlconv dependency
if ! command -v cddlconv &> /dev/null; then
  echo "Error: 'cddlconv' is required but not installed." >&2
  echo "Please install it using cargo: 'cargo install cddlconv@0.1.7'" >&2
  exit 1
fi

# Check pre-commit for formatting
if ! command -v pre-commit &> /dev/null; then
  echo "Warning: 'pre-commit' is not installed. Code formatting might fail or be skipped." >&2
fi

CURRENT_BRANCH=$(git branch --show-current)
if [ -z "$CURRENT_BRANCH" ]; then
  echo "Error: Could not detect current git branch." >&2
  exit 1
fi

# Make sure there are no uncommitted changes
if [ -n "$(git status --porcelain)" ]; then
  echo "Error: You have uncommitted changes on branch '$CURRENT_BRANCH'." >&2
  echo "Please commit, stash, or discard them before running this script." >&2
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
  echo "Installing dependencies and generating main spec CDDL..."
  npm install parse5
  ./scripts/test.sh
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

# 6. Install chromium-bidi dependencies
echo "Installing npm dependencies for chromium-bidi..."
npm ci

# 7. Generate TypeScript and Zod types from CDDL files
echo "Generating types..."
node tools/generate-bidi-types.mjs --cddl-file all.cddl
node tools/generate-bidi-types.mjs --cddl-file permissions.cddl --ts-file src/protocol/generated/webdriver-bidi-permissions.ts --zod-file src/protocol-parser/generated/webdriver-bidi-permissions.ts
node tools/generate-bidi-types.mjs --cddl-file web-bluetooth.cddl --ts-file src/protocol/generated/webdriver-bidi-bluetooth.ts --zod-file src/protocol-parser/generated/webdriver-bidi-bluetooth.ts
node tools/generate-bidi-types.mjs --cddl-file nav-speculation.cddl --ts-file src/protocol/generated/webdriver-bidi-nav-speculation.ts --zod-file src/protocol-parser/generated/webdriver-bidi-nav-speculation.ts
node tools/generate-bidi-types.mjs --cddl-file ua-client-hints.cddl --ts-file src/protocol/generated/webdriver-bidi-ua-client-hints.ts --zod-file src/protocol-parser/generated/webdriver-bidi-ua-client-hints.ts

# Remove the temporary CDDL files that we copied to the root (as they are gitignored)
rm -f all.cddl permissions.cddl web-bluetooth.cddl nav-speculation.cddl ua-client-hints.cddl

# 8. Run formatters
echo "Running code formatting..."
npm run format || npm run format

# 9. Check if changes were made and commit
if [ -n "$(git status --porcelain src/protocol/generated/ src/protocol-parser/generated/)" ]; then
  COMMIT_MSG="build(spec): update WebDriverBiDi types"
  NEW_BRANCH="browser-automation-bot/update-bidi-types-$(date +%s)"

  echo "Changes detected. Creating new branch '$NEW_BRANCH' based off '$CURRENT_BRANCH'..."
  git checkout -b "$NEW_BRANCH"

  echo "Committing changes: '$COMMIT_MSG'..."
  git add src/protocol/generated/ src/protocol-parser/generated/
  git commit -m "$COMMIT_MSG"

  echo "Pushing branch '$NEW_BRANCH' to origin..."
  git push origin "$NEW_BRANCH" --force

  # Determine the base branch for the PR.
  # Default to the repository's default branch (usually main).
  # If the current branch exists on the remote target repository, we target that instead.
  DEFAULT_BRANCH=$(gh repo view --json defaultBranchRef --template '{{.defaultBranchRef.name}}')
  BASE_BRANCH="$DEFAULT_BRANCH"
  BASE_REPO=$(gh repo view --json nameWithOwner --template '{{.nameWithOwner}}')
  if git ls-remote --exit-code --heads "https://github.com/$BASE_REPO.git" "$CURRENT_BRANCH" &>/dev/null; then
    BASE_BRANCH="$CURRENT_BRANCH"
  fi

  PR_BODY="Automatically generated by tools/update-bidi-types.sh"
  echo "Creating Pull Request on GitHub into branch '$BASE_BRANCH'..."
  gh pr create \
    --title "$COMMIT_MSG" \
    --body "$PR_BODY" \
    --head "$NEW_BRANCH" \
    --base "$BASE_BRANCH"

  echo "Restoring original branch '$CURRENT_BRANCH'..."
  git checkout "$CURRENT_BRANCH"
else
  echo "No updates required for WebDriverBiDi types."
fi
