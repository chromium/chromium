#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

if [[ $(basename ${PWD}) != "src" ]]; then
  echo "Please set the current working directory to chromium/src first!"
  exit 1
fi
SRC_DIR=${PWD}

MP_VERSION=$(grep -oP 'Version: \K[0-9a-f]+' third_party/mediapipe/README.chromium)

function join_by {
  local d=${1-}
  local f=${2-}
  if shift 2; then
    printf "%s" "$f" "${@/#/$d}"
  fi
}

EXCLUDE_PATTERNS=(
  # Directories
  .git/
  audio/
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

if [[ -d "/tmp/mediapipe" ]]; then
  rm -rf /tmp/mediapipe
fi

echo "Downloading "mediapipe@${MP_VERSION}"..."
mkdir -p /tmp/mediapipe
curl -s -L "https://github.com/google/mediapipe/archive/${MP_VERSION}.tar.gz" | tar xz --strip=1 -C /tmp/mediapipe

cd /tmp/mediapipe
FILES=$(find . -type f | grep -Ev "${EXCLUDE_PATTERN}" | sort)
OBJC_FILES=$(find . -type f | grep -E "${OBJC_INCLUDE_PATTERN}" | sort)
WEB_FILES=$(find . -type f | grep -E "${WEB_INCLUDE_PATTERN}" | sort)
cd "${SRC_DIR}"

rm -rf third_party/mediapipe/src
mkdir -p third_party/mediapipe/src

echo "Replacing existing files..."
cd third_party/mediapipe/src/
for file in ${FILES[@]} ${OBJC_FILES[@]} ${WEB_FILES[@]} ; do
  mkdir -p "$(dirname ${file})"
  cp "/tmp/mediapipe/${file}" "${file}"
done
cd "${SRC_DIR}"

echo "Applying patches..."
for patch_file in $(ls third_party/mediapipe/patches/) ; do
  git apply "third_party/mediapipe/patches/${patch_file}" || echo "Failed to apply third_party/mediapipe/patches/${patch_file}"
done

rm -rf /tmp/mediapipe

echo "Done"
