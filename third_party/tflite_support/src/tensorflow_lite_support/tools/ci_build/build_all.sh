#!/usr/bin/env bash
# Copyright 2020 The TensorFlow Authors. All Rights Reserved.
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
# External `build_all.sh`

set -ex

bazel build -c opt --config=monolithic \
    //tensorflow_lite_support/java:tensorflowlite_support \
    //tensorflow_lite_support/codegen/python:codegen \
    //tensorflow_lite_support/metadata/java:tensorflowlite_support_metadata_lib \
    //tensorflow_lite_support/metadata/cc:metadata_extractor \
    //tensorflow_lite_support/custom_ops/kernel:all \
    //tensorflow_lite_support/custom_ops/python:tflite_text_api

# Build Task libraries.
bazel build -c opt --config=monolithic \
    --config=android_arm64 --fat_apk_cpu=x86,x86_64,arm64-v8a,armeabi-v7a \
    //tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/core:base-task-api.aar \
    //tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/text:task-library-text \
    //tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/vision:task-library-vision
