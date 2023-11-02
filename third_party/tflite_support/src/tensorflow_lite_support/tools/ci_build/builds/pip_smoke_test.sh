#!/bin/bash
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
# Pip install TensorFlow Lite Support and run basic test on the pip package.

# Important: Use msys shell to run this script on Windows.

set -e
set -x

function run_smoke_test() {
  VENV_TMP_DIR="$(mktemp -d)"

  echo "Running on $OSTYPE"

  if [[ "$OSTYPE" == "msys" ]]; then
    VENV_TMP_DIR="$(cygpath -m $VENV_TMP_DIR)"
  fi

  python -m virtualenv "${VENV_TMP_DIR}" || \
      die "FAILED: Unable to create virtualenv"

  if [[ "$OSTYPE" == "msys" ]]; then
    source "${VENV_TMP_DIR}/Scripts/activate" || \
        die "FAILED: Unable to activate virtualenv "
  else
    source "${VENV_TMP_DIR}/bin/activate" || \
        die "FAILED: Unable to activate virtualenv "
  fi

  # install tflite-support
  python -m pip install ${WHL_NAME} || \
      die "pip install (forcing to reinstall tflite-support) FAILED"
  echo "Successfully installed pip package ${WHL_NAME}"

  # Download a test model
  export TEST_MODEL="$(pwd)/test.tflite"
  wget https://tfhub.dev/tensorflow/lite-model/mobilenet_v1_0.75_192_quantized/1/metadata/1\?lite-format\=tflite -O "$TEST_MODEL"
  if [[ "$OSTYPE" == "msys" ]]; then
    TEST_MODEL=$(cygpath -m $TEST_MODEL)
  fi

  # Download a test image
  export TEST_IMAGE="$(pwd)/test.jpg"
  if [[ "$OSTYPE" == "msys" ]]; then
    TEST_IMAGE=$(cygpath -m $TEST_IMAGE)
  fi
  wget https://github.com/tensorflow/tflite-support/raw/master/tensorflow_lite_support/cc/test/testdata/task/vision/burger.jpg -O "$TEST_IMAGE"

  test_tfls_imports

  test_codegen

  # On Mac and Ubuntu, verify that the task library builds successfully.
  if [[ "$OSTYPE" != "msys" ]]; then
    test_tfl_task_lib
  fi

  # Deactivate from virtualenv.
  deactivate || source deactivate || \
      die "FAILED: Unable to deactivate from existing virtualenv."

  echo "All smoke test passes!"
}

function test_tfls_imports() {
  TMP_DIR=$(mktemp -d)
  pushd "${TMP_DIR}"

  # test for basic import and metadata display.
  RET_VAL=$(python -c "from tflite_support import metadata; \
md = metadata.MetadataDisplayer.with_model_file(\"$TEST_MODEL\"); \
print(md.get_metadata_json())")

  # just check if the model name is there.
  if ! [[ ${RET_VAL} == *"MobileNetV1 image classifier (quantized)"* ]]; then
    echo "Unexpected return value: ${RET_VAL}"
    echo "PIP smoke test on virtualenv FAILED, do not upload ${WHL_NAME}."
    return 1
  fi

  RESULT=$?

  popd
  return $RESULT
}

function test_codegen() {
  TMP_DIR=$(mktemp -d)
  pushd "${TMP_DIR}"

  # test for basic import and metadata display.
  tflite_codegen --model ${TEST_MODEL} --destination tmp
  RESULT=$?

  # just check if the model name is there.
  if [[ ${RESULT} -ne 0 ]]; then
    echo "Unexpected return value: ${RESULT}"
    echo "PIP smoke test on virtualenv FAILED, do not upload ${WHL_NAME}."
    return 1
  fi

  popd
  return $RESULT
}

function test_tfl_task_lib() {
  TMP_DIR=$(mktemp -d)
  pushd "${TMP_DIR}"

  # test for basic import and metadata display.
  RET_VAL=$(python -c "from tflite_support.task import vision; \
classifier = vision.ImageClassifier.create_from_file(\"$TEST_MODEL\"); \
image = vision.TensorImage.create_from_file(\"$TEST_IMAGE\"); \
image_result = classifier.classify(image); \
print(image_result.classifications[0].categories[0].category_name)")

  # just check if the model name is there.
  if ! [[ ${RET_VAL} == *"cheeseburger"* ]]; then
    echo "Unexpected return value: ${RET_VAL}"
    echo "PIP smoke test on virtualenv FAILED, do not upload ${WHL_NAME}."
    return 1
  fi

  RESULT=$?

  popd
  return $RESULT
}

###########################################################################
# Main
###########################################################################
if [[ -z "${1}" ]]; then
  echo "TFLite Support WHL path not given, unable to install and test."
  return 1
fi

which python
python --version

WHL_NAME=${1}
run_smoke_test
