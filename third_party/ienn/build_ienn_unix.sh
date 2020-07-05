#!/usr/bin/env bash

# Copyright (C) 2018-2020 Intel Corporation
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

error() {
    local code="${3:-1}"
    if [[ -n "$2" ]];then
        echo "Error on or near line $1: $2; exiting with status ${code}"
    else
        echo "Error on or near line $1; exiting with status ${code}"
    fi
    exit "${code}"
}
trap 'error ${LINENO}' ERR

INTEL_OPENVINO_DIR="/opt/intel/openvino"
CURRENT_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

printf "\n$CURRENT_PATH\n"
printf "\nSetting environment variables for building samples...\n"

if [ -z "$INTEL_OPENVINO_DIR" ]; then
    if [ -e "$CURRENT_PATH/../../../bin/setupvars.sh" ]; then
        setvars_path="$CURRENT_PATH/../../../bin/setupvars.sh"
    elif [ -e "$CURRENT_PATH/../../../../bin/setupvars.sh" ]; then
        setvars_path="$CURRENT_PATH/../../../../bin/setupvars.sh"
    else
        printf "Error: Failed to set the environment variables automatically. To fix, run the following command:\n source <INSTALL_DIR>/bin/setupvars.sh\n where INSTALL_DIR is the OpenVINO installation directory.\n\n"
        exit 1
    fi
    if ! source "$setvars_path" ; then
        printf "Unable to run ./setupvars.sh. Please check its presence. \n\n"
        exit 1
    fi
else
    # case for run with `sudo -E` 
    source "$INTEL_OPENVINO_DIR/bin/setupvars.sh"
fi

if ! command -v cmake &>/dev/null; then
    printf "\n\nCMAKE is not installed. It is required to build Inference Engine samples. Please install it. \n\n"
    exit 1
fi

build_dir="$CURRENT_PATH/build"

OS_PATH=$(uname -m)
NUM_THREADS="-j2"

if [ "$OS_PATH" == "x86_64" ]; then
  OS_PATH="intel64"
  NUM_THREADS="-j8"
fi

if [ -e "$build_dir/CMakeCache.txt" ]; then
    rm -rf "$build_dir/CMakeCache.txt"
fi
mkdir -p "$build_dir"
cd "$build_dir"
cmake -DCMAKE_BUILD_TYPE=Release "$CURRENT_PATH"
make $NUM_THREADS

printf "\nBuild completed, you can find binaries for all samples in the $build_dir/%s/Release subfolder.\n\n" "$OS_PATH"
