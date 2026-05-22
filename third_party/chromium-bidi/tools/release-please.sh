#!/bin/bash

# @license
# Copyright 2026 Google Inc.
# SPDX-License-Identifier: Apache-2.0

set -e

# Change directory to the git repository root so the script works from any directory
cd "$(git rev-parse --show-toplevel)"

# 1. Check requirements
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

GITHUB_TOKEN=$(gh auth token)
if [ -z "$GITHUB_TOKEN" ]; then
  echo "Error: Failed to retrieve authentication token from 'gh auth token'." >&2
  exit 1
fi

# 2. Determine repository
REPO_URL="GoogleChromeLabs/chromium-bidi"

echo "Running release-please for $REPO_URL..."

# 3. Run manifest-pr
echo "Checking and updating release pull request..."
npx release-please manifest-pr \
  --token="$GITHUB_TOKEN" \
  --repo-url="$REPO_URL" \
  --target-branch="main" \
  --config-file="release-please-config.json" \
  --manifest-file=".release-please-manifest.json" \
  --fork \
  "$@"

# 4. Run manifest-release
echo "Checking and creating github release..."
npx release-please manifest-release \
  --token="$GITHUB_TOKEN" \
  --repo-url="$REPO_URL" \
  --target-branch="main" \
  --config-file="release-please-config.json" \
  --manifest-file=".release-please-manifest.json" \
  "$@"

echo "Release-please run complete!"
