/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow_lite_support/cc/task/text/clu_lib/tflite_test_utils.h"

#include <initializer_list>

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/string_util.h"

namespace tflite::task::text::clu {

template <>
void PopulateTfLiteTensorValue<std::string>(
    const std::initializer_list<std::string> values, TfLiteTensor* tensor) {
  tflite::DynamicBuffer buf;
  for (const std::string& s : values) {
    buf.AddString(s.data(), s.length());
  }
  buf.WriteToTensor(tensor, /*new_shape=*/nullptr);
}

size_t NumTotalFromShape(const std::initializer_list<int>& shape) {
  size_t num_total;
  if (shape.size() > 0)
    num_total = 1;
  else
    num_total = 0;
  for (const int dim : shape) num_total *= dim;
  return num_total;
}

TfLiteTensor* UniqueTfLiteTensor::get() { return tensor_; }

UniqueTfLiteTensor::~UniqueTfLiteTensor() { TfLiteTensorFree(tensor_); }

template <>
TfLiteType TypeToTfLiteType<std::string>() {
  return kTfLiteString;
}

}  // namespace tflite::task::text::clu
