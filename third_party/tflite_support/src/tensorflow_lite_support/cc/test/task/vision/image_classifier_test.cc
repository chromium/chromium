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

#include "tensorflow_lite_support/cc/task/vision/image_classifier.h"

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
#include "tensorflow_lite_support/cc/task/vision/proto/classifications_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/image_classifier_options_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/image_utils.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"

namespace tflite {
namespace task {
namespace vision {
namespace {

using ::testing::ElementsAreArray;
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
// Float model.
constexpr char kMobileNetFloatWithMetadata[] = "mobilenet_v2_1.0_224.tflite";
// Quantized model.
constexpr char kMobileNetQuantizedWithMetadata[] =
    "mobilenet_v1_0.25_224_quant.tflite";
// Hello world flowers classifier supporting 5 classes (quantized model).
constexpr char kAutoMLModelWithMetadata[] = "automl_labeler_model.tflite";

StatusOr<ImageData> LoadImage(std::string image_name) {
  return DecodeImageFromFile(JoinPath("./" /*test src dir*/,
                                      kTestDataDirectory, image_name));
}

// If the proto definition changes, please also change this function.
void ExpectApproximatelyEqual(const ClassificationResult& actual,
                              const ClassificationResult& expected,
                              const float precision = 1e-6) {
  EXPECT_EQ(actual.classifications_size(), expected.classifications_size());
  for (int i = 0; i < actual.classifications_size(); ++i) {
    const Classifications& a = actual.classifications(i);
    const Classifications& b = expected.classifications(i);
    EXPECT_EQ(a.head_index(), b.head_index());
    EXPECT_EQ(a.classes_size(), b.classes_size());
    for (int j = 0; j < a.classes_size(); ++j) {
      EXPECT_EQ(a.classes(j).index(), b.classes(j).index());
      EXPECT_EQ(a.classes(j).class_name(), b.classes(j).class_name());
      EXPECT_EQ(a.classes(j).display_name(), b.classes(j).display_name());
      EXPECT_NEAR(a.classes(j).score(), b.classes(j).score(), precision);
    }
  }
}

class MobileNetQuantizedOpResolver : public ::tflite::MutableOpResolver {
 public:
  MobileNetQuantizedOpResolver() {
    AddBuiltin(::tflite::BuiltinOperator_AVERAGE_POOL_2D,
               ::tflite::ops::builtin::Register_AVERAGE_POOL_2D());
    AddBuiltin(::tflite::BuiltinOperator_CONV_2D,
               ::tflite::ops::builtin::Register_CONV_2D());
    AddBuiltin(::tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
               ::tflite::ops::builtin::Register_DEPTHWISE_CONV_2D());
    AddBuiltin(::tflite::BuiltinOperator_RESHAPE,
               ::tflite::ops::builtin::Register_RESHAPE());
    AddBuiltin(::tflite::BuiltinOperator_SOFTMAX,
               ::tflite::ops::builtin::Register_SOFTMAX());
  }

  MobileNetQuantizedOpResolver(const MobileNetQuantizedOpResolver& r) = delete;
};

class CreateFromOptionsTest : public tflite::testing::Test {};

TEST_F(CreateFromOptionsTest, SucceedsWithSelectiveOpResolver) {
  ImageClassifierOptions options;
  options.set_max_results(3);
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetQuantizedWithMetadata));

  SUPPORT_ASSERT_OK(ImageClassifier::CreateFromOptions(
      options, absl::make_unique<MobileNetQuantizedOpResolver>()));
}

class MobileNetQuantizedOpResolverMissingOps
    : public ::tflite::MutableOpResolver {
 public:
  MobileNetQuantizedOpResolverMissingOps() {
    AddBuiltin(::tflite::BuiltinOperator_SOFTMAX,
               ::tflite::ops::builtin::Register_SOFTMAX());
  }

  MobileNetQuantizedOpResolverMissingOps(
      const MobileNetQuantizedOpResolverMissingOps& r) = delete;
};

TEST_F(CreateFromOptionsTest, FailsWithSelectiveOpResolverMissingOps) {
  ImageClassifierOptions options;
  options.set_max_results(3);
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetQuantizedWithMetadata));

  auto image_classifier_or = ImageClassifier::CreateFromOptions(
      options, absl::make_unique<MobileNetQuantizedOpResolverMissingOps>());
  EXPECT_EQ(image_classifier_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(image_classifier_or.status().message(),
              HasSubstr("Didn't find op for builtin opcode"));
  EXPECT_THAT(image_classifier_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kUnsupportedBuiltinOp))));
}

TEST_F(CreateFromOptionsTest, FailsWithTwoModelSources) {
  ImageClassifierOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetQuantizedWithMetadata));
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetFloatWithMetadata));

  StatusOr<std::unique_ptr<ImageClassifier>> image_classifier_or =
      ImageClassifier::CreateFromOptions(options);

  EXPECT_EQ(image_classifier_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(image_classifier_or.status().message(),
              HasSubstr("Expected exactly one of `base_options.model_file` or "
                        "`model_file_with_metadata` to be provided, found 2."));
  EXPECT_THAT(image_classifier_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST_F(CreateFromOptionsTest, FailsWithMissingModel) {
  ImageClassifierOptions options;

  StatusOr<std::unique_ptr<ImageClassifier>> image_classifier_or =
      ImageClassifier::CreateFromOptions(options);

  EXPECT_EQ(image_classifier_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(image_classifier_or.status().message(),
              HasSubstr("Expected exactly one of `base_options.model_file` or "
                        "`model_file_with_metadata` to be provided, found 0."));
  EXPECT_THAT(image_classifier_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST_F(CreateFromOptionsTest, FailsWithInvalidMaxResults) {
  ImageClassifierOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetQuantizedWithMetadata));
  options.set_max_results(0);

  StatusOr<std::unique_ptr<ImageClassifier>> image_classifier_or =
      ImageClassifier::CreateFromOptions(options);

  EXPECT_EQ(image_classifier_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(image_classifier_or.status().message(),
              HasSubstr("Invalid `max_results` option"));
  EXPECT_THAT(image_classifier_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST_F(CreateFromOptionsTest, FailsWithCombinedWhitelistAndBlacklist) {
  ImageClassifierOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetQuantizedWithMetadata));
  options.add_class_name_whitelist("foo");
  options.add_class_name_blacklist("bar");

  StatusOr<std::unique_ptr<ImageClassifier>> image_classifier_or =
      ImageClassifier::CreateFromOptions(options);

  EXPECT_EQ(image_classifier_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(image_classifier_or.status().message(),
              HasSubstr("mutually exclusive options"));
  EXPECT_THAT(image_classifier_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST_F(CreateFromOptionsTest, SucceedsWithNumberOfThreads) {
  ImageClassifierOptions options;
  options.set_num_threads(4);
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetFloatWithMetadata));

  SUPPORT_ASSERT_OK(ImageClassifier::CreateFromOptions(options));
}

using NumThreadsTest = ::testing::TestWithParam<int>;

INSTANTIATE_TEST_SUITE_P(Default, NumThreadsTest, ::testing::Values(0, -2));

TEST_P(NumThreadsTest, FailsWithInvalidNumberOfThreads) {
  ImageClassifierOptions options;
  options.set_num_threads(GetParam());
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetFloatWithMetadata));

  StatusOr<std::unique_ptr<ImageClassifier>> image_classifier_or =
      ImageClassifier::CreateFromOptions(options);

  EXPECT_EQ(image_classifier_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(image_classifier_or.status().message(),
              HasSubstr("`num_threads` must be greater than "
                        "0 or equal to -1"));
  EXPECT_THAT(image_classifier_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST(ClassifyTest, SucceedsWithFloatModel) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData rgb_image, LoadImage("burger.jpg"));
  std::unique_ptr<FrameBuffer> frame_buffer = CreateFromRgbRawBuffer(
      rgb_image.pixel_data,
      FrameBuffer::Dimension{rgb_image.width, rgb_image.height});

  ImageClassifierOptions options;
  options.set_max_results(3);
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetFloatWithMetadata));

  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageClassifier> image_classifier,
                       ImageClassifier::CreateFromOptions(options));

  StatusOr<ClassificationResult> result_or =
      image_classifier->Classify(*frame_buffer);
  ImageDataFree(&rgb_image);
  SUPPORT_ASSERT_OK(result_or);

  const ClassificationResult& result = result_or.value();
  ExpectApproximatelyEqual(
      result,
      ParseTextProtoOrDie<ClassificationResult>(
          R"pb(classifications {
                 classes {
                   index: 934
                   score: 0.7399742
                   class_name: "cheeseburger"
                 }
                 classes {
                   index: 925
                   score: 0.026928535
                   class_name: "guacamole"
                 }
                 classes { index: 932 score: 0.025737215 class_name: "bagel" }
                 head_index: 0
               }
          )pb"));
}

TEST(ClassifyTest, SucceedsWithRegionOfInterest) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData rgb_image, LoadImage("multi_objects.jpg"));
  std::unique_ptr<FrameBuffer> frame_buffer = CreateFromRgbRawBuffer(
      rgb_image.pixel_data,
      FrameBuffer::Dimension{rgb_image.width, rgb_image.height});

  ImageClassifierOptions options;
  options.set_max_results(1);
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetFloatWithMetadata));

  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageClassifier> image_classifier,
                       ImageClassifier::CreateFromOptions(options));

  // Crop around the soccer ball.
  BoundingBox roi;
  roi.set_origin_x(406);
  roi.set_origin_y(110);
  roi.set_width(148);
  roi.set_height(153);

  StatusOr<ClassificationResult> result_or =
      image_classifier->Classify(*frame_buffer, roi);
  ImageDataFree(&rgb_image);
  SUPPORT_ASSERT_OK(result_or);

  const ClassificationResult& result = result_or.value();
  ExpectApproximatelyEqual(result, ParseTextProtoOrDie<ClassificationResult>(
                                       R"pb(classifications {
                                              classes {
                                                index: 806
                                                score: 0.99673367
                                                class_name: "soccer ball"
                                              }
                                              head_index: 0
                                            }
                                       )pb"));
}

TEST(ClassifyTest, SucceedsWithQuantizedModel) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData rgb_image, LoadImage("burger.jpg"));
  std::unique_ptr<FrameBuffer> frame_buffer = CreateFromRgbRawBuffer(
      rgb_image.pixel_data,
      FrameBuffer::Dimension{rgb_image.width, rgb_image.height});

  ImageClassifierOptions options;
  options.set_max_results(1);
  options.set_score_threshold(0.5);
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetQuantizedWithMetadata));

  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageClassifier> image_classifier,
                       ImageClassifier::CreateFromOptions(options));

  StatusOr<ClassificationResult> result_or =
      image_classifier->Classify(*frame_buffer);
  ImageDataFree(&rgb_image);
  SUPPORT_ASSERT_OK(result_or);

  const ClassificationResult& result = result_or.value();
  ExpectApproximatelyEqual(
      result,
      ParseTextProtoOrDie<ClassificationResult>(
          R"pb(classifications {
                 classes { index: 934 score: 0.96 class_name: "cheeseburger" }
                 head_index: 0
               }
          )pb"),
      0.01);
}

TEST(ClassifyTest, SucceedsWithBaseOptions) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData rgb_image, LoadImage("burger.jpg"));
  std::unique_ptr<FrameBuffer> frame_buffer = CreateFromRgbRawBuffer(
      rgb_image.pixel_data,
      FrameBuffer::Dimension{rgb_image.width, rgb_image.height});

  ImageClassifierOptions options;
  options.set_max_results(3);
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetFloatWithMetadata));

  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageClassifier> image_classifier,
                       ImageClassifier::CreateFromOptions(options));

  StatusOr<ClassificationResult> result_or =
      image_classifier->Classify(*frame_buffer);
  ImageDataFree(&rgb_image);
  SUPPORT_ASSERT_OK(result_or);

  const ClassificationResult& result = result_or.value();
  ExpectApproximatelyEqual(
      result,
      ParseTextProtoOrDie<ClassificationResult>(
          R"pb(classifications {
                 classes {
                   index: 934
                   score: 0.7399742
                   class_name: "cheeseburger"
                 }
                 classes {
                   index: 925
                   score: 0.026928535
                   class_name: "guacamole"
                 }
                 classes { index: 932 score: 0.025737215 class_name: "bagel" }
                 head_index: 0
               }
          )pb"));
}

// Configure the ImageClassifier to use the mini-benchmark to decide if we
// should use the XNNPack Delegate with the given number of threads.
void ConfigureXnnPackMiniBenchmark(int num_threads,
                                   ImageClassifierOptions& options) {
  auto* mutable_mini_benchmark_settings =
      options.mutable_base_options()
          ->mutable_compute_settings()
          ->mutable_settings_to_test_locally();
  auto* setting_to_benchmark =
      mutable_mini_benchmark_settings->add_settings_to_test();
  setting_to_benchmark->set_delegate(tflite::proto::Delegate::XNNPACK);
  setting_to_benchmark->mutable_xnnpack_settings()->set_num_threads(
      num_threads);

  // Configuring mini-benchmark storage paths
  mutable_mini_benchmark_settings->mutable_storage_paths()
      ->set_storage_file_path(
          JoinPath(::testing::TempDir(), "mini_benchmark_storage"));
  mutable_mini_benchmark_settings->mutable_storage_paths()
      ->set_data_directory_path(::testing::TempDir());
}

TEST(ClassifyTest, SucceedsWithMiniBenchmark) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData rgb_image, LoadImage("burger.jpg"));
  std::unique_ptr<FrameBuffer> frame_buffer = CreateFromRgbRawBuffer(
      rgb_image.pixel_data,
      FrameBuffer::Dimension{rgb_image.width, rgb_image.height});

  auto file_name = JoinPath("./" /*test src dir*/,
                            kTestDataDirectory, kMobileNetFloatWithMetadata);

  ImageClassifierOptions options;
  options.set_max_results(3);
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      file_name);

  ConfigureXnnPackMiniBenchmark(/*num_threads=*/2, options);

  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageClassifier> image_classifier,
                       ImageClassifier::CreateFromOptions(options));

  StatusOr<ClassificationResult> result_or =
      image_classifier->Classify(*frame_buffer);
  ImageDataFree(&rgb_image);
  SUPPORT_ASSERT_OK(result_or);

  const ClassificationResult& result = result_or.value();
  ExpectApproximatelyEqual(
      result,
      ParseTextProtoOrDie<ClassificationResult>(
          R"pb(classifications {
                 classes {
                   index: 934
                   score: 0.7399742
                   class_name: "cheeseburger"
                 }
                 classes {
                   index: 925
                   score: 0.026928535
                   class_name: "guacamole"
                 }
                 classes { index: 932 score: 0.025737215 class_name: "bagel" }
                 head_index: 0
               }
          )pb"));
}

TEST(ClassifyTest, GetInputCountSucceeds) {
  ImageClassifierOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetFloatWithMetadata));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageClassifier> image_classifier,
                       ImageClassifier::CreateFromOptions(options));

  int32_t input_count = image_classifier->GetInputCount();
  EXPECT_THAT(input_count, 1);
}

TEST(ClassifyTest, GetInputShapeSucceeds) {
  ImageClassifierOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetFloatWithMetadata));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageClassifier> image_classifier,
                       ImageClassifier::CreateFromOptions(options));

  // Verify the shape array size.
  const TfLiteIntArray* input_shape_0 = image_classifier->GetInputShape(0);
  EXPECT_THAT(input_shape_0->size, 4);

  // Verify the shape array data.
  auto shape_data = input_shape_0->data;
  std::vector<int> shape_vector(shape_data, shape_data + input_shape_0->size);
  EXPECT_THAT(shape_vector, ElementsAreArray({1, 224, 224, 3}));
}

TEST(ClassifyTest, GetOutputCountSucceeds) {
  ImageClassifierOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetFloatWithMetadata));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageClassifier> image_classifier,
                       ImageClassifier::CreateFromOptions(options));

  int32_t output_count = image_classifier->GetOutputCount();
  EXPECT_THAT(output_count, 1);
}

TEST(ClassifyTest, GetOutputShapeSucceeds) {
  ImageClassifierOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetFloatWithMetadata));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageClassifier> image_classifier,
                       ImageClassifier::CreateFromOptions(options));

  // Verify the shape array size.
  const TfLiteIntArray* output_shape_0 = image_classifier->GetOutputShape(0);
  EXPECT_THAT(output_shape_0->size, 2);

  // Verify the shape array data.
  auto shape_data = output_shape_0->data;
  std::vector<int> shape_vector(shape_data, shape_data + output_shape_0->size);
  EXPECT_THAT(shape_vector, ElementsAreArray({1, 1001}));
}

class PostprocessTest : public tflite::testing::Test {
 public:
  class TestImageClassifier : public ImageClassifier {
   public:
    using ImageClassifier::ImageClassifier;
    using ImageClassifier::Postprocess;

    static StatusOr<std::unique_ptr<TestImageClassifier>> CreateFromOptions(
        const ImageClassifierOptions& options) {
      TFLITE_RETURN_IF_ERROR(SanityCheckOptions(options));

      auto options_copy = absl::make_unique<ImageClassifierOptions>(options);

      TFLITE_ASSIGN_OR_RETURN(
          auto image_classifier,
          TaskAPIFactory::CreateFromExternalFileProto<TestImageClassifier>(
              &options_copy->model_file_with_metadata()));

      TFLITE_RETURN_IF_ERROR(image_classifier->Init(std::move(options_copy)));

      return image_classifier;
    }

    TfLiteTensor* GetOutputTensor() {
      if (TfLiteEngine::OutputCount(GetTfLiteEngine()->interpreter()) != 1) {
        return nullptr;
      }
      return TfLiteEngine::GetOutput(GetTfLiteEngine()->interpreter(), 0);
    }
  };

 protected:
  void SetUp() override { tflite::testing::Test::SetUp(); }
  void SetUp(const ImageClassifierOptions& options) {
    StatusOr<std::unique_ptr<TestImageClassifier>> test_image_classifier_or =
        TestImageClassifier::CreateFromOptions(options);

    init_status_ = test_image_classifier_or.status();

    if (init_status_.ok()) {
      test_image_classifier_ = std::move(test_image_classifier_or).value();
    }

    dummy_frame_buffer_ = CreateFromRgbRawBuffer(/*input=*/nullptr, {});
  }

  std::unique_ptr<TestImageClassifier> test_image_classifier_;
  std::unique_ptr<FrameBuffer> dummy_frame_buffer_;
  absl::Status init_status_;
};

TEST_F(PostprocessTest, SucceedsWithMaxResultsOption) {
  ImageClassifierOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kAutoMLModelWithMetadata));
  options.set_max_results(3);

  SetUp(options);
  ASSERT_TRUE(test_image_classifier_ != nullptr) << init_status_;

  TfLiteTensor* output_tensor = test_image_classifier_->GetOutputTensor();
  ASSERT_NE(output_tensor, nullptr);

  std::vector<uint8_t> scores = {/*daisy*/ 0, /*dandelion*/ 64, /*roses*/ 255,
                                 /*sunflowers*/ 32, /*tulips*/ 128};
  SUPPORT_ASSERT_OK(PopulateTensor(scores, output_tensor));
  SUPPORT_ASSERT_OK_AND_ASSIGN(ClassificationResult result,
                       test_image_classifier_->Postprocess(
                           {output_tensor}, *dummy_frame_buffer_, /*roi=*/{}));
  ExpectApproximatelyEqual(
      result,
      ParseTextProtoOrDie<ClassificationResult>(
          R"pb(classifications {
                 classes { index: 2 score: 0.99609375 class_name: "roses" }
                 classes { index: 4 score: 0.5 class_name: "tulips" }
                 classes { index: 1 score: 0.25 class_name: "dandelion" }
                 head_index: 0
               }
          )pb"));
}

TEST_F(PostprocessTest, SucceedsWithScoreThresholdOption) {
  ImageClassifierOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kAutoMLModelWithMetadata));
  options.set_score_threshold(0.4);

  SetUp(options);
  ASSERT_TRUE(test_image_classifier_ != nullptr) << init_status_;

  TfLiteTensor* output_tensor = test_image_classifier_->GetOutputTensor();
  ASSERT_NE(output_tensor, nullptr);

  std::vector<uint8_t> scores = {/*daisy*/ 0, /*dandelion*/ 64, /*roses*/ 255,
                                 /*sunflowers*/ 32, /*tulips*/ 128};
  SUPPORT_ASSERT_OK(PopulateTensor(scores, output_tensor));
  SUPPORT_ASSERT_OK_AND_ASSIGN(ClassificationResult result,
                       test_image_classifier_->Postprocess(
                           {output_tensor}, *dummy_frame_buffer_, /*roi=*/{}));

  ExpectApproximatelyEqual(
      result,
      ParseTextProtoOrDie<ClassificationResult>(
          R"pb(classifications {
                 classes { index: 2 score: 0.99609375 class_name: "roses" }
                 classes { index: 4 score: 0.5 class_name: "tulips" }
                 head_index: 0
               }
          )pb"));
}

TEST_F(PostprocessTest, SucceedsWithWhitelistOption) {
  ImageClassifierOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kAutoMLModelWithMetadata));
  options.add_class_name_whitelist("dandelion");
  options.add_class_name_whitelist("daisy");

  SetUp(options);
  ASSERT_TRUE(test_image_classifier_ != nullptr) << init_status_;

  TfLiteTensor* output_tensor = test_image_classifier_->GetOutputTensor();
  ASSERT_NE(output_tensor, nullptr);

  std::vector<uint8_t> scores = {/*daisy*/ 0, /*dandelion*/ 64, /*roses*/ 255,
                                 /*sunflowers*/ 32, /*tulips*/ 128};
  SUPPORT_ASSERT_OK(PopulateTensor(scores, output_tensor));
  SUPPORT_ASSERT_OK_AND_ASSIGN(ClassificationResult result,
                       test_image_classifier_->Postprocess(
                           {output_tensor}, *dummy_frame_buffer_, /*roi=*/{}));
  ExpectApproximatelyEqual(
      result,
      ParseTextProtoOrDie<ClassificationResult>(
          R"pb(classifications {
                 classes { index: 1 score: 0.25 class_name: "dandelion" }
                 classes { index: 0 score: 0 class_name: "daisy" }
                 head_index: 0
               }
          )pb"));
}

TEST_F(PostprocessTest, SucceedsWithBlacklistOption) {
  ImageClassifierOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kAutoMLModelWithMetadata));
  options.add_class_name_blacklist("dandelion");
  options.add_class_name_blacklist("daisy");

  SetUp(options);
  ASSERT_TRUE(test_image_classifier_ != nullptr) << init_status_;

  TfLiteTensor* output_tensor = test_image_classifier_->GetOutputTensor();
  ASSERT_NE(output_tensor, nullptr);

  std::vector<uint8_t> scores = {/*daisy*/ 0, /*dandelion*/ 64, /*roses*/ 255,
                                 /*sunflowers*/ 32, /*tulips*/ 128};
  SUPPORT_ASSERT_OK(PopulateTensor(scores, output_tensor));
  SUPPORT_ASSERT_OK_AND_ASSIGN(ClassificationResult result,
                       test_image_classifier_->Postprocess(
                           {output_tensor}, *dummy_frame_buffer_, /*roi=*/{}));

  ExpectApproximatelyEqual(
      result,
      ParseTextProtoOrDie<ClassificationResult>(
          R"pb(classifications {
                 classes { index: 2 score: 0.99609375 class_name: "roses" }
                 classes { index: 4 score: 0.5 class_name: "tulips" }
                 classes { index: 3 score: 0.125 class_name: "sunflowers" }
                 head_index: 0
               }
          )pb"));
}

}  // namespace
}  // namespace vision
}  // namespace task
}  // namespace tflite
