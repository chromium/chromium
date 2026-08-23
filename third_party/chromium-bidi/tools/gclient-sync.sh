#!/bin/bash
set -euo pipefail

# Resolve directories
# We are currently in the repository checkout directory.
# The parent directory is where .gclient and depot_tools will reside.
CWD=$(pwd)
DIR_NAME=$(basename "$CWD")
PARENT_DIR=$(dirname "$CWD")

echo "Setting up gclient in the parent directory: $PARENT_DIR"

# Clone depot_tools if it doesn't exist
DEPOT_TOOLS_DIR="$PARENT_DIR/depot_tools"
if [ ! -d "$DEPOT_TOOLS_DIR" ]; then
  echo "Cloning depot_tools..."
  git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "$DEPOT_TOOLS_DIR"
else
  echo "depot_tools already exists."
fi

# Add depot_tools to PATH for the current execution
export PATH="$DEPOT_TOOLS_DIR:$PATH"

# If running in GitHub Actions, also append depot_tools to GITHUB_PATH
if [ -n "${GITHUB_PATH:-}" ]; then
  echo "Adding depot_tools to GITHUB_PATH..."
  echo "$DEPOT_TOOLS_DIR" >> "$GITHUB_PATH"
fi

# Create .gclient in the parent directory using gclient config
echo "Creating .gclient in $PARENT_DIR..."
(cd "$PARENT_DIR" && gclient config --spec 'solutions = [
  {
    "name": "'"${DIR_NAME}"'",
    "url": None,
    "deps_file": "DEPS",
    "managed": False,
  },
]')

# Run gclient sync
echo "Running gclient sync..."
gclient sync -D
