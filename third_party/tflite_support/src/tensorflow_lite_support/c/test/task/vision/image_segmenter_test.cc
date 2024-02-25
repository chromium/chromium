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

#include "tensorflow_lite_support/c/task/vision/image_segmenter.h"

#include <string.h>

#include <cstdio>

#include "tensorflow/lite/test_util.h"
#include "tensorflow_lite_support/c/common.h"
#include "tensorflow_lite_support/c/task/processor/segmentation_result.h"
#include "tensorflow_lite_support/c/task/vision/core/frame_buffer.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/status_matchers.h"
#include "tensorflow_lite_support/cc/task/vision/utils/image_utils.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"

namespace tflite {
namespace task {
namespace vision {
namespace {

using ::testing::HasSubstr;
using ::testing::StrEq;
using ::tflite::support::StatusOr;
using ::tflite::task::JoinPath;

constexpr char kTestDataDirectory[] =
    "/tensorflow_lite_support/cc/test/testdata/task/"
    "vision/";
// Quantized model.
constexpr char kDeepLabV3[] = "deeplabv3.tflite";

StatusOr<ImageData> LoadImage(const char* image_name) {
  return DecodeImageFromFile(JoinPath("./" /*test src dir*/,
                                      kTestDataDirectory, image_name));
}

// The maximum fraction of pixels in the candidate mask that can have a
// different class than the golden mask for the test to pass.
constexpr float kGoldenMaskTolerance = 1e-2;
// Magnification factor used when creating the golden category masks to make
// them more human-friendly. Each pixel in the golden masks has its value
// multiplied by this factor, i.e. a value of 10 means class index 1, a value of
// 20 means class index 2, etc.
constexpr int kGoldenMaskMagnificationFactor = 10;

void InitializeColoredLabel(TfLiteColoredLabel& colored_label, uint32_t r,
                            uint32_t g, uint32_t b, const char* label) {
  colored_label.r = r;
  colored_label.g = g;
  colored_label.b = b;
  colored_label.label = strdup(label);
  colored_label.display_name = nullptr;
}

TfLiteSegmentation CreatePartialDeepLabV3Segmentation() {
  TfLiteSegmentation segmentation = {.width = 257, .height = 257};
  segmentation.colored_labels = new TfLiteColoredLabel[21];
  segmentation.colored_labels_size = 21;
  InitializeColoredLabel(segmentation.colored_labels[0], 0, 0, 0, "background");
  InitializeColoredLabel(segmentation.colored_labels[1], 128, 0, 0,
                         "aeroplane");
  InitializeColoredLabel(segmentation.colored_labels[2], 0, 128, 0, "bicycle");
  InitializeColoredLabel(segmentation.colored_labels[3], 128, 128, 0, "bird");
  InitializeColoredLabel(segmentation.colored_labels[4], 0, 0, 128, "boat");
  InitializeColoredLabel(segmentation.colored_labels[5], 128, 0, 128, "bottle");
  InitializeColoredLabel(segmentation.colored_labels[6], 0, 128, 128, "bus");
  InitializeColoredLabel(segmentation.colored_labels[7], 128, 128, 128, "car");
  InitializeColoredLabel(segmentation.colored_labels[8], 64, 0, 0, "cat");
  InitializeColoredLabel(segmentation.colored_labels[9], 192, 0, 0, "chair");
  InitializeColoredLabel(segmentation.colored_labels[10], 64, 128, 0, "cow");
  InitializeColoredLabel(segmentation.colored_labels[11], 192, 128, 0,
                         "dining table");
  InitializeColoredLabel(segmentation.colored_labels[12], 64, 0, 128, "dog");
  InitializeColoredLabel(segmentation.colored_labels[13], 192, 0, 128, "horse");
  InitializeColoredLabel(segmentation.colored_labels[14], 64, 128, 128,
                         "motorbike");
  InitializeColoredLabel(segmentation.colored_labels[15], 192, 128, 128,
                         "person");
  InitializeColoredLabel(segmentation.colored_labels[16], 0, 64, 0,
                         "potted plant");
  InitializeColoredLabel(segmentation.colored_labels[17], 128, 64, 0, "sheep");
  InitializeColoredLabel(segmentation.colored_labels[18], 0, 192, 0, "sofa");
  InitializeColoredLabel(segmentation.colored_labels[19], 128, 192, 0, "train");
  InitializeColoredLabel(segmentation.colored_labels[20], 0, 64, 128, "tv");

  return segmentation;
}

TfLiteSegmentation partial_deep_lab_v3_segmentation =
    CreatePartialDeepLabV3Segmentation();

// Checks that the two provided `TfLiteSegmentation`s are equal.
void ExpectPartiallyEqual(const TfLiteSegmentation& actual,
                          const TfLiteSegmentation& expected) {
  EXPECT_EQ(actual.height, expected.height);
  EXPECT_EQ(actual.width, expected.width);
  for (int i = 0; i < actual.colored_labels_size; i++) {
    EXPECT_EQ(actual.colored_labels[i].r, expected.colored_labels[i].r);
    EXPECT_EQ(actual.colored_labels[i].g, expected.colored_labels[i].g);
    EXPECT_EQ(actual.colored_labels[i].b, expected.colored_labels[i].b);
    EXPECT_THAT(actual.colored_labels[i].label,
                StrEq(expected.colored_labels[i].label));
  }
}

class ImageSegmenterFromOptionsTest : public tflite::testing::Test {};

TEST_F(ImageSegmenterFromOptionsTest, FailsWithNullOptionsAndError) {
  TfLiteSupportError* error = nullptr;

  TfLiteImageSegmenter* image_segmenter =
      TfLiteImageSegmenterFromOptions(nullptr, &error);

  EXPECT_EQ(image_segmenter, nullptr);

  if (image_segmenter) TfLiteImageSegmenterDelete(image_segmenter);

  ASSERT_NE(error, nullptr);
  EXPECT_EQ(error->code, kInvalidArgumentError);
  EXPECT_NE(error->message, nullptr);
  EXPECT_THAT(error->message, HasSubstr("Expected non null options"));

  TfLiteSupportErrorDelete(error);
}

TEST_F(ImageSegmenterFromOptionsTest, FailsWithMissingModelPath) {
  TfLiteImageSegmenterOptions options = TfLiteImageSegmenterOptionsCreate();

  TfLiteImageSegmenter* image_segmenter =
      TfLiteImageSegmenterFromOptions(&options, nullptr);

  EXPECT_EQ(image_segmenter, nullptr);

  if (image_segmenter) TfLiteImageSegmenterDelete(image_segmenter);
}

TEST_F(ImageSegmenterFromOptionsTest, FailsWithMissingModelPathAndError) {
  TfLiteImageSegmenterOptions options = TfLiteImageSegmenterOptionsCreate();

  TfLiteSupportError* error = nullptr;

  TfLiteImageSegmenter* image_segmenter =
      TfLiteImageSegmenterFromOptions(&options, &error);

  EXPECT_EQ(image_segmenter, nullptr);

  if (image_segmenter) TfLiteImageSegmenterDelete(image_segmenter);

  ASSERT_NE(error, nullptr);
  EXPECT_EQ(error->code, kInvalidArgumentError);
  EXPECT_NE(error->message, nullptr);
  EXPECT_THAT(error->message, HasSubstr("`base_options.model_file`"));

  TfLiteSupportErrorDelete(error);
}

TEST_F(ImageSegmenterFromOptionsTest, SucceedsWithModelPath) {
  std::string model_path = JoinPath("./" /*test src dir*/,
                                    kTestDataDirectory, kDeepLabV3);

  TfLiteImageSegmenterOptions options = TfLiteImageSegmenterOptionsCreate();
  options.base_options.model_file.file_path = model_path.data();

  TfLiteImageSegmenter* image_segmenter =
      TfLiteImageSegmenterFromOptions(&options, nullptr);

  EXPECT_NE(image_segmenter, nullptr);

  TfLiteImageSegmenterDelete(image_segmenter);
}

TEST_F(ImageSegmenterFromOptionsTest, SucceedsWithNumberOfThreadsAndError) {
  std::string model_path = JoinPath("./" /*test src dir*/,
                                    kTestDataDirectory, kDeepLabV3);

  TfLiteImageSegmenterOptions options = TfLiteImageSegmenterOptionsCreate();
  options.base_options.model_file.file_path = model_path.data();
  options.base_options.compute_settings.cpu_settings.num_threads = 3;

  TfLiteSupportError* error = nullptr;
  TfLiteImageSegmenter* image_segmenter =
      TfLiteImageSegmenterFromOptions(&options, &error);

  EXPECT_NE(image_segmenter, nullptr);
  EXPECT_EQ(error, nullptr);

  if (image_segmenter) TfLiteImageSegmenterDelete(image_segmenter);
  if (error) TfLiteSupportErrorDelete(error);
}

TEST_F(ImageSegmenterFromOptionsTest, FailsWithUnspecifiedOutputTypeAndError) {
  std::string model_path = JoinPath("./" /*test src dir*/,
                                    kTestDataDirectory, kDeepLabV3);

  TfLiteImageSegmenterOptions options = TfLiteImageSegmenterOptionsCreate();
  options.base_options.model_file.file_path = model_path.data();
  options.output_type = kUnspecified;

  TfLiteSupportError* error = nullptr;
  TfLiteImageSegmenter* image_segmenter =
      TfLiteImageSegmenterFromOptions(&options, &error);

  EXPECT_EQ(image_segmenter, nullptr);
  EXPECT_NE(error, nullptr);

  if (image_segmenter) TfLiteImageSegmenterDelete(image_segmenter);
  if (error) TfLiteSupportErrorDelete(error);
}

class ImageSegmenterSegmentTest : public tflite::testing::Test {
 protected:
  void SetUp() override {
    std::string model_path = JoinPath("./" /*test src dir*/,
                                      kTestDataDirectory, kDeepLabV3);

    TfLiteImageSegmenterOptions options = TfLiteImageSegmenterOptionsCreate();
    options.base_options.model_file.file_path = model_path.data();
    image_segmenter = TfLiteImageSegmenterFromOptions(&options, nullptr);
    ASSERT_NE(image_segmenter, nullptr);
  }

  void TearDown() override { TfLiteImageSegmenterDelete(image_segmenter); }
  TfLiteImageSegmenter* image_segmenter;
};

TEST_F(ImageSegmenterSegmentTest, SucceedsWithCategoryMask) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData image_data,
                       LoadImage("segmentation_input_rotation0.jpg"));

  TfLiteFrameBuffer frame_buffer = {
      .format = kRGB,
      .orientation = kTopLeft,
      .dimension = {.width = image_data.width, .height = image_data.height},
      .buffer = image_data.pixel_data};

  TfLiteSegmentationResult* segmentation_result =
      TfLiteImageSegmenterSegment(image_segmenter, &frame_buffer, nullptr);

  ImageDataFree(&image_data);

  ASSERT_NE(segmentation_result, nullptr);
  EXPECT_EQ(segmentation_result->size, 1);
  EXPECT_NE(segmentation_result->segmentations, nullptr);
  EXPECT_NE(segmentation_result->segmentations[0].category_mask, nullptr);

  ExpectPartiallyEqual(partial_deep_lab_v3_segmentation,
                       segmentation_result->segmentations[0]);

  // Load golden mask output.
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData golden_mask,
                       LoadImage("segmentation_golden_rotation0.png"));

  int inconsistent_pixels = 0;
  int num_pixels = golden_mask.height * golden_mask.width;

  for (int i = 0; i < num_pixels; ++i) {
    inconsistent_pixels +=
        (segmentation_result->segmentations[0].category_mask[i] *
             kGoldenMaskMagnificationFactor !=
         golden_mask.pixel_data[i]);
  }

  EXPECT_LT(static_cast<float>(inconsistent_pixels) / num_pixels,
            kGoldenMaskTolerance);

  ImageDataFree(&golden_mask);

  TfLiteSegmentationResultDelete(segmentation_result);
}

TEST_F(ImageSegmenterSegmentTest, SucceedsWithCategoryMaskAndOrientation) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData image_data,
                       LoadImage("segmentation_input_rotation90_flop.jpg"));

  TfLiteFrameBuffer frame_buffer = {
      .format = kRGB,
      .orientation = kRightBottom,
      .dimension = {.width = image_data.width, .height = image_data.height},
      .buffer = image_data.pixel_data};

  TfLiteSegmentationResult* segmentation_result =
      TfLiteImageSegmenterSegment(image_segmenter, &frame_buffer, nullptr);

  ImageDataFree(&image_data);

  ASSERT_NE(segmentation_result, nullptr);
  EXPECT_EQ(segmentation_result->size, 1);
  EXPECT_NE(segmentation_result->segmentations, nullptr);
  EXPECT_NE(segmentation_result->segmentations[0].category_mask, nullptr);

  ExpectPartiallyEqual(partial_deep_lab_v3_segmentation,
                       segmentation_result->segmentations[0]);

  // Load golden mask output.
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData golden_mask,
                       LoadImage("segmentation_golden_rotation90_flop.png"));

  int inconsistent_pixels = 0;
  int num_pixels = golden_mask.height * golden_mask.width;

  for (int i = 0; i < num_pixels; ++i) {
    inconsistent_pixels +=
        (segmentation_result->segmentations[0].category_mask[i] *
             kGoldenMaskMagnificationFactor !=
         golden_mask.pixel_data[i]);
  }

  EXPECT_LT(static_cast<float>(inconsistent_pixels) / num_pixels,
            kGoldenMaskTolerance);

  ImageDataFree(&golden_mask);

  TfLiteSegmentationResultDelete(segmentation_result);
}

}  // namespace
}  // namespace vision
}  // namespace task
}  // namespace tflite
