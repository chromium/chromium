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

#include "tensorflow_lite_support/cc/task/processor/image_preprocessor.h"

#include <memory>

#include "absl/status/status.h"  // from @com_google_absl
#include "tensorflow/lite/test_util.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/status_matchers.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/image_utils.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"

namespace tflite {
namespace task {
namespace processor {
namespace {

using ::tflite::support::StatusOr;
using ::tflite::task::JoinPath;
using ::tflite::task::core::TfLiteEngine;
using ::tflite::task::vision::DecodeImageFromFile;
using ::tflite::task::vision::FrameBuffer;
using ::tflite::task::vision::ImageData;

constexpr char kTestDataDirectory[] =
    "/tensorflow_lite_support/cc/test/testdata/task/"
    "vision/";

constexpr char kDilatedConvolutionModelWithMetaData[] = "dilated_conv.tflite";

StatusOr<ImageData> LoadImage(std::string image_name) {
  return DecodeImageFromFile(JoinPath("./" /*test src dir*/,
                                      kTestDataDirectory, image_name));
}

class DynamicInputTest : public tflite::testing::Test {
 protected:
  void PreprocessImage() {
    engine_ = absl::make_unique<TfLiteEngine>();
    SUPPORT_ASSERT_OK(engine_->BuildModelFromFile(
        JoinPath("./" /*test src dir*/, kTestDataDirectory,
                 kDilatedConvolutionModelWithMetaData)));
    SUPPORT_ASSERT_OK(engine_->InitInterpreter());

    SUPPORT_ASSERT_OK_AND_ASSIGN(auto preprocessor,
                         ImagePreprocessor::Create(engine_.get(), {0}));

    SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData image, LoadImage("burger.jpg"));
    std::unique_ptr<FrameBuffer> frame_buffer = CreateFromRgbRawBuffer(
        image.pixel_data, FrameBuffer::Dimension{image.width, image.height});

    SUPPORT_ASSERT_OK(preprocessor->Preprocess(*frame_buffer));

    ImageDataFree(&image);
  }

  std::unique_ptr<TfLiteEngine> engine_ = nullptr;
};

// See if output tensor has been re-dimmed as per the input
// tensor. Expected shape: (1, input_height, input_width, 16).
TEST_F(DynamicInputTest, OutputDimensionCheck) {
  PreprocessImage();

  EXPECT_TRUE(engine_->interpreter_wrapper()->InvokeWithoutFallback().ok());
  EXPECT_EQ(engine_->GetOutputs()[0]->dims->data[0], 1);
  EXPECT_EQ(engine_->GetOutputs()[0]->dims->data[1],
            engine_->GetInputs()[0]->dims->data[1]);
  EXPECT_EQ(engine_->GetOutputs()[0]->dims->data[2],
            engine_->GetInputs()[0]->dims->data[2]);
  EXPECT_EQ(engine_->GetOutputs()[0]->dims->data[3], 16);
}

// Compare pre-processed input with an already pre-processed
// golden image.
TEST_F(DynamicInputTest, GoldenImageComparison) {
  PreprocessImage();

  // Get the processed input image.
  SUPPORT_ASSERT_OK_AND_ASSIGN(float* processed_input_data,
                       tflite::task::core::AssertAndReturnTypedTensor<float>(
                           engine_->GetInputs()[0]));

  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData image, LoadImage("burger.jpg"));
  const uint8* image_data = image.pixel_data;
  const size_t image_size = image.width * image.height * 3;

  for (size_t i = 0; i < image_size; ++i, ++image_data, ++processed_input_data)
    EXPECT_NEAR(static_cast<float>(*image_data), *processed_input_data,
                std::numeric_limits<float>::epsilon());

  ImageDataFree(&image);
}

}  // namespace
}  // namespace processor
}  // namespace task
}  // namespace tflite
