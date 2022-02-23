/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// This program runs the tflite model specified in --model with random inputs.
// For string type, the input is filled with a fixed string.

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/model_builder.h"
#include "tensorflow/lite/string_util.h"

void FillRandomString(tflite::DynamicBuffer* buffer,
                      const TfLiteIntArray* dim_array,
                      const std::function<std::string()>& random_func) {
  int num_elements = 1;
  for (size_t i = 0; i < dim_array->size; i++) {
    num_elements *= dim_array->data[i];
  }
  for (int i = 0; i < num_elements; ++i) {
    auto str = random_func();
    buffer->AddString(str.data(), str.length());
  }
}

bool RunWithRandomInputs(const std::string& filename) {
  std::unique_ptr<tflite::FlatBufferModel> model =
      tflite::FlatBufferModel::BuildFromFile(filename.c_str());

  // Build the interpreter
  tflite::ops::builtin::BuiltinOpResolver resolver;
  std::unique_ptr<tflite::Interpreter> interpreter;
  if (tflite::InterpreterBuilder(*model, resolver)(&interpreter) != kTfLiteOk) {
    LOG(ERROR) << "Could not initialize interpreter for TFLite model.";
    return false;
  }

  // Resize input tensors, if desired.
  if (interpreter->AllocateTensors() != kTfLiteOk) {
    LOG(ERROR) << "Could not allocate tensor.";
    return false;
  }

  // Fill the random data.
  std::vector<std::vector<uint8_t>> sample;
  for (int tensor_idx : interpreter->inputs()) {
    auto tensor = interpreter->tensor(tensor_idx);
    if (tensor->type == kTfLiteString) {
      tflite::DynamicBuffer buffer;
      FillRandomString(&buffer, tensor->dims, []() {
        return "we're have some friends over saturday to hang out in the "
               "yard";
      });
      buffer.WriteToTensor(tensor, /*new_shape=*/nullptr);
    } else {
      std::vector<uint8_t> data(tensor->bytes);
      for (auto it = data.begin(); it != data.end(); ++it) {
        *it = random();
      }
      sample.push_back(data);
      tensor->data.raw = reinterpret_cast<char*>(sample.rbegin()->data());
    }
  }

  // Running inference.
  if (interpreter->Invoke() != kTfLiteOk) {
    LOG(ERROR) << "Failed to run the model.";
    return false;
  }

  // Get the output.
  for (int tensor_idx : interpreter->outputs()) {
    auto tensor = interpreter->tensor(tensor_idx);
    LOG(INFO) << "Output type: " << TfLiteTypeGetName(tensor->type);
  }
  return true;
}

TEST(SelectiveBuiltTest, SentencePieceTokenizerModel) {
  std::string model =
      "tensorflow_lite_support/custom_ops/testdata/"
      "sentencepiece_tokenizer_flex_op.tflite";
  EXPECT_THAT(RunWithRandomInputs(model), true);
}

TEST(SelectiveBuiltTest, Wiki40bLmEnModel) {
  std::string model =
      "tensorflow_lite_support/custom_ops/testdata/"
      "wiki40b-lm-en.tflite";
  EXPECT_THAT(RunWithRandomInputs(model), true);
}
