#!/usr/bin/env bash
# Copyright 2015 The TensorFlow Authors. All Rights Reserved.
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

set -e

function is_absolute {
  [[ "$1" = /* ]] || [[ "$1" =~ ^[a-zA-Z]:[/\\].* ]]
}

function real_path() {
  is_absolute "$1" && echo "$1" || echo "$PWD/${1#./}"
}

function move_to_root_if_exists () {
  arg_to_move="$1"
  if [ -e "${arg_to_move}" ]; then
    mv ${arg_to_move} ./
  fi
}

function reorganize_includes() {
  TMPDIR="${1%/}"
}

PLATFORM="$(uname -s | tr 'A-Z' 'a-z')"
function is_windows() {
  if [[ "${PLATFORM}" =~ (cygwin|mingw32|mingw64|msys)_nt* ]]; then
    true
  else
    false
  fi
}

function prepare_src() {
  if [ $# -lt 1 ] ; then
    echo "No destination dir provided"
    exit 1
  fi

  TMPDIR="${1%/}"
  mkdir -p "$TMPDIR"
  EXTERNAL_INCLUDES="${TMPDIR}/tflite_support/include/external"

  echo $(date) : "=== Preparing sources in dir: ${TMPDIR}"

  if [ ! -d bazel-bin/tensorflow_lite_support ]; then
    echo "Could not find bazel-bin.  Did you run from the root of the build tree?"
    exit 1
  fi

  if is_windows; then
    rm -rf ./bazel-bin/tensorflow_lite_support/tools/pip_package/simple_console_for_windows_unzip
    mkdir -p ./bazel-bin/tensorflow_lite_support/tools/pip_package/simple_console_for_windows_unzip
    echo "Unzipping simple_console_for_windows.zip to create runfiles tree..."
    unzip -o -q ./bazel-bin/tensorflow_lite_support/tools/pip_package/simple_console_for_windows.zip -d ./bazel-bin/tensorflow_lite_support/tools/pip_package/simple_console_for_windows_unzip
    echo "Unzip finished."
    # runfiles structure after unzip the python binary
    RUNFILES=bazel-bin/tensorflow_lite_support/tools/pip_package/simple_console_for_windows_unzip/runfiles/org_tensorflow_lite_support

    # TODO(b/165872313): Investigate the case and remove the hack.
    # On Windows, __init__.py are not auto genereated at directories that only
    # contains Pybind libraries.
    touch "$RUNFILES/tensorflow_lite_support/metadata/cc/__init__.py"
    touch "$RUNFILES/tensorflow_lite_support/metadata/cc/python/__init__.py"
    touch "$RUNFILES/tensorflow_lite_support/metadata/flatbuffers_lib/__init__.py"
  else
    RUNFILES=bazel-bin/tensorflow_lite_support/tools/pip_package/build_pip_package.runfiles/org_tensorflow_lite_support
  fi

  cp "$RUNFILES/LICENSE" "${TMPDIR}"
  cp -R "$RUNFILES/tensorflow_lite_support" "${TMPDIR}"

  reorganize_includes "${TMPDIR}"

  cp tensorflow_lite_support/tools/pip_package/MANIFEST.in ${TMPDIR}
  cp tensorflow_lite_support/tools/pip_package/README ${TMPDIR}/README.md
  cp tensorflow_lite_support/tools/pip_package/setup.py ${TMPDIR}

  # A helper entry.
  mkdir ${TMPDIR}/tflite_support
  cp tensorflow_lite_support/tools/pip_package/tflite_support.__init__.py ${TMPDIR}/tflite_support/__init__.py
  mkdir ${TMPDIR}/tflite_support/metadata_writers
  cp tensorflow_lite_support/tools/pip_package/metadata_writers.__init__.py ${TMPDIR}/tflite_support/metadata_writers/__init__.py
  if ! is_windows; then
    # Task Library is not supported on Windows yet.
    mkdir ${TMPDIR}/tflite_support/task
    mkdir ${TMPDIR}/tflite_support/task/core
    cp tensorflow_lite_support/tools/pip_package/task.__init__.py ${TMPDIR}/tflite_support/task/__init__.py
    cp tensorflow_lite_support/tools/pip_package/task_core.__init__.py ${TMPDIR}/tflite_support/task/core/__init__.py
    mkdir ${TMPDIR}/tflite_support/task/vision
    cp tensorflow_lite_support/tools/pip_package/task_vision.__init__.py ${TMPDIR}/tflite_support/task/vision/__init__.py
    mkdir ${TMPDIR}/tflite_support/task/text
    cp tensorflow_lite_support/tools/pip_package/task_text.__init__.py ${TMPDIR}/tflite_support/task/text/__init__.py
    mkdir ${TMPDIR}/tflite_support/task/audio
    cp tensorflow_lite_support/tools/pip_package/task_audio.__init__.py ${TMPDIR}/tflite_support/task/audio/__init__.py
    mkdir ${TMPDIR}/tflite_support/task/processor
    cp tensorflow_lite_support/tools/pip_package/task_processor.__init__.py ${TMPDIR}/tflite_support/task/processor/__init__.py
  fi
}

function build_wheel() {
  if [ $# -lt 2 ] ; then
    echo "No src and dest dir provided"
    exit 1
  fi

  TMPDIR="$1"
  DEST="$2"
  PKG_NAME_FLAG="$3"

  # Before we leave the top-level directory, make sure we know how to
  # call python.
  if [[ -e tools/python_bin_path.sh ]]; then
    source tools/python_bin_path.sh
  fi

  pushd ${TMPDIR} > /dev/null

  rm -f MANIFEST
  echo $(date) : "=== Building wheel"
  "${PYTHON_BIN_PATH:-python}" setup.py bdist_wheel ${PKG_NAME_FLAG} >/dev/null
  mkdir -p ${DEST}
  cp dist/* ${DEST}
  popd > /dev/null
  echo $(date) : "=== Output wheel file is in: ${DEST}"
}

function usage() {
  echo "Usage:"
  echo "$0 [--src srcdir] [--dst dstdir] [options]"
  echo "$0 dstdir [options]"
  echo ""
  echo "    --src                 prepare sources in srcdir"
  echo "                              will use temporary dir if not specified"
  echo ""
  echo "    --dst                 build wheel in dstdir"
  echo "                              if dstdir is not set do not build, only prepare sources"
  echo ""
  echo "  Options:"
  echo "    --project_name <name>           set project name to <name>"
  echo "    --version <version>             reset the pip package version to <version>"
  echo "    --nightly_flag                  build TFLite Support nightly"
  echo ""
  echo "When using bazel, add the following flag: --run_under=\"cd \$PWD && \""
  echo ""
  exit 1
}

function main() {
  PKG_NAME_FLAG=""
  PROJECT_NAME=""
  NIGHTLY_BUILD=0
  SRCDIR=""
  DSTDIR=""
  CLEANSRC=1
  VERSION=""
  while true; do
    if [[ "$1" == "--help" ]]; then
      usage
      exit 1
    elif [[ "$1" == "--nightly_flag" ]]; then
      NIGHTLY_BUILD=1
    elif [[ "$1" == "--project_name" ]]; then
      shift
      if [[ -z "$1" ]]; then
        break
      fi
      PROJECT_NAME="$1"
    elif [[ "$1" == "--version" ]]; then
      shift
      if [[ -z "$1" ]]; then
        break
      fi
      VERSION="$1"
    elif [[ "$1" == "--src" ]]; then
      shift
      SRCDIR="$(real_path $1)"
      CLEANSRC=0
    elif [[ "$1" == "--dst" ]]; then
      shift
      DSTDIR="$(real_path $1)"
    else
      echo "Unrecognized flag: $1"
      usage
      exit 1
    fi
    shift

    if [[ -z "$1" ]]; then
      break
    fi
  done

  if [[ -z "$DSTDIR" ]] && [[ -z "$SRCDIR" ]]; then
    echo "No destination dir provided"
    usage
    exit 1
  fi

  if [[ -z "$SRCDIR" ]]; then
    # make temp srcdir if none set
    SRCDIR="$(mktemp -d -t tmp.XXXXXXXXXX)"
  fi

  if [[ -z "$DSTDIR" ]]; then
      # only want to prepare sources
      exit
  fi

  if [[ -n ${PROJECT_NAME} ]]; then
    PKG_NAME_FLAG="--project_name ${PROJECT_NAME}"
  elif [[ ${NIGHTLY_BUILD} == "1" ]]; then
    PKG_NAME_FLAG="--project_name tflite_support_nightly"
  fi

  # Set additional package name flags (for ARM builds).
  if [[ -n ${EXTRA_PKG_NAME_FLAG} ]]; then
      PKG_NAME_FLAG="${PKG_NAME_FLAG} ${EXTRA_PKG_NAME_FLAG}"
  fi

  if [[ ${NIGHTLY_BUILD} == "1" ]]; then
    # we use a script to update versions to avoid any tool differences on different platforms.
    if [[ ! -z ${VERSION} ]]; then
      python tensorflow_lite_support/tools/ci_build/update_version.py --src "." --version ${VERSION} --nightly
    else
      python tensorflow_lite_support/tools/ci_build/update_version.py --src "." --nightly
    fi
  elif [[ ! -z ${VERSION} ]]; then
    python tensorflow_lite_support/tools/ci_build/update_version.py --src "." --version ${VERSION}
  fi

  prepare_src "$SRCDIR"

  build_wheel "$SRCDIR" "$DSTDIR" "$PKG_NAME_FLAG"

  if [[ $CLEANSRC -ne 0 ]]; then
    rm -rf "${TMPDIR}"
  fi
}

main "$@"
