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

#include "tensorflow_lite_support/c/task/vision/object_detector.h"

#include <string.h>

#include "tensorflow/lite/test_util.h"
#include "tensorflow_lite_support/c/common.h"
#include "tensorflow_lite_support/c/task/processor/detection_result.h"
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
using ::testing::StrNe;
using ::tflite::support::StatusOr;
using ::tflite::task::JoinPath;

constexpr char kTestDataDirectory[] =
    "/tensorflow_lite_support/cc/test/testdata/task/"
    "vision/";
// Quantized model.
constexpr char kMobileSsdWithMetadata[] =
    "coco_ssd_mobilenet_v1_1.0_quant_2018_06_29.tflite";
constexpr int kMaxPixelOffset = 5;

StatusOr<ImageData> LoadImage(const char* image_name) {
  return DecodeImageFromFile(JoinPath("./" /*test src dir*/,
                                      kTestDataDirectory, image_name));
}

void VerifyDetection(const TfLiteDetection& detection,
                     TfLiteBoundingBox expected_bounding_box,
                     float expected_first_score,
                     const std::string& expected_first_label) {
  EXPECT_GE(detection.size, 1);
  EXPECT_NE(detection.categories, nullptr);
  EXPECT_NEAR(detection.bounding_box.origin_x, expected_bounding_box.origin_x
              , kMaxPixelOffset
             );
  EXPECT_NEAR(detection.bounding_box.origin_y, expected_bounding_box.origin_y
              , kMaxPixelOffset
             );
  EXPECT_NEAR(detection.bounding_box.height, expected_bounding_box.height
              , kMaxPixelOffset
             );
  EXPECT_NEAR(detection.bounding_box.width, expected_bounding_box.width
              , kMaxPixelOffset
             );

  EXPECT_THAT(detection.categories[0].label, StrEq(expected_first_label));
  EXPECT_NEAR(detection.categories[0].score, expected_first_score, 0.05);
}

void VerifyResults(TfLiteDetectionResult* detection_result) {
  ASSERT_NE(detection_result, nullptr);
  EXPECT_GE(detection_result->size, 1);
  EXPECT_NE(detection_result->detections, nullptr);

  VerifyDetection(
      detection_result->detections[0],
      {.origin_x = 54, .origin_y = 396, .width = 393, .height = 196},
      0.64453125, "cat");
  VerifyDetection(
      detection_result->detections[1],
      {.origin_x = 602, .origin_y = 157, .width = 394, .height = 447},
      0.59765625, "cat");
  VerifyDetection(
      detection_result->detections[2],
      {.origin_x = 261, .origin_y = 394, .width = 179, .height = 209}, 0.5625,
      "cat");
  VerifyDetection(
      detection_result->detections[3],
      {.origin_x = 389, .origin_y = 197, .width = 276, .height = 409},
      0.51171875, "dog");
}

class ObjectDetectorFromOptionsTest : public tflite::testing::Test {};

TEST_F(ObjectDetectorFromOptionsTest, FailsWithNullOptionsAndError) {
  TfLiteSupportError* error = nullptr;
  TfLiteObjectDetector* object_detector =
      TfLiteObjectDetectorFromOptions(nullptr, &error);

  EXPECT_EQ(object_detector, nullptr);
  if (object_detector) TfLiteObjectDetectorDelete(object_detector);

  ASSERT_NE(error, nullptr);
  EXPECT_EQ(error->code, kInvalidArgumentError);
  EXPECT_NE(error->message, nullptr);
  EXPECT_THAT(error->message, HasSubstr("Expected non null options"));

  TfLiteSupportErrorDelete(error);
}

TEST_F(ObjectDetectorFromOptionsTest, FailsWithMissingModelPath) {
  TfLiteObjectDetectorOptions options = TfLiteObjectDetectorOptionsCreate();
  TfLiteObjectDetector* object_detector =
      TfLiteObjectDetectorFromOptions(&options, nullptr);
  EXPECT_EQ(object_detector, nullptr);
  if (object_detector) TfLiteObjectDetectorDelete(object_detector);
}

TEST_F(ObjectDetectorFromOptionsTest, FailsWithMissingModelPathAndError) {
  TfLiteObjectDetectorOptions options = TfLiteObjectDetectorOptionsCreate();

  TfLiteSupportError* error = nullptr;
  TfLiteObjectDetector* object_detector =
      TfLiteObjectDetectorFromOptions(&options, &error);

  EXPECT_EQ(object_detector, nullptr);
  if (object_detector) TfLiteObjectDetectorDelete(object_detector);

  ASSERT_NE(error, nullptr);
  EXPECT_EQ(error->code, kInvalidArgumentError);
  EXPECT_NE(error->message, nullptr);
  EXPECT_THAT(error->message, HasSubstr("`base_options.model_file`"));

  TfLiteSupportErrorDelete(error);
}

TEST_F(ObjectDetectorFromOptionsTest, SucceedsWithModelPath) {
  std::string model_path = JoinPath("./" /*test src dir*/,
                                    kTestDataDirectory, kMobileSsdWithMetadata);
  TfLiteObjectDetectorOptions options = TfLiteObjectDetectorOptionsCreate();
  options.base_options.model_file.file_path = model_path.data();
  TfLiteObjectDetector* object_detector =
      TfLiteObjectDetectorFromOptions(&options, nullptr);

  EXPECT_NE(object_detector, nullptr);
  TfLiteObjectDetectorDelete(object_detector);
}

TEST_F(ObjectDetectorFromOptionsTest, SucceedsWithNumberOfThreadsAndError) {
  std::string model_path = JoinPath("./" /*test src dir*/,
                                    kTestDataDirectory, kMobileSsdWithMetadata);
  TfLiteObjectDetectorOptions options = TfLiteObjectDetectorOptionsCreate();
  options.base_options.model_file.file_path = model_path.data();
  options.base_options.compute_settings.cpu_settings.num_threads = 3;

  TfLiteSupportError* error = nullptr;
  TfLiteObjectDetector* object_detector =
      TfLiteObjectDetectorFromOptions(&options, &error);

  EXPECT_NE(object_detector, nullptr);
  EXPECT_EQ(error, nullptr);

  if (object_detector) TfLiteObjectDetectorDelete(object_detector);
  if (error) TfLiteSupportErrorDelete(error);
}

TEST_F(ObjectDetectorFromOptionsTest,
       FailsWithClassNameDenyListAndClassNameAllowListAndError) {
  std::string model_path = JoinPath("./" /*test src dir*/,
                                    kTestDataDirectory, kMobileSsdWithMetadata);

  TfLiteObjectDetectorOptions options = TfLiteObjectDetectorOptionsCreate();
  options.base_options.model_file.file_path = model_path.data();

  char* label_denylist[9] = {(char*)"cat"};
  options.classification_options.label_denylist.list = label_denylist;
  options.classification_options.label_denylist.length = 1;

  char* label_allowlist[12] = {(char*)"dog"};
  options.classification_options.label_allowlist.list = label_allowlist;
  options.classification_options.label_allowlist.length = 1;

  TfLiteSupportError* error = nullptr;
  TfLiteObjectDetector* object_detector =
      TfLiteObjectDetectorFromOptions(&options, &error);

  EXPECT_EQ(object_detector, nullptr);
  if (object_detector) TfLiteObjectDetectorDelete(object_detector);

  ASSERT_NE(error, nullptr);
  EXPECT_EQ(error->code, kInvalidArgumentError);
  EXPECT_NE(error->message, nullptr);
  EXPECT_THAT(error->message, HasSubstr("mutually exclusive options"));

  TfLiteSupportErrorDelete(error);
}

TEST(ObjectDetectorNullDetectorDetectTest,
     FailsWithNullObjectDetectorAndError) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData image_data, LoadImage("cats_and_dogs.jpg"));

  TfLiteSupportError* error = nullptr;
  TfLiteDetectionResult* detection_result =
      TfLiteObjectDetectorDetect(nullptr, nullptr, &error);

  ImageDataFree(&image_data);

  EXPECT_EQ(detection_result, nullptr);
  if (detection_result) TfLiteDetectionResultDelete(detection_result);

  ASSERT_NE(error, nullptr);
  EXPECT_EQ(error->code, kInvalidArgumentError);
  EXPECT_NE(error->message, nullptr);
  EXPECT_THAT(error->message, HasSubstr("Expected non null object detector."));

  TfLiteSupportErrorDelete(error);
}

class ObjectDetectorDetectTest : public tflite::testing::Test {
 protected:
  void SetUp() override {
    std::string model_path =
        JoinPath("./" /*test src dir*/, kTestDataDirectory,
                 kMobileSsdWithMetadata);

    TfLiteObjectDetectorOptions options = TfLiteObjectDetectorOptionsCreate();
    options.base_options.model_file.file_path = model_path.data();
    object_detector = TfLiteObjectDetectorFromOptions(&options, nullptr);
    ASSERT_NE(object_detector, nullptr);
  }

  void TearDown() override { TfLiteObjectDetectorDelete(object_detector); }
  TfLiteObjectDetector* object_detector;
};

TEST_F(ObjectDetectorDetectTest, SucceedsWithImageData) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData image_data, LoadImage("cats_and_dogs.jpg"));

  TfLiteFrameBuffer frame_buffer = {
      .format = kRGB,
      .orientation = kTopLeft,
      .dimension = {.width = image_data.width, .height = image_data.height},
      .buffer = image_data.pixel_data};

  TfLiteDetectionResult* detection_result =
      TfLiteObjectDetectorDetect(object_detector, &frame_buffer, nullptr);

  ImageDataFree(&image_data);

  VerifyResults(detection_result);

  TfLiteDetectionResultDelete(detection_result);
}

TEST_F(ObjectDetectorDetectTest, FailsWithNullFrameBufferAndError) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData image_data, LoadImage("cats_and_dogs.jpg"));

  TfLiteSupportError* error = nullptr;
  TfLiteDetectionResult* detection_result =
      TfLiteObjectDetectorDetect(object_detector, nullptr, &error);

  ImageDataFree(&image_data);

  EXPECT_EQ(detection_result, nullptr);
  if (detection_result) TfLiteDetectionResultDelete(detection_result);

  ASSERT_NE(error, nullptr);
  EXPECT_EQ(error->code, kInvalidArgumentError);
  EXPECT_NE(error->message, nullptr);
  EXPECT_THAT(error->message, HasSubstr("Expected non null frame buffer"));

  TfLiteSupportErrorDelete(error);
}

TEST_F(ObjectDetectorDetectTest, FailsWithNullImageDataAndError) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData image_data, LoadImage("cats_and_dogs.jpg"));
  TfLiteSupportError* error = nullptr;
  TfLiteDetectionResult* detection_result =
      TfLiteObjectDetectorDetect(object_detector, nullptr, &error);

  ImageDataFree(&image_data);

  EXPECT_EQ(detection_result, nullptr);
  if (detection_result) TfLiteDetectionResultDelete(detection_result);

  ASSERT_NE(error, nullptr);
  EXPECT_EQ(error->code, kInvalidArgumentError);
  EXPECT_NE(error->message, nullptr);
  EXPECT_THAT(error->message, HasSubstr("INVALID_ARGUMENT"));

  TfLiteSupportErrorDelete(error);
}

TEST(ObjectDetectorWithUserDefinedOptionsDetectorTest,
     SucceedsWithClassNameDenyList) {
  char* denylisted_label_name = (char*)"cat";
  std::string model_path = JoinPath("./" /*test src dir*/,
                                    kTestDataDirectory, kMobileSsdWithMetadata);

  TfLiteObjectDetectorOptions options = TfLiteObjectDetectorOptionsCreate();
  options.base_options.model_file.file_path = model_path.data();

  char* label_denylist[12] = {denylisted_label_name};
  options.classification_options.label_denylist.list = label_denylist;
  options.classification_options.label_denylist.length = 1;

  TfLiteObjectDetector* object_detector =
      TfLiteObjectDetectorFromOptions(&options, nullptr);
  ASSERT_NE(object_detector, nullptr);

  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData image_data, LoadImage("cats_and_dogs.jpg"));

  TfLiteFrameBuffer frame_buffer = {
      .format = kRGB,
      .orientation = kTopLeft,
      .dimension = {.width = image_data.width, .height = image_data.height},
      .buffer = image_data.pixel_data};

  TfLiteDetectionResult* detection_result =
      TfLiteObjectDetectorDetect(object_detector, &frame_buffer, nullptr);

  ImageDataFree(&image_data);
  if (object_detector) TfLiteObjectDetectorDelete(object_detector);

  ASSERT_NE(detection_result, nullptr);
  EXPECT_GE(detection_result->size, 1);
  EXPECT_NE(detection_result->detections, nullptr);
  EXPECT_GE(detection_result->detections->size, 1);
  EXPECT_NE(detection_result->detections[0].categories, nullptr);
  EXPECT_THAT(detection_result->detections[0].categories[0].label,
              StrNe(denylisted_label_name));

  TfLiteDetectionResultDelete(detection_result);
}

TEST(ObjectDetectorWithUserDefinedOptionsDetectorTest,
     SucceedsWithClassNameAllowList) {
  char* allowlisted_label_name = (char*)"cat";
  std::string model_path = JoinPath("./" /*test src dir*/,
                                    kTestDataDirectory, kMobileSsdWithMetadata)
                               .data();

  TfLiteObjectDetectorOptions options = TfLiteObjectDetectorOptionsCreate();
  options.base_options.model_file.file_path = model_path.data();

  char* label_allowlist[12] = {allowlisted_label_name};
  options.classification_options.label_allowlist.list = label_allowlist;
  options.classification_options.label_allowlist.length = 1;

  TfLiteObjectDetector* object_detector =
      TfLiteObjectDetectorFromOptions(&options, nullptr);
  ASSERT_NE(object_detector, nullptr);

  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData image_data, LoadImage("cats_and_dogs.jpg"));

  TfLiteFrameBuffer frame_buffer = {
      .format = kRGB,
      .orientation = kTopLeft,
      .dimension = {.width = image_data.width, .height = image_data.height},
      .buffer = image_data.pixel_data};

  TfLiteDetectionResult* detection_result =
      TfLiteObjectDetectorDetect(object_detector, &frame_buffer, nullptr);

  ImageDataFree(&image_data);
  if (object_detector) TfLiteObjectDetectorDelete(object_detector);

  ASSERT_NE(detection_result, nullptr);
  EXPECT_GE(detection_result->size, 1);
  EXPECT_NE(detection_result->detections, nullptr);
  EXPECT_GE(detection_result->detections->size, 1);
  EXPECT_NE(detection_result->detections[0].categories, nullptr);
  EXPECT_THAT(detection_result->detections[0].categories[0].label,
              StrEq(allowlisted_label_name));

  TfLiteDetectionResultDelete(detection_result);
}

TEST(ObjectDetectorWithUserDefinedOptionsDetectorTest,
     SucceedsWithScoreThreshold) {
  std::string model_path = JoinPath("./" /*test src dir*/,
                                    kTestDataDirectory, kMobileSsdWithMetadata)
                               .data();

  TfLiteObjectDetectorOptions options = TfLiteObjectDetectorOptionsCreate();
  options.base_options.model_file.file_path = model_path.data();

  options.classification_options.score_threshold = 0.6;

  TfLiteObjectDetector* object_detector =
      TfLiteObjectDetectorFromOptions(&options, nullptr);
  ASSERT_NE(object_detector, nullptr);

  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData image_data, LoadImage("cats_and_dogs.jpg"));

  TfLiteFrameBuffer frame_buffer = {
      .format = kRGB,
      .orientation = kTopLeft,
      .dimension = {.width = image_data.width, .height = image_data.height},
      .buffer = image_data.pixel_data};

  TfLiteDetectionResult* detection_result =
      TfLiteObjectDetectorDetect(object_detector, &frame_buffer, nullptr);

  ImageDataFree(&image_data);
  if (object_detector) TfLiteObjectDetectorDelete(object_detector);

  ASSERT_NE(detection_result, nullptr);
  EXPECT_EQ(detection_result->size, 2);

  EXPECT_NE(detection_result->detections, nullptr);
  VerifyDetection(
      detection_result->detections[0],
      {.origin_x = 54, .origin_y = 396, .width = 393, .height = 196},
      0.64453125, "cat");
  VerifyDetection(
      detection_result->detections[1],
      {.origin_x = 602, .origin_y = 157, .width = 394, .height = 447}, 0.609375,
      "cat");

  TfLiteDetectionResultDelete(detection_result);
}

}  // namespace
}  // namespace vision
}  // namespace task
}  // namespace tflite
