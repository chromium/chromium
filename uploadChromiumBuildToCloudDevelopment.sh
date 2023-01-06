#!/bin/bash

# Copyright 2021 Record Replay Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Use the current dir as reference point.
DIR=$(pwd)

# Target directory where the build was written
DIST_DIR="${DIR}/out/Release"

# S3 prefix
S3_DIR_PATH="s3://recordreplay-us-east-2-dev"

DO_ARCHIVE=1
DO_UPLOAD_BUILD=1
DO_SYMBOLICATE=1
DO_UPLOAD_SYMBOLS=1
if [ ! -z "$1" ]; then
  DO_ARCHIVE=
  DO_UPLOAD_BUILD=
  DO_SYMBOLICATE=
  DO_UPLOAD_SYMBOLS=

  while [ ! -z "$1" ]; do
    if [ "$1" == "archive" ]; then DO_ARCHIVE=1;
    elif [ "$1" == "upload-build" ]; then DO_UPLOAD_BUILD=1;
    elif [ "$1" == "symbolicate" ]; then DO_SYMBOLICATE=1;
    elif [ "$1" == "upload-symbols" ]; then DO_UPLOAD_SYMBOLS=1;
    else
      echo "Unrecognized argument: $1" >&2
      exit 1
    fi
    shift
  done
fi

# Exit on any error
set -e

echo Uploading chromium build from local dir to cloud-dev
echo Archive=${DO_ARCHIVE} UploadBuild=${DO_UPLOAD_BUILD}
echo Symbolicate=${DO_SYMBOLICATE} UploadSymbols=${DO_UPLOAD_SYMBOLS}
echo

# Always use ReplayDevelopment aws profile, no matter what the
# default is.
export AWS_PROFILE=ReplayDevelopment
echo AWSProfile: ${AWS_PROFILE}

echo CurrentDir: ${DIR}

# Read the build id
BUILD_ID_FILE=base/record_replay_driver.cc
BUILD_ID=$(grep gBuildId "${BUILD_ID_FILE}" | sed 's/^.*"\([^"]*\)".*$/\1/')

S3_BUILD_PREFIX="${S3_DIR_PATH}/builds/${BUILD_ID}"
S3_SYMBOLS_DIR="${S3_DIR_PATH}/symbols"

echo BuildId: ${BUILD_ID}
echo S3TargetPath: ${S3_BUILD_PREFIX}

# Make a temp-dir to store the distribution
TMP_DIR=$(mktemp -d replay-chromium-upload.XXXXXXX)
echo TmpDir: ${TMP_DIR}

if [ -z "${DO_ARCHIVE}" ]; then
  echo "Skipping ARCHIVE"
else
  # Copy the linux build into the temp dir
  echo
  echo RUNNING copyLinuxBuild.js
  mkdir "${TMP_DIR}/replay-chromium"
  time node copyLinuxBuild.js "${DIST_DIR}" "${TMP_DIR}/replay-chromium"

  # Create the archive
  echo
  echo ARCHIVING "${TMP_DIR}/replay-chromium"
  cd "${TMP_DIR}" && time tar cfJ ./archive.tar.xz replay-chromium
fi

# Copy the build to s3
if [ -z "${DO_UPLOAD_BUILD}" ]; then
  echo "Skipping UPLOAD_BUILD"
else
  echo "UPLOADING BUILD to ${S3_BUILD_PREFIX}.xz"
  time aws s3 cp "${TMP_DIR}/archive.tar.xz" "${S3_BUILD_PREFIX}.tar.xz"
fi

# Build the symbols file
SYMBOLS_ARCHIVE_NAME="${BUILD_ID}.symbols.tgz"
SYMBOLS_OUT="${TMP_DIR}/${BUILD_ID}.symbols.json"
if [ -z "${DO_SYMBOLICATE}" ]; then
  echo "Skipping SYMBOLICATE"
else
  echo "SYMBOLICATING ${DIST_DIR}/chrome into ${SYMBOLS_OUT}"
  time nm "${DIST_DIR}/chrome" | grep '^[a-f0-9]\+ [tTw]' >"${TMP_DIR}/symbols"
  echo "{ \"chrome\": {" >"${SYMBOLS_OUT}"
  time cat "${TMP_DIR}/symbols" | \
    perl -pe '/0+([a-f0-9]+) . (.*)$/; $a = hex $1; $b = $2; $_ = "  \"$a\": \"$b\",\n";' \
    >>"${SYMBOLS_OUT}"
    #sed 's/^0\+\([a-f0-9]\+\) . \(.*\)$/  0x\1: "\2",/' >>"${SYMBOLS_OUT}"
  echo "  \"dummy\": \"dummy-entry-without-comma-at-end-of-line\"" >>"${SYMBOLS_OUT}"
  echo "}}" >>"${SYMBOLS_OUT}"

  (cd "${TMP_DIR}" && time tar czf "./${SYMBOLS_ARCHIVE_NAME}" "${BUILD_ID}.symbols.json")
fi

if [ -z "${DO_UPLOAD_SYMBOLS}" ]; then
  echo "Skipping UPLOAD_SYMBOLS"
else
  echo "UPLOADING SYMBOLS to ${S3_SYMBOLS_DIR}/${SYMBOLS_ARCHIVE_NAME}"
  time aws s3 cp "${TMP_DIR}/${SYMBOLS_ARCHIVE_NAME}" "${S3_SYMBOLS_DIR}/${SYMBOLS_ARCHIVE_NAME}"
fi

# No, I'm not going to rm -rf the tmpdir.  That command should never appear
# in shell scripts where the path is a raw variable with no literal path
# prefix that scopes it.
