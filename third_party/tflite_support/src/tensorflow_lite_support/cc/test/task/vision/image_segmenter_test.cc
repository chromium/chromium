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

#include "tensorflow_lite_support/cc/task/vision/image_segmenter.h"

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
#include "tensorflow_lite_support/cc/task/vision/proto/image_segmenter_options_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/segmentations_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/image_utils.h"
#include "tensorflow_lite_support/cc/test/message_matchers.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"

namespace tflite {
namespace task {
namespace vision {
namespace {

using ::testing::HasSubstr;
using ::testing::Optional;
using ::tflite::support::EqualsProto;
using ::tflite::support::kTfLiteSupportPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::task::JoinPath;
using ::tflite::task::core::PopulateTensor;
using ::tflite::task::core::TaskAPIFactory;
using ::tflite::task::core::TfLiteEngine;

constexpr char kTestDataDirectory[] =
    "/tensorflow_lite_support/cc/test/testdata/task/"
    "vision/";
constexpr char kDeepLabV3[] = "deeplabv3.tflite";

// All results returned by DeepLabV3 are expected to contain these in addition
// to the segmentation masks.
constexpr char kDeepLabV3PartialResult[] =
    R"(width: 257
       height: 257
       colored_labels { r: 0 g: 0 b: 0 class_name: "background" }
       colored_labels { r: 128 g: 0 b: 0 class_name: "aeroplane" }
       colored_labels { r: 0 g: 128 b: 0 class_name: "bicycle" }
       colored_labels { r: 128 g: 128 b: 0 class_name: "bird" }
       colored_labels { r: 0 g: 0 b: 128 class_name: "boat" }
       colored_labels { r: 128 g: 0 b: 128 class_name: "bottle" }
       colored_labels { r: 0 g: 128 b: 128 class_name: "bus" }
       colored_labels { r: 128 g: 128 b: 128 class_name: "car" }
       colored_labels { r: 64 g: 0 b: 0 class_name: "cat" }
       colored_labels { r: 192 g: 0 b: 0 class_name: "chair" }
       colored_labels { r: 64 g: 128 b: 0 class_name: "cow" }
       colored_labels { r: 192 g: 128 b: 0 class_name: "dining table" }
       colored_labels { r: 64 g: 0 b: 128 class_name: "dog" }
       colored_labels { r: 192 g: 0 b: 128 class_name: "horse" }
       colored_labels { r: 64 g: 128 b: 128 class_name: "motorbike" }
       colored_labels { r: 192 g: 128 b: 128 class_name: "person" }
       colored_labels { r: 0 g: 64 b: 0 class_name: "potted plant" }
       colored_labels { r: 128 g: 64 b: 0 class_name: "sheep" }
       colored_labels { r: 0 g: 192 b: 0 class_name: "sofa" }
       colored_labels { r: 128 g: 192 b: 0 class_name: "train" }
       colored_labels { r: 0 g: 64 b: 128 class_name: "tv" })";

// The maximum fraction of pixels in the candidate mask that can have a
// different class than the golden mask for the test to pass.
constexpr float kGoldenMaskTolerance = 1e-2;
// Magnification factor used when creating the golden category masks to make
// them more human-friendly. Each pixel in the golden masks has its value
// multiplied by this factor, i.e. a value of 10 means class index 1, a value of
// 20 means class index 2, etc.
constexpr int kGoldenMaskMagnificationFactor = 10;

StatusOr<ImageData> LoadImage(std::string image_name) {
  return DecodeImageFromFile(JoinPath("./" /*test src dir*/,
                                      kTestDataDirectory, image_name));
}

// Checks that the two provided `Segmentation` protos are equal.
// If the proto definition changes, please also change this function.
void ExpectApproximatelyEqual(const Segmentation& actual,
                              const Segmentation& expected) {
  EXPECT_EQ(actual.height(), expected.height());
  EXPECT_EQ(actual.width(), expected.width());
  for (int i = 0; i < actual.colored_labels_size(); i++) {
    EXPECT_THAT(actual.colored_labels(i),
                EqualsProto(expected.colored_labels(i)));
  }
}

class DeepLabOpResolver : public ::tflite::MutableOpResolver {
 public:
  DeepLabOpResolver() {
    AddBuiltin(::tflite::BuiltinOperator_ADD,
               ::tflite::ops::builtin::Register_ADD());
    AddBuiltin(::tflite::BuiltinOperator_AVERAGE_POOL_2D,
               ::tflite::ops::builtin::Register_AVERAGE_POOL_2D());
    AddBuiltin(::tflite::BuiltinOperator_CONCATENATION,
               ::tflite::ops::builtin::Register_CONCATENATION());
    AddBuiltin(::tflite::BuiltinOperator_CONV_2D,
               ::tflite::ops::builtin::Register_CONV_2D());
    // DeepLab uses different versions of DEPTHWISE_CONV_2D.
    AddBuiltin(::tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
               ::tflite::ops::builtin::Register_DEPTHWISE_CONV_2D(),
               /*min_version=*/1, /*max_version=*/2);
    AddBuiltin(::tflite::BuiltinOperator_RESIZE_BILINEAR,
               ::tflite::ops::builtin::Register_RESIZE_BILINEAR());
  }

  DeepLabOpResolver(const DeepLabOpResolver& r) = delete;
};

class CreateFromOptionsTest : public tflite::testing::Test {};

TEST_F(CreateFromOptionsTest, SucceedsWithSelectiveOpResolver) {
  ImageSegmenterOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kDeepLabV3));

  SUPPORT_ASSERT_OK(ImageSegmenter::CreateFromOptions(
      options, absl::make_unique<DeepLabOpResolver>()));
}

class DeepLabOpResolverMissingOps : public ::tflite::MutableOpResolver {
 public:
  DeepLabOpResolverMissingOps() {
    AddBuiltin(::tflite::BuiltinOperator_ADD,
               ::tflite::ops::builtin::Register_ADD());
  }

  DeepLabOpResolverMissingOps(const DeepLabOpResolverMissingOps& r) = delete;
};

TEST_F(CreateFromOptionsTest, FailsWithSelectiveOpResolverMissingOps) {
  ImageSegmenterOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kDeepLabV3));

  auto image_segmenter_or = ImageSegmenter::CreateFromOptions(
      options, absl::make_unique<DeepLabOpResolverMissingOps>());

  EXPECT_EQ(image_segmenter_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(image_segmenter_or.status().message(),
              HasSubstr("Didn't find op for builtin opcode"));
  EXPECT_THAT(image_segmenter_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kUnsupportedBuiltinOp))));
}

TEST_F(CreateFromOptionsTest, FailsWithTwoModelSources) {
  ImageSegmenterOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kDeepLabV3));
  options.mutable_base_options()->mutable_model_file()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kDeepLabV3));

  StatusOr<std::unique_ptr<ImageSegmenter>> image_segmenter_or =
      ImageSegmenter::CreateFromOptions(options);

  EXPECT_EQ(image_segmenter_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(image_segmenter_or.status().message(),
              HasSubstr("Expected exactly one of `base_options.model_file` or "
                        "`model_file_with_metadata` to be provided, found 2."));
  EXPECT_THAT(image_segmenter_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST_F(CreateFromOptionsTest, FailsWithMissingModel) {
  ImageSegmenterOptions options;

  auto image_segmenter_or = ImageSegmenter::CreateFromOptions(options);

  EXPECT_EQ(image_segmenter_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(image_segmenter_or.status().message(),
              HasSubstr("Expected exactly one of `base_options.model_file` or "
                        "`model_file_with_metadata` to be provided, found 0."));
  EXPECT_THAT(image_segmenter_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST_F(CreateFromOptionsTest, FailsWithUnspecifiedOutputType) {
  ImageSegmenterOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kDeepLabV3));
  options.set_output_type(ImageSegmenterOptions::UNSPECIFIED);

  auto image_segmenter_or = ImageSegmenter::CreateFromOptions(options);

  EXPECT_EQ(image_segmenter_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(image_segmenter_or.status().message(),
              HasSubstr("`output_type` must not be UNSPECIFIED"));
  EXPECT_THAT(image_segmenter_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST_F(CreateFromOptionsTest, SucceedsWithNumberOfThreads) {
  ImageSegmenterOptions options;
  options.set_num_threads(4);
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kDeepLabV3));

  SUPPORT_ASSERT_OK(ImageSegmenter::CreateFromOptions(options));
}

using NumThreadsTest = ::testing::TestWithParam<int>;

INSTANTIATE_TEST_SUITE_P(Default, NumThreadsTest, ::testing::Values(0, -2));

TEST_P(NumThreadsTest, FailsWithInvalidNumberOfThreads) {
  ImageSegmenterOptions options;
  options.set_num_threads(GetParam());
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kDeepLabV3));

  StatusOr<std::unique_ptr<ImageSegmenter>> image_segmenter_or =
      ImageSegmenter::CreateFromOptions(options);

  EXPECT_EQ(image_segmenter_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(image_segmenter_or.status().message(),
              HasSubstr("`num_threads` must be greater than "
                        "0 or equal to -1"));
  EXPECT_THAT(image_segmenter_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

// Confidence masks tested in PostProcess unit tests below.
TEST(SegmentTest, SucceedsWithCategoryMask) {
  // Load input and build frame buffer.
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData rgb_image,
                       LoadImage("segmentation_input_rotation0.jpg"));
  std::unique_ptr<FrameBuffer> frame_buffer = CreateFromRgbRawBuffer(
      rgb_image.pixel_data,
      FrameBuffer::Dimension{rgb_image.width, rgb_image.height});
  // Load golden mask output.
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData golden_mask,
                       LoadImage("segmentation_golden_rotation0.png"));

  ImageSegmenterOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kDeepLabV3));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageSegmenter> image_segmenter,
                       ImageSegmenter::CreateFromOptions(options));
  SUPPORT_ASSERT_OK_AND_ASSIGN(const SegmentationResult result,
                       image_segmenter->Segment(*frame_buffer));

  EXPECT_EQ(result.segmentation_size(), 1);
  const Segmentation& segmentation = result.segmentation(0);
  ExpectApproximatelyEqual(
      segmentation, ParseTextProtoOrDie<Segmentation>(kDeepLabV3PartialResult));
  EXPECT_TRUE(segmentation.has_category_mask());
  const uint8* mask =
      reinterpret_cast<const uint8*>(segmentation.category_mask().data());

  int inconsistent_pixels = 0;
  int num_pixels = golden_mask.height * golden_mask.width;
  for (int i = 0; i < num_pixels; ++i) {
    inconsistent_pixels +=
        (mask[i] * kGoldenMaskMagnificationFactor != golden_mask.pixel_data[i]);
  }
  EXPECT_LT(static_cast<float>(inconsistent_pixels) / num_pixels,
            kGoldenMaskTolerance);
  ImageDataFree(&rgb_image);
  ImageDataFree(&golden_mask);
}

TEST(SegmentTest, SucceedsWithOrientation) {
  // Load input and build frame buffer with kRightBottom orientation.
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData rgb_image,
                       LoadImage("segmentation_input_rotation90_flop.jpg"));
  std::unique_ptr<FrameBuffer> frame_buffer = CreateFromRgbRawBuffer(
      rgb_image.pixel_data,
      FrameBuffer::Dimension{rgb_image.width, rgb_image.height},
      FrameBuffer::Orientation::kRightBottom);
  // Load golden mask output.
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData golden_mask,
                       LoadImage("segmentation_golden_rotation90_flop.png"));

  ImageSegmenterOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kDeepLabV3));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageSegmenter> image_segmenter,
                       ImageSegmenter::CreateFromOptions(options));
  SUPPORT_ASSERT_OK_AND_ASSIGN(const SegmentationResult result,
                       image_segmenter->Segment(*frame_buffer));

  EXPECT_EQ(result.segmentation_size(), 1);
  const Segmentation& segmentation = result.segmentation(0);
  ExpectApproximatelyEqual(
      segmentation, ParseTextProtoOrDie<Segmentation>(kDeepLabV3PartialResult));
  EXPECT_TRUE(segmentation.has_category_mask());
  const uint8* mask =
      reinterpret_cast<const uint8*>(segmentation.category_mask().data());
  int inconsistent_pixels = 0;
  int num_pixels = golden_mask.height * golden_mask.width;
  for (int i = 0; i < num_pixels; ++i) {
    inconsistent_pixels +=
        (mask[i] * kGoldenMaskMagnificationFactor != golden_mask.pixel_data[i]);
  }
  EXPECT_LT(static_cast<float>(inconsistent_pixels) / num_pixels,
            kGoldenMaskTolerance);
  ImageDataFree(&rgb_image);
  ImageDataFree(&golden_mask);
}

TEST(SegmentTest, SucceedsWithBaseOptions) {
  // Load input and build frame buffer.
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData rgb_image,
                       LoadImage("segmentation_input_rotation0.jpg"));
  std::unique_ptr<FrameBuffer> frame_buffer = CreateFromRgbRawBuffer(
      rgb_image.pixel_data,
      FrameBuffer::Dimension{rgb_image.width, rgb_image.height});
  // Load golden mask output.
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData golden_mask,
                       LoadImage("segmentation_golden_rotation0.png"));

  ImageSegmenterOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kDeepLabV3));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageSegmenter> image_segmenter,
                       ImageSegmenter::CreateFromOptions(options));
  SUPPORT_ASSERT_OK_AND_ASSIGN(const SegmentationResult result,
                       image_segmenter->Segment(*frame_buffer));

  EXPECT_EQ(result.segmentation_size(), 1);
  const Segmentation& segmentation = result.segmentation(0);
  ExpectApproximatelyEqual(
      segmentation, ParseTextProtoOrDie<Segmentation>(kDeepLabV3PartialResult));
  EXPECT_TRUE(segmentation.has_category_mask());
  const uint8* mask =
      reinterpret_cast<const uint8*>(segmentation.category_mask().data());

  int inconsistent_pixels = 0;
  int num_pixels = golden_mask.height * golden_mask.width;
  for (int i = 0; i < num_pixels; ++i) {
    inconsistent_pixels +=
        (mask[i] * kGoldenMaskMagnificationFactor != golden_mask.pixel_data[i]);
  }
  EXPECT_LT(static_cast<float>(inconsistent_pixels) / num_pixels,
            kGoldenMaskTolerance);
  ImageDataFree(&rgb_image);
  ImageDataFree(&golden_mask);
}

class PostprocessTest : public tflite::testing::Test {
 public:
  class TestImageSegmenter : public ImageSegmenter {
   public:
    using ImageSegmenter::ImageSegmenter;
    using ImageSegmenter::Postprocess;

    static StatusOr<std::unique_ptr<TestImageSegmenter>> CreateFromOptions(
        const ImageSegmenterOptions& options) {
      TFLITE_RETURN_IF_ERROR(SanityCheckOptions(options));

      auto options_copy = absl::make_unique<ImageSegmenterOptions>(options);

      TFLITE_ASSIGN_OR_RETURN(
          auto image_segmenter,
          TaskAPIFactory::CreateFromExternalFileProto<TestImageSegmenter>(
              &options_copy->model_file_with_metadata()));

      TFLITE_RETURN_IF_ERROR(image_segmenter->Init(std::move(options_copy)));

      return image_segmenter;
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
  void SetUp(const ImageSegmenterOptions& options) {
    StatusOr<std::unique_ptr<TestImageSegmenter>> test_image_segmenter_or =
        TestImageSegmenter::CreateFromOptions(options);

    init_status_ = test_image_segmenter_or.status();

    if (init_status_.ok()) {
      test_image_segmenter_ = std::move(test_image_segmenter_or).value();
    }
  }

  StatusOr<const TfLiteTensor*> FillAndGetOutputTensor() {
    TfLiteTensor* output_tensor = test_image_segmenter_->GetOutputTensor();

    // Fill top-left corner and pad all other pixels with zeros.
    std::vector<float> confidence_scores = confidence_scores_;
    confidence_scores.resize(/*width*/ 257 *
                             /*height*/ 257 *
                             /*classes*/ 21);
    TFLITE_RETURN_IF_ERROR(PopulateTensor(confidence_scores, output_tensor));

    return output_tensor;
  }

  std::unique_ptr<TestImageSegmenter> test_image_segmenter_;
  absl::Status init_status_;
  std::vector<float> confidence_scores_ = {/*background=*/0.01,
                                           /*aeroplane=*/0.01,
                                           /*bicycle=*/0.01,
                                           /*bird=*/0.01,
                                           /*boat=*/0.01,
                                           /*bottle=*/0.01,
                                           /*bus=*/0.21,
                                           /*car=*/0.60,  // highest (index=7)
                                           /*cat=*/0.01,
                                           /*chair=*/0.01,
                                           /*cow=*/0.01,
                                           /*dining table=*/0.01,
                                           /*dog=*/0.01,
                                           /*horse=*/0.01,
                                           /*motorbike=*/0.01,
                                           /*person=*/0.01,
                                           /*potted plant=*/0.01,
                                           /*sheep=*/0.01,
                                           /*sofa=*/0.01,
                                           /*train=*/0.01,
                                           /*tv=*/0.01};
};

TEST_F(PostprocessTest, SucceedsWithCategoryMask) {
  ImageSegmenterOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kDeepLabV3));
  std::unique_ptr<FrameBuffer> frame_buffer =
      CreateFromRgbaRawBuffer(/*input=*/nullptr, {});

  SetUp(options);
  ASSERT_TRUE(test_image_segmenter_ != nullptr) << init_status_;
  SUPPORT_ASSERT_OK_AND_ASSIGN(const TfLiteTensor* output_tensor,
                       FillAndGetOutputTensor());
  SUPPORT_ASSERT_OK_AND_ASSIGN(SegmentationResult result,
                       test_image_segmenter_->Postprocess(
                           {output_tensor}, *frame_buffer, /*roi=*/{}));

  EXPECT_EQ(result.segmentation_size(), 1);
  const Segmentation& segmentation = result.segmentation(0);
  ExpectApproximatelyEqual(
      segmentation, ParseTextProtoOrDie<Segmentation>(kDeepLabV3PartialResult));
  EXPECT_TRUE(segmentation.has_category_mask());
  // Check top-left corner has expected class.
  const uint8* category_mask =
      reinterpret_cast<const uint8*>(segmentation.category_mask().data());
  EXPECT_EQ(category_mask[0], /*car*/ 7);
}

TEST_F(PostprocessTest, SucceedsWithCategoryMaskAndOrientation) {
  ImageSegmenterOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kDeepLabV3));
  // Frame buffer with kRightBottom orientation.
  std::unique_ptr<FrameBuffer> frame_buffer = CreateFromRgbaRawBuffer(
      /*input=*/nullptr, {}, FrameBuffer::Orientation::kRightBottom);

  SetUp(options);
  ASSERT_TRUE(test_image_segmenter_ != nullptr) << init_status_;
  SUPPORT_ASSERT_OK_AND_ASSIGN(const TfLiteTensor* output_tensor,
                       FillAndGetOutputTensor());
  SUPPORT_ASSERT_OK_AND_ASSIGN(SegmentationResult result,
                       test_image_segmenter_->Postprocess(
                           {output_tensor}, *frame_buffer, /*roi=*/{}));

  EXPECT_EQ(result.segmentation_size(), 1);
  const Segmentation& segmentation = result.segmentation(0);
  ExpectApproximatelyEqual(
      segmentation, ParseTextProtoOrDie<Segmentation>(kDeepLabV3PartialResult));
  EXPECT_TRUE(segmentation.has_category_mask());
  // Check bottom-right corner has expected class.
  const uint8* category_mask =
      reinterpret_cast<const uint8*>(segmentation.category_mask().data());
  EXPECT_EQ(category_mask[/*width*/ 257 * /*height*/ 257 - 1], /*car*/ 7);
}

TEST_F(PostprocessTest, SucceedsWithConfidenceMask) {
  ImageSegmenterOptions options;
  options.set_output_type(ImageSegmenterOptions::CONFIDENCE_MASK);
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kDeepLabV3));
  std::unique_ptr<FrameBuffer> frame_buffer =
      CreateFromRgbaRawBuffer(/*input=*/nullptr, {});

  SetUp(options);
  ASSERT_TRUE(test_image_segmenter_ != nullptr) << init_status_;
  SUPPORT_ASSERT_OK_AND_ASSIGN(const TfLiteTensor* output_tensor,
                       FillAndGetOutputTensor());
  SUPPORT_ASSERT_OK_AND_ASSIGN(SegmentationResult result,
                       test_image_segmenter_->Postprocess(
                           {output_tensor}, *frame_buffer, /*roi=*/{}));

  EXPECT_EQ(result.segmentation_size(), 1);
  const Segmentation& segmentation = result.segmentation(0);
  ExpectApproximatelyEqual(
      segmentation, ParseTextProtoOrDie<Segmentation>(kDeepLabV3PartialResult));
  EXPECT_TRUE(segmentation.has_confidence_masks());
  const Segmentation::ConfidenceMasks confidence_masks =
      segmentation.confidence_masks();
  EXPECT_EQ(confidence_masks.confidence_mask_size(), confidence_scores_.size());
  // Check top-left corner has expected confidences.
  for (int index = 0; index < confidence_scores_.size(); ++index) {
    const float* confidence_mask = reinterpret_cast<const float*>(
        confidence_masks.confidence_mask(index).value().data());
    EXPECT_EQ(confidence_mask[0], confidence_scores_[index]);
  }
}

TEST_F(PostprocessTest, SucceedsWithConfidenceMaskAndOrientation) {
  ImageSegmenterOptions options;
  options.set_output_type(ImageSegmenterOptions::CONFIDENCE_MASK);
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kDeepLabV3));
  // Frame buffer with kRightBottom orientation.
  std::unique_ptr<FrameBuffer> frame_buffer = CreateFromRgbaRawBuffer(
      /*input=*/nullptr, {}, FrameBuffer::Orientation::kRightBottom);

  SetUp(options);
  ASSERT_TRUE(test_image_segmenter_ != nullptr) << init_status_;
  SUPPORT_ASSERT_OK_AND_ASSIGN(const TfLiteTensor* output_tensor,
                       FillAndGetOutputTensor());
  SUPPORT_ASSERT_OK_AND_ASSIGN(SegmentationResult result,
                       test_image_segmenter_->Postprocess(
                           {output_tensor}, *frame_buffer, /*roi=*/{}));

  EXPECT_EQ(result.segmentation_size(), 1);
  const Segmentation& segmentation = result.segmentation(0);
  ExpectApproximatelyEqual(
      segmentation, ParseTextProtoOrDie<Segmentation>(kDeepLabV3PartialResult));
  EXPECT_TRUE(segmentation.has_confidence_masks());
  const Segmentation::ConfidenceMasks confidence_masks =
      segmentation.confidence_masks();
  EXPECT_EQ(confidence_masks.confidence_mask_size(), confidence_scores_.size());
  // Check top-left corner has expected confidences.
  for (int index = 0; index < confidence_scores_.size(); ++index) {
    const float* confidence_mask = reinterpret_cast<const float*>(
        confidence_masks.confidence_mask(index).value().data());
    EXPECT_EQ(confidence_mask[/*width*/ 257 * /*height*/ 257 - 1],
              confidence_scores_[index]);
  }
}

}  // namespace
}  // namespace vision
}  // namespace task
}  // namespace tflite
