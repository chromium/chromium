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

#include "tensorflow_lite_support/cc/task/vision/object_detector.h"

#include <memory>

#include "absl/flags/flag.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/cord.h"  // from @com_google_absl
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/kernels/builtin_op_kernels.h"
#include "tensorflow/lite/mutable_op_resolver.h"
#include "tensorflow/lite/test_util.h"
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/status_matchers.h"
#include "tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/task/core/tflite_engine.h"
#include "tensorflow_lite_support/cc/task/vision/core/frame_buffer.h"
#include "tensorflow_lite_support/cc/task/vision/proto/bounding_box_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/detections_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/object_detector_options_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/image_utils.h"
#include "tensorflow_lite_support/cc/test/message_matchers.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"

namespace tflite {

namespace ops {
namespace custom {

// Forward declaration for the custom Detection_PostProcess op.
//
// See:
// https://medium.com/@bsramasubramanian/running-a-tensorflow-lite-model-in-python-with-custom-ops-9b2b46efd355
TfLiteRegistration* Register_DETECTION_POSTPROCESS();

}  // namespace custom
}  // namespace ops

namespace task {
namespace vision {
namespace {

using ::testing::HasSubstr;
using ::testing::Optional;
using ::tflite::support::kTfLiteSupportPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::task::JoinPath;
using ::tflite::task::ParseTextProtoOrDie;
using ::tflite::task::core::PopulateTensor;
using ::tflite::task::core::TaskAPIFactory;
using ::tflite::task::core::TfLiteEngine;

constexpr char kTestDataDirectory[] =
    "/tensorflow_lite_support/cc/test/testdata/task/"
    "vision/";
constexpr char kMobileSsdWithMetadata[] =
    "coco_ssd_mobilenet_v1_1.0_quant_2018_06_29.tflite";
constexpr char kExpectResults[] =
    R"pb(detections {
           bounding_box { origin_x: 54 origin_y: 396 width: 393 height: 196 }
           classes { index: 16 score: 0.64 class_name: "cat" }
         }
         detections {
           bounding_box { origin_x: 602 origin_y: 157 width: 394 height: 447 }
           classes { index: 16 score: 0.61 class_name: "cat" }
         }
         detections {
           bounding_box { origin_x: 260 origin_y: 394 width: 180 height: 209 }
           # Actually a dog, but the model gets confused.
           classes { index: 16 score: 0.56 class_name: "cat" }
         }
         detections {
           bounding_box { origin_x: 389 origin_y: 197 width: 278 height: 409 }
           classes { index: 17 score: 0.5 class_name: "dog" }
         }
    )pb";
constexpr char kMobileSsdWithMetadataDummyScoreCalibration[] =
    "coco_ssd_mobilenet_v1_1.0_quant_2018_06_29_score_calibration.tflite";
// The model has different output tensor order.
constexpr char kEfficientDetWithMetadata[] =
    "coco_efficientdet_lite0_v1_1.0_quant_2021_09_06.tflite";

StatusOr<ImageData> LoadImage(std::string image_name) {
  return DecodeImageFromFile(JoinPath("./" /*test src dir*/,
                                      kTestDataDirectory, image_name));
}

// Checks that the two provided `DetectionResult` protos are equal, with a
// tolerancy on floating-point scores to account for numerical instabilities.
// If the proto definition changes, please also change this function.
void ExpectApproximatelyEqual(const DetectionResult& actual,
                              const DetectionResult& expected,
                              const float score_precision = 1e-6,
                              int bounding_box_tolerance = 3) {
  EXPECT_EQ(actual.detections_size(), expected.detections_size());
  for (int i = 0; i < actual.detections_size(); ++i) {
    const Detection& a = actual.detections(i);
    const Detection& b = expected.detections(i);
    EXPECT_NEAR(a.bounding_box().origin_x(), b.bounding_box().origin_x(),
                bounding_box_tolerance);
    EXPECT_NEAR(a.bounding_box().origin_y(), b.bounding_box().origin_y(),
                bounding_box_tolerance);
    EXPECT_NEAR(a.bounding_box().width(), b.bounding_box().width(),
                bounding_box_tolerance);
    EXPECT_NEAR(a.bounding_box().height(), b.bounding_box().height(),
                bounding_box_tolerance);
    EXPECT_EQ(a.classes_size(), 1);
    EXPECT_EQ(b.classes_size(), 1);
    EXPECT_EQ(a.classes(0).index(), b.classes(0).index());
    EXPECT_EQ(a.classes(0).class_name(), b.classes(0).class_name());
    EXPECT_NEAR(a.classes(0).score(), b.classes(0).score(), score_precision);
  }
}

// OpResolver including the custom Detection_PostProcess op.
class MobileSsdQuantizedOpResolver : public ::tflite::MutableOpResolver {
 public:
  MobileSsdQuantizedOpResolver() {
    AddBuiltin(::tflite::BuiltinOperator_CONCATENATION,
               ::tflite::ops::builtin::Register_CONCATENATION());
    AddBuiltin(::tflite::BuiltinOperator_CONV_2D,
               ::tflite::ops::builtin::Register_CONV_2D());
    AddBuiltin(::tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
               ::tflite::ops::builtin::Register_DEPTHWISE_CONV_2D());
    AddBuiltin(::tflite::BuiltinOperator_RESHAPE,
               ::tflite::ops::builtin::Register_RESHAPE());
    AddBuiltin(::tflite::BuiltinOperator_LOGISTIC,
               ::tflite::ops::builtin::Register_LOGISTIC());
    AddBuiltin(::tflite::BuiltinOperator_ADD,
               ::tflite::ops::builtin::Register_ADD());
    AddCustom("TFLite_Detection_PostProcess",
              tflite::ops::custom::Register_DETECTION_POSTPROCESS());
  }

  MobileSsdQuantizedOpResolver(const MobileSsdQuantizedOpResolver& r) = delete;
};

class CreateFromOptionsTest : public tflite::testing::Test {};

TEST_F(CreateFromOptionsTest, SucceedsWithSelectiveOpResolver) {
  ObjectDetectorOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileSsdWithMetadata));

  SUPPORT_ASSERT_OK(ObjectDetector::CreateFromOptions(
      options, absl::make_unique<MobileSsdQuantizedOpResolver>()));
}

// OpResolver missing the Detection_PostProcess op.
class MobileSsdQuantizedOpResolverMissingOps
    : public ::tflite::MutableOpResolver {
 public:
  MobileSsdQuantizedOpResolverMissingOps() {
    AddBuiltin(::tflite::BuiltinOperator_CONCATENATION,
               ::tflite::ops::builtin::Register_CONCATENATION());
    AddBuiltin(::tflite::BuiltinOperator_CONV_2D,
               ::tflite::ops::builtin::Register_CONV_2D());
    AddBuiltin(::tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
               ::tflite::ops::builtin::Register_DEPTHWISE_CONV_2D());
    AddBuiltin(::tflite::BuiltinOperator_RESHAPE,
               ::tflite::ops::builtin::Register_RESHAPE());
    AddBuiltin(::tflite::BuiltinOperator_LOGISTIC,
               ::tflite::ops::builtin::Register_LOGISTIC());
    AddBuiltin(::tflite::BuiltinOperator_ADD,
               ::tflite::ops::builtin::Register_ADD());
  }

  MobileSsdQuantizedOpResolverMissingOps(
      const MobileSsdQuantizedOpResolverMissingOps& r) = delete;
};

TEST_F(CreateFromOptionsTest, FailsWithSelectiveOpResolverMissingOps) {
  ObjectDetectorOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileSsdWithMetadata));

  auto object_detector_or = ObjectDetector::CreateFromOptions(
      options, absl::make_unique<MobileSsdQuantizedOpResolverMissingOps>());
  EXPECT_EQ(object_detector_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(object_detector_or.status().message(),
              HasSubstr("Encountered unresolved custom op"));
  EXPECT_THAT(object_detector_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kUnsupportedCustomOp))));
}

TEST_F(CreateFromOptionsTest, FailsWithTwoModelSources) {
  ObjectDetectorOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileSsdWithMetadata));
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileSsdWithMetadata));

  StatusOr<std::unique_ptr<ObjectDetector>> object_detector_or =
      ObjectDetector::CreateFromOptions(options);

  EXPECT_EQ(object_detector_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(object_detector_or.status().message(),
              HasSubstr("Expected exactly one of `base_options.model_file` or "
                        "`model_file_with_metadata` to be provided, found 2."));
  EXPECT_THAT(object_detector_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST_F(CreateFromOptionsTest, FailsWithMissingModel) {
  ObjectDetectorOptions options;

  StatusOr<std::unique_ptr<ObjectDetector>> object_detector_or =
      ObjectDetector::CreateFromOptions(options);

  EXPECT_EQ(object_detector_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(object_detector_or.status().message(),
              HasSubstr("Expected exactly one of `base_options.model_file` or "
                        "`model_file_with_metadata` to be provided, found 0."));
  EXPECT_THAT(object_detector_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST_F(CreateFromOptionsTest, FailsWithInvalidMaxResults) {
  ObjectDetectorOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileSsdWithMetadata));
  options.set_max_results(0);

  StatusOr<std::unique_ptr<ObjectDetector>> object_detector_or =
      ObjectDetector::CreateFromOptions(options);

  EXPECT_EQ(object_detector_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(object_detector_or.status().message(),
              HasSubstr("Invalid `max_results` option"));
  EXPECT_THAT(object_detector_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST_F(CreateFromOptionsTest, FailsWithCombinedWhitelistAndBlacklist) {
  ObjectDetectorOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileSsdWithMetadata));
  options.add_class_name_whitelist("foo");
  options.add_class_name_blacklist("bar");

  StatusOr<std::unique_ptr<ObjectDetector>> object_detector_or =
      ObjectDetector::CreateFromOptions(options);

  EXPECT_EQ(object_detector_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(object_detector_or.status().message(),
              HasSubstr("mutually exclusive options"));
  EXPECT_THAT(object_detector_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST_F(CreateFromOptionsTest, SucceedsWithNumberOfThreads) {
  ObjectDetectorOptions options;
  options.set_num_threads(4);
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileSsdWithMetadata));

  SUPPORT_ASSERT_OK(ObjectDetector::CreateFromOptions(options));
}

using NumThreadsTest = ::testing::TestWithParam<int>;

INSTANTIATE_TEST_SUITE_P(Default, NumThreadsTest, ::testing::Values(0, -2));

TEST_P(NumThreadsTest, FailsWithInvalidNumberOfThreads) {
  ObjectDetectorOptions options;
  options.set_num_threads(GetParam());
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileSsdWithMetadata));

  StatusOr<std::unique_ptr<ObjectDetector>> object_detector_or =
      ObjectDetector::CreateFromOptions(options);

  EXPECT_EQ(object_detector_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(object_detector_or.status().message(),
              HasSubstr("`num_threads` must be greater than "
                        "0 or equal to -1"));
  EXPECT_THAT(object_detector_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

class DetectTest : public tflite::testing::Test {};

TEST_F(DetectTest, Succeeds) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData rgb_image, LoadImage("cats_and_dogs.jpg"));
  std::unique_ptr<FrameBuffer> frame_buffer = CreateFromRgbRawBuffer(
      rgb_image.pixel_data,
      FrameBuffer::Dimension{rgb_image.width, rgb_image.height});

  ObjectDetectorOptions options;
  options.set_max_results(4);
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileSsdWithMetadata));

  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ObjectDetector> object_detector,
                       ObjectDetector::CreateFromOptions(options));

  SUPPORT_ASSERT_OK_AND_ASSIGN(const DetectionResult result,
                       object_detector->Detect(*frame_buffer));
  ImageDataFree(&rgb_image);
  ExpectApproximatelyEqual(result,
                           ParseTextProtoOrDie<DetectionResult>(kExpectResults),
                           /*score_precision=*/0.05f);
}

TEST_F(DetectTest, SucceedswithBaseOptions) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData rgb_image, LoadImage("cats_and_dogs.jpg"));
  std::unique_ptr<FrameBuffer> frame_buffer = CreateFromRgbRawBuffer(
      rgb_image.pixel_data,
      FrameBuffer::Dimension{rgb_image.width, rgb_image.height});

  ObjectDetectorOptions options;
  options.set_max_results(4);
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileSsdWithMetadata));

  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ObjectDetector> object_detector,
                       ObjectDetector::CreateFromOptions(options));

  SUPPORT_ASSERT_OK_AND_ASSIGN(const DetectionResult result,
                       object_detector->Detect(*frame_buffer));
  ImageDataFree(&rgb_image);
  ExpectApproximatelyEqual(result,
                           ParseTextProtoOrDie<DetectionResult>(kExpectResults),
                           /*score_precision=*/0.05f);
}

TEST_F(DetectTest, SucceedswithScoreCalibrations) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData rgb_image, LoadImage("cats_and_dogs.jpg"));
  std::unique_ptr<FrameBuffer> frame_buffer = CreateFromRgbRawBuffer(
      rgb_image.pixel_data,
      FrameBuffer::Dimension{rgb_image.width, rgb_image.height});

  ObjectDetectorOptions options;
  options.set_max_results(4);
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileSsdWithMetadataDummyScoreCalibration));

  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ObjectDetector> object_detector,
                       ObjectDetector::CreateFromOptions(options));

  SUPPORT_ASSERT_OK_AND_ASSIGN(const DetectionResult result,
                       object_detector->Detect(*frame_buffer));
  ImageDataFree(&rgb_image);
  ExpectApproximatelyEqual(result,
                           ParseTextProtoOrDie<DetectionResult>(kExpectResults),
                           /*score_precision=*/0.05f);
}

class PostprocessTest : public tflite::testing::Test {
 public:
  class TestObjectDetector : public ObjectDetector {
   public:
    using ObjectDetector::ObjectDetector;
    using ObjectDetector::Postprocess;

    static StatusOr<std::unique_ptr<TestObjectDetector>> CreateFromOptions(
        const ObjectDetectorOptions& options) {
      TFLITE_RETURN_IF_ERROR(SanityCheckOptions(options));

      auto options_copy = absl::make_unique<ObjectDetectorOptions>(options);

      TFLITE_ASSIGN_OR_RETURN(
          auto object_detector,
          TaskAPIFactory::CreateFromExternalFileProto<TestObjectDetector>(
              &options_copy->model_file_with_metadata()));

      TFLITE_RETURN_IF_ERROR(object_detector->Init(std::move(options_copy)));

      return object_detector;
    }

    std::vector<TfLiteTensor*> GetOutputTensors() {
      std::vector<TfLiteTensor*> outputs;
      int num_outputs =
          TfLiteEngine::OutputCount(GetTfLiteEngine()->interpreter());
      outputs.reserve(num_outputs);
      for (int i = 0; i < num_outputs; i++) {
        outputs.push_back(
            TfLiteEngine::GetOutput(GetTfLiteEngine()->interpreter(), i));
      }
      return outputs;
    }
  };

 protected:
  void SetUp() override { tflite::testing::Test::SetUp(); }
  void SetUp(const ObjectDetectorOptions& options) {
    StatusOr<std::unique_ptr<TestObjectDetector>> test_object_detector_or =
        TestObjectDetector::CreateFromOptions(options);

    init_status_ = test_object_detector_or.status();

    if (init_status_.ok()) {
      test_object_detector_ = std::move(test_object_detector_or).value();
    }

    dummy_frame_buffer_ = CreateFromRgbRawBuffer(/*input=*/nullptr,
                                                 {/*width=*/20, /*height=*/10});
  }

  StatusOr<std::vector<const TfLiteTensor*>> FillAndGetOutputTensors() {
    std::vector<TfLiteTensor*> output_tensors =
        test_object_detector_->GetOutputTensors();
    if (output_tensors.size() != 4) {
      return absl::InternalError(absl::StrFormat(
          "Expected 4 output tensors, found %d", output_tensors.size()));
    }

    std::vector<const TfLiteTensor*> result;

    TfLiteTensor* locations = output_tensors[0];
    std::vector<float> locations_data = {
        /*left=*/0.2, /*top=*/0.2, /*right=*/0.4, /*bottom=*/0.6,
        /*left=*/0.4, /*top=*/0.2, /*right=*/0.6, /*bottom=*/0.6,
        /*left=*/0.2, /*top=*/0.4, /*right=*/0.4, /*bottom=*/0.8};
    // Pad with zeros to fill the 10 locations.
    locations_data.resize(4 * 10);
    TFLITE_RETURN_IF_ERROR(PopulateTensor(locations_data, locations));
    result.push_back(locations);

    TfLiteTensor* classes = output_tensors[1];
    std::vector<float> classes_data = {/*bicycle*/ 1, /*car*/ 2,
                                       /*motorcycle*/ 3};
    // Pad with zeros to fill the 10 classes.
    classes_data.resize(10);
    TFLITE_RETURN_IF_ERROR(PopulateTensor(classes_data, classes));
    result.push_back(classes);

    TfLiteTensor* scores = output_tensors[2];
    std::vector<float> scores_data = {0.8, 0.6, 0.4};
    // Pad with zeros to fill the 10 scores.
    scores_data.resize(10);
    TFLITE_RETURN_IF_ERROR(PopulateTensor(scores_data, scores));
    result.push_back(scores);

    TfLiteTensor* num_results = output_tensors[3];
    std::vector<float> num_results_data = {10};
    TFLITE_RETURN_IF_ERROR(PopulateTensor(num_results_data, num_results));
    result.push_back(num_results);

    return result;
  }

  std::unique_ptr<TestObjectDetector> test_object_detector_;
  std::unique_ptr<FrameBuffer> dummy_frame_buffer_;
  absl::Status init_status_;
};

TEST_F(PostprocessTest, SucceedsWithScoreThresholdOption) {
  ObjectDetectorOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileSsdWithMetadata));
  options.set_score_threshold(0.5);

  SetUp(options);
  ASSERT_TRUE(test_object_detector_ != nullptr) << init_status_;

  SUPPORT_ASSERT_OK_AND_ASSIGN(const std::vector<const TfLiteTensor*> output_tensors,
                       FillAndGetOutputTensors());

  SUPPORT_ASSERT_OK_AND_ASSIGN(DetectionResult result,
                       test_object_detector_->Postprocess(
                           output_tensors, *dummy_frame_buffer_, /*roi=*/{}));

  ExpectApproximatelyEqual(
      result,
      ParseTextProtoOrDie<DetectionResult>(
          R"pb(detections {
                 bounding_box { origin_x: 4 origin_y: 2 width: 8 height: 2 }
                 classes { index: 1 score: 0.8 class_name: "bicycle" }
               }
               detections {
                 bounding_box { origin_x: 4 origin_y: 4 width: 8 height: 2 }
                 classes { index: 2 score: 0.6 class_name: "car" }
               }
          )pb"));
}

TEST_F(PostprocessTest, SucceedsWithFrameBufferOrientation) {
  std::unique_ptr<FrameBuffer> frame_buffer_with_orientation =
      CreateFromRgbRawBuffer(/*input=*/nullptr, {/*width=*/20, /*height=*/10},
                             FrameBuffer::Orientation::kBottomRight);

  ObjectDetectorOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileSsdWithMetadata));
  options.set_score_threshold(0.5);

  SetUp(options);
  ASSERT_TRUE(test_object_detector_ != nullptr) << init_status_;

  SUPPORT_ASSERT_OK_AND_ASSIGN(const std::vector<const TfLiteTensor*> output_tensors,
                       FillAndGetOutputTensors());

  SUPPORT_ASSERT_OK_AND_ASSIGN(
      DetectionResult result,
      test_object_detector_->Postprocess(
          output_tensors, *frame_buffer_with_orientation, /*roi=*/{}));

  ExpectApproximatelyEqual(
      result,
      ParseTextProtoOrDie<DetectionResult>(
          R"pb(detections {
                 bounding_box { origin_x: 8 origin_y: 6 width: 8 height: 2 }
                 classes { index: 1 score: 0.8 class_name: "bicycle" }
               }
               detections {
                 bounding_box { origin_x: 8 origin_y: 4 width: 8 height: 2 }
                 classes { index: 2 score: 0.6 class_name: "car" }
               }
          )pb"));
}

TEST_F(PostprocessTest, SucceedsWithMaxResultsOption) {
  ObjectDetectorOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileSsdWithMetadata));
  options.set_max_results(1);

  SetUp(options);
  ASSERT_TRUE(test_object_detector_ != nullptr) << init_status_;

  SUPPORT_ASSERT_OK_AND_ASSIGN(const std::vector<const TfLiteTensor*> output_tensors,
                       FillAndGetOutputTensors());

  SUPPORT_ASSERT_OK_AND_ASSIGN(DetectionResult result,
                       test_object_detector_->Postprocess(
                           output_tensors, *dummy_frame_buffer_, /*roi=*/{}));

  ExpectApproximatelyEqual(
      result,
      ParseTextProtoOrDie<DetectionResult>(
          R"pb(detections {
                 bounding_box { origin_x: 4 origin_y: 2 width: 8 height: 2 }
                 classes { index: 1 score: 0.8 class_name: "bicycle" }
               }
          )pb"));
}

TEST_F(PostprocessTest, SucceedsWithWhitelistOption) {
  ObjectDetectorOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileSsdWithMetadata));
  options.add_class_name_whitelist("car");
  options.add_class_name_whitelist("motorcycle");

  SetUp(options);
  ASSERT_TRUE(test_object_detector_ != nullptr) << init_status_;

  SUPPORT_ASSERT_OK_AND_ASSIGN(const std::vector<const TfLiteTensor*> output_tensors,
                       FillAndGetOutputTensors());

  SUPPORT_ASSERT_OK_AND_ASSIGN(DetectionResult result,
                       test_object_detector_->Postprocess(
                           output_tensors, *dummy_frame_buffer_, /*roi=*/{}));

  ExpectApproximatelyEqual(
      result,
      ParseTextProtoOrDie<DetectionResult>(
          R"pb(detections {
                 bounding_box { origin_x: 4 origin_y: 4 width: 8 height: 2 }
                 classes { index: 2 score: 0.6 class_name: "car" }
               }
               detections {
                 bounding_box { origin_x: 8 origin_y: 2 width: 8 height: 2 }
                 classes { index: 3 score: 0.4 class_name: "motorcycle" }
               }
          )pb"));
}

TEST_F(PostprocessTest, SucceedsWithBlacklistOption) {
  ObjectDetectorOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileSsdWithMetadata));
  options.add_class_name_blacklist("car");
  // Setting score threshold to discard the 7 padded-with-zeros results.
  options.set_score_threshold(0.1);

  SetUp(options);
  ASSERT_TRUE(test_object_detector_ != nullptr) << init_status_;

  SUPPORT_ASSERT_OK_AND_ASSIGN(const std::vector<const TfLiteTensor*> output_tensors,
                       FillAndGetOutputTensors());

  SUPPORT_ASSERT_OK_AND_ASSIGN(DetectionResult result,
                       test_object_detector_->Postprocess(
                           output_tensors, *dummy_frame_buffer_, /*roi=*/{}));

  ExpectApproximatelyEqual(
      result,
      ParseTextProtoOrDie<DetectionResult>(
          R"pb(detections {
                 bounding_box { origin_x: 4 origin_y: 2 width: 8 height: 2 }
                 classes { index: 1 score: 0.8 class_name: "bicycle" }
               }
               detections {
                 bounding_box { origin_x: 8 origin_y: 2 width: 8 height: 2 }
                 classes { index: 3 score: 0.4 class_name: "motorcycle" }
               }
          )pb"));
}

}  // namespace
}  // namespace vision
}  // namespace task
}  // namespace tflite
