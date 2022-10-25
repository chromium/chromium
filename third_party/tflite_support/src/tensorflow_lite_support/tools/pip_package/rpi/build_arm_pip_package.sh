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

set -ex

NIGHTLY_FLAG=$1

bazel build -c opt --config=elinux_armhf --define=tflite_with_ruy=true \
    --copt=-march=armv7-a --copt=-mfpu=neon-vfpv4 --copt=-fno-tree-pre \
    --cxxopt=-fpermissive --define tensorflow_mkldnn_contraction_kernel=0 \
    --define=raspberry_pi_with_neon=true --define=tflite_with_xnnpack=false \
    --define darwinn_portable=1 --linkopt=-L/usr/lib/arm-linux-gnueabihf \
    tensorflow_lite_support/tools/pip_package:build_pip_package
EXTRA_PKG_NAME_FLAG="--plat-name=manylinux2014-armv7l" ./bazel-bin/tensorflow_lite_support/tools/pip_package/build_pip_package --dst wheels ${NIGHTLY_FLAG}

bazel build -c opt --define=tflite_with_ruy=true --config=elinux_aarch64 \
    --define tensorflow_mkldnn_contraction_kernel=0 \
    --define darwinn_portable=1 \
    --linkopt=-L/usr/lib/aarch64-linux-gnu \
    tensorflow_lite_support/tools/pip_package:build_pip_package
EXTRA_PKG_NAME_FLAG="--plat-name=manylinux2014-aarch64" ./bazel-bin/tensorflow_lite_support/tools/pip_package/build_pip_package --dst wheels ${NIGHTLY_FLAG}
