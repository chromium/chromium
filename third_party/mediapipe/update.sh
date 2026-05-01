#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

if [ ! -d "./third_party/mediapipe" ]; then
  echo "Please set the current working directory to chromium root first."
  exit 1
fi
SRC_DIR=${PWD}

MP_VERSION=$(grep -oP 'Revision: \K[0-9a-f]+' third_party/mediapipe/README.chromium)

function join_by {
  local d=${1-}
  local f=${2-}
  if shift 2; then
    printf "%s" "$f" "${@/#/$d}"
  fi
}

# Fetch the following third party mediapipe dependencies.
THIRD_PARTY_DEPS=(
  "com_google_audio_tools" # https://github.com/google/multichannel-audio-tools
)
# Patches to apply (<directory> <patch file>)
THIRD_PARTY_DEPS_PATCHES=(
  "third_party/com_google_audio_tools third_party/com_google_audio_tools_fixes.diff"
)

EXCLUDE_PATTERNS=(
  # Directories
  .git/
  docs/
  ios/
  java/
  models/
  objc/
  python/
  vision/
  web/

  # Suffixes
  py$

  # Wildcards
  example
  test
)
EXCLUDE_PATTERN=$(join_by '|' "${EXCLUDE_PATTERNS[@]}")

OBJC_INCLUDE_PATTERNS=(
  objc/CFHolder.h
  objc/util.cc
  objc/util.h
)
OBJC_INCLUDE_PATTERN=$(join_by '|' "${OBJC_INCLUDE_PATTERNS[@]}")

WEB_INCLUDE_PATTERNS=(
  web/jspi_check.h
)
WEB_INCLUDE_PATTERN=$(join_by '|' "${WEB_INCLUDE_PATTERNS[@]}")

function download_mediapipe() {
  if [[ -d "/tmp/mediapipe" ]]; then
    rm -rf /tmp/mediapipe
  fi

  echo "Downloading mediapipe@${MP_VERSION}..."
  mkdir -p /tmp/mediapipe
  curl -s -L "https://github.com/google/mediapipe/archive/${MP_VERSION}.tar.gz" | tar xz --strip=1 -C /tmp/mediapipe
}

function list_files() {
  (
    cd /tmp/mediapipe
    find . -type f | grep -Ev "${EXCLUDE_PATTERN}" | sort
    find . -type f | grep -E "${OBJC_INCLUDE_PATTERN}" | sort
    find . -type f | grep -E "${WEB_INCLUDE_PATTERN}" | sort
  )
}

function replace_files() {
  FILES=$(list_files)
  rm -rf third_party/mediapipe/src
  mkdir -p third_party/mediapipe/src
  echo "Replacing existing files..."
  (
    cd third_party/mediapipe/src/
    for file in ${FILES[@]}; do
      mkdir -p "$(dirname ${file})"
      cp "/tmp/mediapipe/${file}" "${file}"
    done
  )
}

function download_deps() {
  echo "Downloading mediapipe third party dependencies..."
  (
    cd third_party/mediapipe/src/
    for dep in "${THIRD_PARTY_DEPS[@]}" ; do
      echo "Downloading ${dep}..."
      ARCHIVE=$(grep -Pzo "(?s)name = \"${dep}\".*?\)" WORKSPACE | grep -Eao 'https://github.com[^"]*' | sed "s/zip/tar\.gz/")
      if [ -z "${ARCHIVE}" ]; then
        echo "Failed to find ${dep} archive in WORKSPACE"
        exit 1
      fi
      echo "Downloading ${ARCHIVE}..."
      mkdir -p ./third_party/${dep}
      curl -s -L "${ARCHIVE}" | tar xz --strip=1 -C ./third_party/${dep}
    done
  )
}

function apply_deps_patches() {
  echo "Applying patches to third party dependencies..."
  (
    cd third_party/mediapipe/src/
    for patch_file in "${THIRD_PARTY_DEPS_PATCHES[@]}" ; do
      set -- $patch_file
      echo $1
      git apply --directory=$1 $2 || echo "Failed to apply $2"
    done
  )
}

function replace_checks() {
  echo "Replacing (D)CHECK with ABSL equivalents..."
  (
    cd third_party/mediapipe/src/
    for dep in "${THIRD_PARTY_DEPS[@]}" ; do
      find ./third_party/${dep} -type f -exec sed -Ei 's/(DCHECK|CHECK)/ABSL_\0/g' {} \;
    done
  )
}

function apply_mediapipe_patches() {
  echo "Applying patches..."
  for patch_file in $(ls third_party/mediapipe/patches/) ; do
    git apply "third_party/mediapipe/patches/${patch_file}" || echo "Failed to apply third_party/mediapipe/patches/${patch_file}"
  done
}

function cleanup() {
  echo "Cleaning up..."
  rm -rf /tmp/mediapipe
}

function all() {
  download_mediapipe
  replace_files
  download_deps
  apply_deps_patches
  replace_checks
  apply_mediapipe_patches
  cleanup
}

if [ $# -eq 0 ]; then
  all
else
  $1
fi

echo "Done"
