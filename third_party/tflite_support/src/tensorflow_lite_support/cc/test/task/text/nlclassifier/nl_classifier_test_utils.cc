/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow_lite_support/cc/test/task/text/nlclassifier/nl_classifier_test_utils.h"

#include "tensorflow/lite/kernels/builtin_op_kernels.h"
#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/string_util.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"

namespace tflite {
namespace ops {
namespace custom {

constexpr char kInputStr[] = "hello";

namespace string_floats {
TfLiteStatus Prepare(TfLiteContext* context, TfLiteNode* node) {
  TfLiteTensor* output = GetOutput(context, node, 0);
  TF_LITE_ENSURE(context, output != nullptr);
  TfLiteIntArray* dims = TfLiteIntArrayCreate(1);
  dims->data[0] = 3;  // Size of 'data' in Invoke()
  return context->ResizeTensor(context, output, dims);
}

TfLiteStatus Invoke(TfLiteContext* context, TfLiteNode* node) {
  const TfLiteTensor* input_tensor = GetInput(context, node, 0);
  TF_LITE_ENSURE(context, input_tensor != nullptr);
  StringRef input_str_ref = GetString(input_tensor, 0);
  std::string input_str(input_str_ref.str, input_str_ref.len);
  if (input_str != kInputStr) {
    return kTfLiteError;
  }
  std::vector<float> data = {255, 510, 765};
  TfLiteTensor* output = GetOutput(context, node, 0);
  TF_LITE_ENSURE(context, output != nullptr);
  // Default Quantize OP scale is 255, will be quantized to {1, 2, 3}
  TF_LITE_ENSURE(context,
                 tflite::task::core::PopulateTensor(data, output).ok());
  return kTfLiteOk;
}

// This custom op takes a string tensor in and outputs a float32 tensor with
// value{0.1, 0.2, 0.3}, it's used to mimic a real text classification model
// which classifies a string into scores of different categories.
TfLiteRegistration* Register() {
  // Dummy implementation of custom OP
  // This op takes string as input and outputs float[]
  static TfLiteRegistration r = {/* init= */ nullptr, /* free= */ nullptr,
                                 /* prepare= */ Prepare, /* invoke= */ Invoke};
  return &r;
}
};  // namespace string_floats

namespace string_doubles {
TfLiteStatus Prepare(TfLiteContext* context, TfLiteNode* node) {
  TfLiteTensor* output = GetOutput(context, node, 0);
  TF_LITE_ENSURE(context, output != nullptr);
  TfLiteIntArray* dims = TfLiteIntArrayCreate(1);
  dims->data[0] = 3;  // Size of 'data' in Invoke()
  return context->ResizeTensor(context, output, dims);
}

TfLiteStatus Invoke(TfLiteContext* context, TfLiteNode* node) {
  const TfLiteTensor* input_tensor = GetInput(context, node, 0);
  TF_LITE_ENSURE(context, input_tensor != nullptr);
  StringRef input_str_ref = GetString(input_tensor, 0);
  std::string input_str(input_str_ref.str, input_str_ref.len);
  if (input_str != kInputStr) {
    return kTfLiteError;
  }
  std::vector<double> data = {255, 510, 765};
  TfLiteTensor* output = GetOutput(context, node, 0);
  TF_LITE_ENSURE(context, output != nullptr);
  // Default Quantize OP scale is 255, will be quantized to {1, 2, 3}
  TF_LITE_ENSURE(context,
                 tflite::task::core::PopulateTensor(data, output).ok());
  return kTfLiteOk;
}

// This custom op takes a string tensor in and outputs a float64 tensor with
// value{0.1, 0.2, 0.3}, it's used to mimic a real text classification model
// which classifies a string into scores of different categories.
TfLiteRegistration* Register() {
  // Dummy implementation of custom OP
  // This op takes string as input and outputs double[]
  static TfLiteRegistration r = {/* init= */ nullptr, /* free= */ nullptr,
                                 /* prepare= */ Prepare, /* invoke= */ Invoke};
  return &r;
}
};  // namespace string_doubles

namespace string_bools {
TfLiteStatus Prepare(TfLiteContext* context, TfLiteNode* node) {
  TfLiteTensor* output = GetOutput(context, node, 0);
  TF_LITE_ENSURE(context, output != nullptr);
  TfLiteIntArray* dims = TfLiteIntArrayCreate(1);
  dims->data[0] = 3;  // Size of 'data' in Invoke()
  return context->ResizeTensor(context, output, dims);
}

TfLiteStatus Invoke(TfLiteContext* context, TfLiteNode* node) {
  const TfLiteTensor* input_tensor = GetInput(context, node, 0);
  TF_LITE_ENSURE(context, input_tensor != nullptr);
  StringRef input_str_ref = GetString(input_tensor, 0);
  std::string input_str(input_str_ref.str, input_str_ref.len);
  if (input_str != kInputStr) {
    return kTfLiteError;
  }
  bool data[] = {true, true, false};
  TfLiteTensor* output = GetOutput(context, node, 0);
  TF_LITE_ENSURE(context, output != nullptr);
  TF_LITE_ENSURE(context,
                 tflite::task::core::PopulateTensor(data, 3, output).ok());
  return kTfLiteOk;
}

// This custom op takes a string tensor in and outputs a bool tensor with
// value{true, true, false}, it's used to mimic a real text classification model
// which classifies a string into scores of different categories.
TfLiteRegistration* Register() {
  // Dummy implementation of custom OP
  // This op takes string as input and outputs bool[]
  static TfLiteRegistration r = {/* init= */ nullptr, /* free= */ nullptr,
                                 /* prepare= */ Prepare, /* invoke= */ Invoke};
  return &r;
}
};  // namespace string_bools

TfLiteStatus GenerateLabelsInvoke(TfLiteContext* context, TfLiteNode* node) {
  const TfLiteTensor* input_tensor = GetInput(context, node, 0);
  TF_LITE_ENSURE(context, input_tensor != nullptr);
  StringRef input_str_ref = GetString(input_tensor, 0);
  std::string input_str(input_str_ref.str, input_str_ref.len);
  if (input_str != kInputStr) {
    return kTfLiteError;
  }
  TfLiteTensor* output = GetOutput(context, node, 0);
  TF_LITE_ENSURE(context, output != nullptr);
  std::vector<std::string> data = {"label0", "label1", "label2"};
  TF_LITE_ENSURE(context,
                 tflite::task::core::PopulateTensor(data, output).ok());
  return kTfLiteOk;
}

// This custom op takes a string tensor in and outputs a string tensor with
// value{"label0", "label1", "label2"}, it's used to mimic a real text
// classification model that stores Class names inside a tensor.
TfLiteRegistration* Register_CUSTOM_OP_GENERATE_LABELS() {
  static TfLiteRegistration r = {/* init= */ nullptr,
                                 /* free= */ nullptr,
                                 /* prepare= */ nullptr,
                                 /* invoke= */ GenerateLabelsInvoke};
  return &r;
}

}  // namespace custom
}  // namespace ops
}  // namespace tflite

namespace tflite {
namespace task {
namespace text {
namespace nlclassifier {

std::unique_ptr<MutableOpResolver> CreateCustomResolver() {
  MutableOpResolver resolver;
  resolver.AddBuiltin(::tflite::BuiltinOperator_QUANTIZE,
                      ::tflite::ops::builtin::Register_QUANTIZE());
  resolver.AddCustom("CUSTOM_OP_STRING_TO_FLOATS",
                     ::tflite::ops::custom::string_floats::Register());
  resolver.AddCustom("CUSTOM_OP_STRING_TO_DOUBLES",
                     ::tflite::ops::custom::string_doubles::Register());
  resolver.AddCustom("CUSTOM_OP_STRING_TO_BOOLS",
                     ::tflite::ops::custom::string_bools::Register());
  resolver.AddCustom(
      "CUSTOM_OP_GENERATE_LABELS",
      ::tflite::ops::custom::Register_CUSTOM_OP_GENERATE_LABELS());
  return absl::make_unique<MutableOpResolver>(resolver);
}

}  // namespace nlclassifier
}  // namespace text
}  // namespace task
}  // namespace tflite
