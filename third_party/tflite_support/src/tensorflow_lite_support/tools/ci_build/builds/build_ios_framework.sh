#!/usr/bin/env bash
# Copyright 2021 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================

# Set the following variables as appropriate.
#   * BAZEL: path to bazel. defaults to the first one available in PATH
#   * TFLS_BUILD_VERSION: to specify the release version. defaults to 0.0.1-dev
#   * IS_RELEASE_BUILD: set as true if this build should be a release build
#   * ARCHIVE_FRAMEWORK: set as true if the framework should be archived
#   * DEST_DIR: destination directory to which the framework will be copied

set -ex

if [[ "$(uname)" != "Darwin" ]]; then
  echo "This build script only works on macOS."
  exit 1
fi

BAZEL="${BAZEL:-$(which bazel)}"
TFLS_BUILD_VERSION=${TFLS_BUILD_VERSION:-0.0.1-dev}
TFLS_ROOT_DIR=$(git rev-parse --show-toplevel)

if [[ ! -x "${BAZEL}" ]]; then
  echo "bazel executable is not found."
  exit 1
fi

if [[ -z "${DEST_DIR}" || "${DEST_DIR}" == ${TFLS_ROOT_DIR}* ]]; then
  echo "DEST_DIR variable must be set and not be under the repository root."
  exit 1
fi

# Builds the C API framework for the specified framework for iOS.
function build_c_api_framework {
  "${BAZEL}" build -c opt --config=ios_fat \
    //tensorflow_lite_support/ios:$1C_framework
}

function create_framework_archive {
  TARGET_FRAMEWORK_NAME="$1"
  C_API_FRAMEWORK_NAME="$1C"

  TFLS_IOS_DIR=tensorflow_lite_support/ios
  BAZEL_IOS_OUTDIR="bazel-bin/${TFLS_IOS_DIR}"

  # Change to the Bazel iOS output directory.
  pushd "${BAZEL_IOS_OUTDIR}"

  # Create the temporary directory for the given framework.
  ARCHIVE_NAME="${TARGET_FRAMEWORK_NAME}-${TFLS_BUILD_VERSION}"
  TFLS_TMPDIR="$(mktemp -d)"

  # Unzip the framework into the appropriate directory structure for CocoaPods.
  # The final archive should contain the C API framework as a vendored framework
  # as well as all the Obj-C source code and the header files.
  #
  # The directory structure will be like:
  #
  # ${TFLS_TMPDIR}/
  #  |-- tensorflow_lite_support/
  #  |   |-- cc/
  #  |   |   +-- <C-API header files...>
  #  |   +-- ios/
  #  |       +-- <Obj-C header/source files...>
  #  +-- Frameworks/
  #      +-- TensorFlowLiteTaskTextC.framework
  #

  # ----- (1) Copy source files -----
  pushd "${TFLS_ROOT_DIR}"

  # Source files to be archived along with the static framework.
  SRC_PATTERNS="
    */cc/*c_api*.h
    */ios/task/text/*/Sources/*
    */ios/task/text/apis/*
  "

  # List of individual files obtained from the patterns above.
  SRC_FILES=$(xargs -n1 find * -wholename <<< "${SRC_PATTERNS}" | sort | uniq)

  # Copy source files with the intermediate directories preserved.
  xargs -n1 -I{} rsync -R {} "${TFLS_TMPDIR}" <<< "${SRC_FILES}"
  popd

  # ----- (2) Unzip the prebuilt C API framework -----
  unzip "${C_API_FRAMEWORK_NAME}_framework.zip" -d "${TFLS_TMPDIR}"/Frameworks

  # ----- (3) Move the framework to the destination -----
  if [[ "${ARCHIVE_FRAMEWORK}" == true ]]; then
    TARGET_DIR="$(realpath "${TARGET_FRAMEWORK_NAME}")"

    # Create the framework archive directory.
    if [[ "${IS_RELEASE_BUILD}" == true ]]; then
      # Get the first 16 bytes of the sha256 checksum of the root directory.
      SHA256_CHECKSUM=$(find "${TFLS_TMPDIR}" -type f -print0 | xargs -0 shasum -a 256 | sort | shasum -a 256 | cut -c1-16)
      FRAMEWORK_ARCHIVE_DIR="${TARGET_DIR}/${TFLS_BUILD_VERSION}/${SHA256_CHECKSUM}"
    else
      FRAMEWORK_ARCHIVE_DIR="${TARGET_DIR}/${TFLS_BUILD_VERSION}"
    fi
    mkdir -p "${FRAMEWORK_ARCHIVE_DIR}"

    # Zip up the framework and move to the archive directory.
    pushd "${TFLS_TMPDIR}"
    TFLS_ARCHIVE_FILE="${ARCHIVE_NAME}.tar.gz"
    tar -cvzf "${TFLS_ARCHIVE_FILE}" .
    mv "${TFLS_ARCHIVE_FILE}" "${FRAMEWORK_ARCHIVE_DIR}"
    popd

    # Move the target directory to the Kokoro artifacts directory.
    mv "${TARGET_DIR}" "$(realpath "${DEST_DIR}")"/
  else
    rsync -r "${TFLS_TMPDIR}/" "$(realpath "${DEST_DIR}")/"
  fi

  # Clean up the temporary directory for the framework.
  rm -rf "${TFLS_TMPDIR}"

  # Pop back to the TFLS root directory.
  popd
}

cd "${TFLS_ROOT_DIR}"
build_c_api_framework TensorFlowLiteTaskText
create_framework_archive TensorFlowLiteTaskText
