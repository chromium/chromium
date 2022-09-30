#!/bin/bash

# Exit on any error
set -e

echo Uploading chromium build from local dir to cloud-dev
echo

# Always use ReplayDevelopment aws profile, no matter what the
# default is.
export AWS_PROFILE=ReplayDevelopment
echo AWSProfile: ${AWS_PROFILE}

# Use the current dir as reference point.
DIR=$(pwd)
TARGET_DIR="${DIR}/out/Release"
S3_DIR_PATH="s3://recordreplay-us-east-2-dev/builds"

echo CurrentDir: ${DIR}

# Read the build id
BUILD_ID_FILE=base/record_replay_driver.cc
BUILD_ID=$(grep gBuildId "${BUILD_ID_FILE}" | sed 's/^.*"\([^"]*\)".*$/\1/')

S3_BUILD_PREFIX="${S3_DIR_PATH}/${BUILD_ID}"

echo BuildId: ${BUILD_ID}
echo S3TargetPath: ${S3_BUILD_PREFIX}

# Make a temp-dir to store the distribution
TMP_DIR=$(mktemp -d)
echo TmpDir: ${TMP_DIR}

echo
echo RUNNING copyLinuxBuild.js
mkdir "${TMP_DIR}/replay-chromium"
node copyLinuxBuild.js "${TARGET_DIR}" "${TMP_DIR}/replay-chromium"

echo
echo ARCHIVING "${TMP_DIR}/replay-chromium"
(cd "${TMP_DIR}" && tar cfJ ./archive.tar.xz replay-chromium)

echo COPYING to "${S3_BUILD_PREFIX}.xz"
aws s3 cp "${TMP_DIR}/archive.tar.xz" "${S3_BUILD_PREFIX}.xz"

# No, I'm not going to rm -rf the tmpdir.  That command should never appear
# in shell scripts where the path is a raw variable with no literal path
# prefix that scopes it.
