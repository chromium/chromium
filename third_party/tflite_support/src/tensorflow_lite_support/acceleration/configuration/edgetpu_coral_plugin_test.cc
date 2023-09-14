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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "tensorflow/lite/acceleration/configuration/configuration_generated.h"
#include "tensorflow/lite/acceleration/configuration/delegate_registry.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow_lite_support/cc/task/vision/utils/image_utils.h"

namespace tflite {
namespace delegates {
namespace {

constexpr char kEdgeTpuModelFilePath[] =
    "tensorflow_lite_support/acceleration/configuration/testdata/"
    "mobilenet_v1_1.0_224_quant_edgetpu.tflite";
constexpr char kRegularModelFilePath[] =
    "tensorflow_lite_support/acceleration/configuration/testdata/"
    "mobilenet_v1_1.0_224_quant.tflite";
constexpr char kImagePath[] =
    "tensorflow_lite_support/acceleration/configuration/testdata/"
    "burger.jpg";

using ::tflite::task::vision::DecodeImageFromFile;
using ::tflite::task::vision::ImageData;
using ::tflite::task::vision::ImageDataFree;

using EdgeTpuCoralPluginTest = testing::TestWithParam<std::string>;

INSTANTIATE_TEST_SUITE_P(CoralPluginTests, EdgeTpuCoralPluginTest,
                         testing::Values(kRegularModelFilePath,
                                         kEdgeTpuModelFilePath));

TEST_P(EdgeTpuCoralPluginTest, CreateEdgeTpuCoralPlugin) {
  // Create the Coral delegate from the Coral plugin.
  flatbuffers::FlatBufferBuilder flatbuffer_builder;
  auto settings = flatbuffers::GetTemporaryPointer(
      flatbuffer_builder,
      CreateTFLiteSettings(flatbuffer_builder, tflite::Delegate_EDGETPU_CORAL));
  auto plugin = ::tflite::delegates::DelegatePluginRegistry::CreateByName(
      "EdgeTpuCoralPlugin", *settings);
  auto coral_delegate = plugin->Create();

  // Load the tflite model file.
  std::unique_ptr<::tflite::FlatBufferModel> tflite_model =
      ::tflite::FlatBufferModel::BuildFromFile(GetParam().c_str());
  ASSERT_NE(tflite_model, nullptr);

  // Create the tflite interpreter.
  tflite::ops::builtin::BuiltinOpResolver resolver;
  std::unique_ptr<::tflite::Interpreter> interpreter;
  ASSERT_EQ(::tflite::InterpreterBuilder(*tflite_model, resolver)(&interpreter),
            kTfLiteOk);
  ASSERT_NE(interpreter, nullptr);
  interpreter->ModifyGraphWithDelegate(coral_delegate.get());

  // Verifies that interpreter runs correctly.
  // To open source the code under tensorflow/lite, the following code needs to
  // be stript from the Task library dependency, meaning forking or rewriting
  // `LoadImage` and `ImageData`.
  // `ASSERT_OK_AND_ASSIGN` is not available externally.
  auto rgb_image_or = DecodeImageFromFile(kImagePath);
  ASSERT_TRUE(rgb_image_or.ok());

  ImageData rgb_image = rgb_image_or.value();
  const uint8_t* input_data = rgb_image.pixel_data;
  size_t input_data_byte_size =
      rgb_image.width * rgb_image.height * rgb_image.channels * sizeof(uint8_t);

  ASSERT_EQ(interpreter->AllocateTensors(), kTfLiteOk);
  uint8_t* input_tensor = interpreter->typed_input_tensor<uint8_t>(0);
  memcpy(input_tensor, input_data, input_data_byte_size);
  ASSERT_EQ(interpreter->Invoke(), kTfLiteOk);
  uint8_t* output_tensor = interpreter->typed_output_tensor<uint8_t>(0);
  // `cheeseburger` is the 935th item in the label file of
  // "mobilenet_v1_1.0_224_quant_edgetpu.tflite". See labels.txt.
  EXPECT_EQ(output_tensor[934], 255);
  ImageDataFree(&rgb_image);
}

}  // namespace
}  // namespace delegates
}  // namespace tflite
