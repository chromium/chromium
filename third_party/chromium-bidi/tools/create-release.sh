#!/bin/bash

# @license
# Copyright 2026 Google Inc.
# SPDX-License-Identifier: Apache-2.0

set -e

# Change directory to the git repository root so the script works from any directory
cd "$(git rev-parse --show-toplevel)"

# Parse arguments for dry-run option
DRY_RUN=false
for arg in "$@"; do
  if [ "$arg" = "--dry-run" ]; then
    DRY_RUN=true
  fi
done

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

# 2. Find the latest version from .release-please-manifest.json
if [ ! -f .release-please-manifest.json ]; then
  echo "Error: .release-please-manifest.json not found." >&2
  exit 1
fi

VERSION=$(node -p "require('./.release-please-manifest.json')['.']" 2>/dev/null)
if [ -z "$VERSION" ]; then
  echo "Error: Failed to extract version from .release-please-manifest.json." >&2
  exit 1
fi

TAG_NAME="chromium-bidi-v$VERSION"
RELEASE_TITLE="chromium-bidi: v$VERSION"

echo "Latest release-please version: $VERSION"
echo "Target Tag: $TAG_NAME"
echo "Target Release Title: $RELEASE_TITLE"

if [ "$DRY_RUN" = true ]; then
  echo "--- RUNNING IN DRY RUN MODE ---"
fi

# 3. Check if the release already exists on GitHub
if gh release view "$TAG_NAME" &>/dev/null; then
  echo "GitHub release '$TAG_NAME' already exists. Nothing to do."
  exit 0
fi

echo "GitHub release '$TAG_NAME' does not exist. Proceeding..."

# 4. Extract the changelog for this version from CHANGELOG.md
export VERSION
CHANGELOG=$(node << 'EOF' 2>/dev/null
const fs = require('fs');
if (!fs.existsSync('CHANGELOG.md')) {
  process.exit(1);
}
const content = fs.readFileSync('CHANGELOG.md', 'utf8');
const version = process.env.VERSION;
const escapedVersion = version.replace(/\./g, '\\.');
const regex = new RegExp(`## \\[\\[?${escapedVersion}\\]?(?:\\([^)]+\\))?\\s*\\([^)]+\\)[\\s\\S]*?(?=\\n## |$)`);
const match = content.match(regex);
if (match) {
  console.log(match[0].split('\n').slice(1).join('\n').trim());
} else {
  process.exit(1);
}
EOF
)

if [ -z "$CHANGELOG" ]; then
  echo "Warning: Changelog for version $VERSION not found in CHANGELOG.md." >&2
  CHANGELOG="Changelog details for version $VERSION."
fi

# 5. Tagging and releasing
if [ "$DRY_RUN" = true ]; then
  echo "[DRY RUN] Would check if git tag '$TAG_NAME' exists locally..."
  if git rev-parse "$TAG_NAME" &>/dev/null; then
    echo "[DRY RUN] Git tag '$TAG_NAME' already exists locally."
  else
    echo "[DRY RUN] Would run: git tag \"$TAG_NAME\""
    echo "[DRY RUN] Would run: git push origin \"$TAG_NAME\""
  fi

  echo "[DRY RUN] Would run: gh release create \"$TAG_NAME\" --title \"$RELEASE_TITLE\" --notes-file <temp_notes_file>"
  echo "[DRY RUN] Extracted changelog notes:"
  echo "--------------------------------------------"
  printf "%s\n" "$CHANGELOG"
  echo "--------------------------------------------"
  echo "[DRY RUN] Dry run complete!"
else
  # Check and create local git tag
  if git rev-parse "$TAG_NAME" &>/dev/null; then
    echo "Git tag '$TAG_NAME' already exists locally."
  else
    echo "Creating git tag '$TAG_NAME'..."
    git tag "$TAG_NAME"

    echo "Pushing tag '$TAG_NAME' to origin..."
    if ! git push origin "$TAG_NAME"; then
      echo "Warning: Failed to push tag to origin. It may already exist on remote." >&2
    fi
  fi

  # Create a temporary file for notes to handle multi-line input and any double quotes perfectly
  NOTES_FILE=$(mktemp)
  trap 'rm -f "$NOTES_FILE"' EXIT
  printf "%s\n" "$CHANGELOG" > "$NOTES_FILE"

  # Create GitHub release
  echo "Creating GitHub release..."
  gh release create "$TAG_NAME" \
    --title "$RELEASE_TITLE" \
    --notes-file "$NOTES_FILE"

  echo "Successfully created GitHub release and tag for version $VERSION!"
fi
