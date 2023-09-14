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

bash tensorflow_lite_support/custom_ops/tf_configure.sh

# Compile the two schema srcjars first. Compiling
# tensorflow-lite-support-metadata-lib directly will lead to a racing issue
# similar to b/200756963 that two sets of TFLite schema source files are
# generated, and will thus cause the duplicated Java class error.
bazel build -c opt --config=monolithic tensorflow_lite_support/metadata:schema_fbs_java_srcjar
bazel build -c opt --config=monolithic tensorflow_lite_support/metadata:metadata_schema_java_srcjar

# Break down metadata builds and avoid the flacky issue when building schema
# with Bazel. See b/200756963.
bazel build -c opt --config=monolithic \
    //tensorflow_lite_support/metadata/java:tensorflow-lite-support-metadata-lib
bazel build -c opt --config=monolithic \
    //tensorflow_lite_support/metadata/cc:metadata_extractor

export BAZEL_PARALLEL="-j 32"

# General targets.
bazel build -c opt ${BAZEL_PARALLEL} --config=monolithic \
    //tensorflow_lite_support/codegen/python:codegen \
    //tensorflow_lite_support/custom_ops/kernel:all \
    //tensorflow_lite_support/custom_ops/python:tflite_text_api \
    //tensorflow_lite_support/examples/task/audio/desktop:audio_classifier_demo

# Android targets.
bazel build -c opt ${BAZEL_PARALLEL} --config=monolithic \
    --config=android_arm64 --fat_apk_cpu=x86,x86_64,arm64-v8a,armeabi-v7a \
    --define=xnn_enable_arm_fp16=false \
    --define=android_dexmerger_tool=d8_dexmerger \
    --define=android_incremental_dexing_tool=d8_dexbuilder\
    //tensorflow_lite_support/java:tensorflowlite_support \
    //tensorflow_lite_support/cc/task/vision:image_embedder \
    //tensorflow_lite_support/cc/task/vision:image_searcher \
    //tensorflow_lite_support/cc/task/audio:audio_embedder \
    //tensorflow_lite_support/cc/task/processor:all \
    //tensorflow_lite_support/cc/task/text:text_searcher \
    //tensorflow_lite_support/odml/java/image \
    //tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/core:base-task-api.aar \
    //tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/text:task-library-text \
    //tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/vision:task-library-vision \
    //tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/audio:task-library-audio \
    //tensorflow_lite_support/acceleration/configuration:gpu-delegate-plugin

# Pip package
bazel build -c opt ${BAZEL_PARALLEL} \
      --define darwinn_portable=1 \
      tensorflow_lite_support/tools/pip_package:build_pip_package

# Tests.

bazel clean

bazel test -c opt $BAZEL_PARALLEL --test_output=all \
    //tensorflow_lite_support/c/test/... \
    //tensorflow_lite_support/cc/test/task/vision:all \
    //tensorflow_lite_support/cc/test/task/text/... \
    //tensorflow_lite_support/custom_ops/kernel/sentencepiece:all \
    //tensorflow_lite_support/metadata/python/tests:metadata_test \
    //tensorflow_lite_support/metadata/python/tests/metadata_writers:all \
    //tensorflow_lite_support/python/test/task/... \
    //tensorflow_lite_support/scann_ondevice/cc/...

bazel test -c opt $BAZEL_PARALLEL --test_output=all --build_tests_only \
    --build_tag_filters=-tflite_emulator_test_android \
    --test_tag_filters=-tflite_emulator_test_android \
    //tensorflow_lite_support/java/src/javatests/org/tensorflow/lite/support/...

