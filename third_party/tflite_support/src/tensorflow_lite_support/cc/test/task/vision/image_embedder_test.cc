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

#include "tensorflow_lite_support/cc/task/vision/image_embedder.h"

#include <memory>

#include "absl/flags/flag.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/kernels/builtin_op_kernels.h"
#include "tensorflow/lite/mutable_op_resolver.h"
#include "tensorflow/lite/test_util.h"
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/status_matchers.h"
#include "tensorflow_lite_support/cc/task/vision/core/frame_buffer.h"
#include "tensorflow_lite_support/cc/task/vision/proto/bounding_box_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/embeddings_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/image_embedder_options_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/image_utils.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"

namespace tflite {
namespace task {
namespace vision {
namespace {

using ::testing::HasSubstr;
using ::testing::Optional;
using ::tflite::support::kTfLiteSupportPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::task::JoinPath;

constexpr char kTestDataDirectory[] =
    "/tensorflow_lite_support/cc/test/testdata/task/"
    "vision/";
// Test model. Float inputs, produces feature vectors that are not
// L2-normalized as this model doesn't include a L2_NORMALIZATION TFLite Op.
constexpr char kMobileNetV3[] = "mobilenet_v3_small_100_224_embedder.tflite";
// Tolerancy for cosine similarity evaluation.
constexpr double kSimilarityTolerancy = 1e-6;

StatusOr<ImageData> LoadImage(std::string image_name) {
  return DecodeImageFromFile(JoinPath("./" /*test src dir*/,
                                      kTestDataDirectory, image_name));
}

class MobileNetV3OpResolver : public ::tflite::MutableOpResolver {
 public:
  MobileNetV3OpResolver() {
    AddBuiltin(::tflite::BuiltinOperator_MUL,
               ::tflite::ops::builtin::Register_MUL());
    AddBuiltin(::tflite::BuiltinOperator_SUB,
               ::tflite::ops::builtin::Register_SUB());
    AddBuiltin(::tflite::BuiltinOperator_CONV_2D,
               ::tflite::ops::builtin::Register_CONV_2D());
    AddBuiltin(::tflite::BuiltinOperator_HARD_SWISH,
               ::tflite::ops::builtin::Register_HARD_SWISH());
    AddBuiltin(::tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
               ::tflite::ops::builtin::Register_DEPTHWISE_CONV_2D());
    AddBuiltin(::tflite::BuiltinOperator_MEAN,
               ::tflite::ops::builtin::Register_MEAN());
    AddBuiltin(::tflite::BuiltinOperator_ADD,
               ::tflite::ops::builtin::Register_ADD());
    AddBuiltin(::tflite::BuiltinOperator_AVERAGE_POOL_2D,
               ::tflite::ops::builtin::Register_AVERAGE_POOL_2D());
    AddBuiltin(::tflite::BuiltinOperator_RESHAPE,
               ::tflite::ops::builtin::Register_RESHAPE());
  }

  MobileNetV3OpResolver(const MobileNetV3OpResolver& r) = delete;
};

class CreateFromOptionsTest : public tflite::testing::Test {};

TEST_F(CreateFromOptionsTest, SucceedsWithSelectiveOpResolver) {
  ImageEmbedderOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kMobileNetV3));

  SUPPORT_ASSERT_OK(ImageEmbedder::CreateFromOptions(
      options, absl::make_unique<MobileNetV3OpResolver>()));
}

class MobileNetV3OpResolverMissingOps : public ::tflite::MutableOpResolver {
 public:
  MobileNetV3OpResolverMissingOps() {
    AddBuiltin(::tflite::BuiltinOperator_SOFTMAX,
               ::tflite::ops::builtin::Register_SOFTMAX());
  }

  MobileNetV3OpResolverMissingOps(const MobileNetV3OpResolverMissingOps& r) =
      delete;
};

TEST_F(CreateFromOptionsTest, FailsWithSelectiveOpResolverMissingOps) {
  ImageEmbedderOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kMobileNetV3));

  auto image_embedder_or = ImageEmbedder::CreateFromOptions(
      options, absl::make_unique<MobileNetV3OpResolverMissingOps>());
  EXPECT_EQ(image_embedder_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(image_embedder_or.status().message(),
              HasSubstr("Didn't find op for builtin opcode"));
  EXPECT_THAT(image_embedder_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kUnsupportedBuiltinOp))));
}

TEST_F(CreateFromOptionsTest, FailsWithMissingModel) {
  ImageEmbedderOptions options;

  StatusOr<std::unique_ptr<ImageEmbedder>> image_embedder_or =
      ImageEmbedder::CreateFromOptions(options);

  EXPECT_EQ(image_embedder_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(image_embedder_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

// Checks that CosineSimilarity fails if provided with a quantized and a float
// feature vector.
TEST(CosineSimilarityTest, FailsWithDifferentFeatureVectorTypes) {
  FeatureVector u;
  *u.mutable_value_string() = "\x01\x02";
  FeatureVector v;
  v.add_value_float(0.1);
  v.add_value_float(0.2);

  StatusOr<double> uv_similarity_or = ImageEmbedder::CosineSimilarity(u, v);

  EXPECT_EQ(uv_similarity_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(uv_similarity_or.status().message(),
              HasSubstr("quantized and float"));
}

// Checks that CosineSimilarity fails if provided with feature vectors of
// different sizes.
TEST(CosineSimilarityTest, FailsWithDifferentFeatureVectorSizes) {
  FeatureVector u_float;
  u_float.add_value_float(0.1);
  FeatureVector v_float;
  v_float.add_value_float(0.1);
  v_float.add_value_float(0.2);
  FeatureVector u_quantized;
  *u_quantized.mutable_value_string() = "\x01";
  FeatureVector v_quantized;
  *v_quantized.mutable_value_string() = "\x01\x02";

  StatusOr<double> float_similarity_or =
      ImageEmbedder::CosineSimilarity(u_float, v_float);
  StatusOr<double> quantized_similarity_or =
      ImageEmbedder::CosineSimilarity(u_quantized, v_quantized);

  EXPECT_EQ(float_similarity_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(float_similarity_or.status().message(),
              HasSubstr("different sizes"));
  EXPECT_EQ(quantized_similarity_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(quantized_similarity_or.status().message(),
              HasSubstr("different sizes"));
}

// Checks that CosineSimilarity fails if one of the feature vectors has 0 norm.
TEST(CosineSimilarityTest, FailsWithZeroNorm) {
  FeatureVector u_float;
  u_float.add_value_float(0.0);
  u_float.add_value_float(0.0);
  FeatureVector v_float;
  v_float.add_value_float(0.1);
  v_float.add_value_float(0.2);
  FeatureVector u_quantized;
  // Prevent literal from being interpreted as null-terminated C-style string.
  *u_quantized.mutable_value_string() = std::string("\x00\x00", 2);
  FeatureVector v_quantized;
  *v_quantized.mutable_value_string() = "\x01\x02";

  StatusOr<double> float_similarity_or =
      ImageEmbedder::CosineSimilarity(u_float, v_float);
  StatusOr<double> quantized_similarity_or =
      ImageEmbedder::CosineSimilarity(u_quantized, v_quantized);

  EXPECT_EQ(float_similarity_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(float_similarity_or.status().message(), HasSubstr("0 norm"));
  EXPECT_EQ(quantized_similarity_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(quantized_similarity_or.status().message(), HasSubstr("0 norm"));
}

// Checks that CosineSimilarity produces expected results.
TEST(CosineSimilarityTest, Succeeds) {
  FeatureVector u_float;
  u_float.add_value_float(1.0);
  u_float.add_value_float(0.0);
  u_float.add_value_float(0.0);
  u_float.add_value_float(0.0);
  FeatureVector v_float;
  v_float.add_value_float(0.5);
  v_float.add_value_float(0.5);
  v_float.add_value_float(0.5);
  v_float.add_value_float(0.5);
  FeatureVector u_quantized;
  // Prevent literal from being interpreted as null-terminated C-style string.
  *u_quantized.mutable_value_string() = std::string("\x7f\x00\x00\x00", 4);
  FeatureVector v_quantized;
  // Prevent literal from being interpreted as null-terminated C-style string.
  *v_quantized.mutable_value_string() = std::string("\x80\x00\x00\x00", 4);

  SUPPORT_ASSERT_OK_AND_ASSIGN(double float_similarity,
                       ImageEmbedder::CosineSimilarity(u_float, v_float));
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      double quantized_similarity,
      ImageEmbedder::CosineSimilarity(u_quantized, v_quantized));

  EXPECT_EQ(float_similarity, 0.5);
  EXPECT_EQ(quantized_similarity, -1.0);
}

// Extracts feature vectors without L2 normalization on two image (one being
// slightly cropped from the other) and checks that cosine similarity is high.
TEST(EmbedTest, SucceedsWithoutL2Normalization) {
  // Create embedder.
  ImageEmbedderOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kMobileNetV3));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageEmbedder> embedder,
                       ImageEmbedder::CreateFromOptions(options));
  // Load images: one is a crop of the other.
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData image, LoadImage("burger.jpg"));
  std::unique_ptr<FrameBuffer> image_frame_buffer = CreateFromRgbRawBuffer(
      image.pixel_data, FrameBuffer::Dimension{image.width, image.height});
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData crop, LoadImage("burger_crop.jpg"));
  std::unique_ptr<FrameBuffer> crop_frame_buffer = CreateFromRgbRawBuffer(
      crop.pixel_data, FrameBuffer::Dimension{crop.width, crop.height});

  // Extract both embeddings.
  SUPPORT_ASSERT_OK_AND_ASSIGN(const EmbeddingResult& image_result,
                       embedder->Embed(*image_frame_buffer));
  ImageDataFree(&image);
  SUPPORT_ASSERT_OK_AND_ASSIGN(const EmbeddingResult& crop_result,
                       embedder->Embed(*crop_frame_buffer));
  ImageDataFree(&crop);

  // Check results sizes
  EXPECT_EQ(image_result.embeddings_size(), 1);
  const FeatureVector& image_feature_vector =
      image_result.embeddings(0).feature_vector();
  EXPECT_EQ(image_feature_vector.value_float_size(), 1024);
  EXPECT_EQ(crop_result.embeddings_size(), 1);
  const FeatureVector& crop_feature_vector =
      crop_result.embeddings(0).feature_vector();
  EXPECT_EQ(crop_feature_vector.value_float_size(), 1024);
  // Check cosine similarity.
  SUPPORT_ASSERT_OK_AND_ASSIGN(double similarity,
                       ImageEmbedder::CosineSimilarity(image_feature_vector,
                                                       crop_feature_vector));
  double expected_similarity = 0.932738;
  EXPECT_LE(abs(similarity - expected_similarity), kSimilarityTolerancy);
}

// Same as above, but with `l2_normalize` option set to true.
TEST(EmbedTest, SucceedsWithL2Normalization) {
  // Create embedder.
  ImageEmbedderOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kMobileNetV3));
  options.set_l2_normalize(true);
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageEmbedder> embedder,
                       ImageEmbedder::CreateFromOptions(options));
  // Load images: one is a crop of the other.
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData image, LoadImage("burger.jpg"));
  std::unique_ptr<FrameBuffer> image_frame_buffer = CreateFromRgbRawBuffer(
      image.pixel_data, FrameBuffer::Dimension{image.width, image.height});
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData crop, LoadImage("burger_crop.jpg"));
  std::unique_ptr<FrameBuffer> crop_frame_buffer = CreateFromRgbRawBuffer(
      crop.pixel_data, FrameBuffer::Dimension{crop.width, crop.height});

  // Extract both embeddings.
  SUPPORT_ASSERT_OK_AND_ASSIGN(const EmbeddingResult& image_result,
                       embedder->Embed(*image_frame_buffer));
  ImageDataFree(&image);
  SUPPORT_ASSERT_OK_AND_ASSIGN(const EmbeddingResult& crop_result,
                       embedder->Embed(*crop_frame_buffer));
  ImageDataFree(&crop);

  // Check results sizes
  EXPECT_EQ(image_result.embeddings_size(), 1);
  const FeatureVector& image_feature_vector =
      image_result.embeddings(0).feature_vector();
  EXPECT_EQ(image_feature_vector.value_float_size(), 1024);
  EXPECT_EQ(crop_result.embeddings_size(), 1);
  const FeatureVector& crop_feature_vector =
      crop_result.embeddings(0).feature_vector();
  EXPECT_EQ(crop_feature_vector.value_float_size(), 1024);
  // Check cosine similarity.
  SUPPORT_ASSERT_OK_AND_ASSIGN(double similarity,
                       ImageEmbedder::CosineSimilarity(image_feature_vector,
                                                       crop_feature_vector));
  double expected_similarity = 0.932738;
  EXPECT_LE(abs(similarity - expected_similarity), kSimilarityTolerancy);
}

// Same as above, but with `quantize` option set to true. Requires also setting
// `l2_normalize` to true, as per the documentation.
// Same as above, but with `l2_normalize` option set to true.
TEST(EmbedTest, SucceedsWithQuantization) {
  // Create embedder.
  ImageEmbedderOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kMobileNetV3));
  options.set_l2_normalize(true);
  options.set_quantize(true);
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageEmbedder> embedder,
                       ImageEmbedder::CreateFromOptions(options));
  // Load images: one is a crop of the other.
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData image, LoadImage("burger.jpg"));
  std::unique_ptr<FrameBuffer> image_frame_buffer = CreateFromRgbRawBuffer(
      image.pixel_data, FrameBuffer::Dimension{image.width, image.height});
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData crop, LoadImage("burger_crop.jpg"));
  std::unique_ptr<FrameBuffer> crop_frame_buffer = CreateFromRgbRawBuffer(
      crop.pixel_data, FrameBuffer::Dimension{crop.width, crop.height});

  // Extract both embeddings.
  SUPPORT_ASSERT_OK_AND_ASSIGN(const EmbeddingResult& image_result,
                       embedder->Embed(*image_frame_buffer));
  ImageDataFree(&image);
  SUPPORT_ASSERT_OK_AND_ASSIGN(const EmbeddingResult& crop_result,
                       embedder->Embed(*crop_frame_buffer));
  ImageDataFree(&crop);

  // Check results sizes
  EXPECT_EQ(image_result.embeddings_size(), 1);
  const FeatureVector& image_feature_vector =
      image_result.embeddings(0).feature_vector();
  EXPECT_EQ(image_feature_vector.value_string().size(), 1024);
  EXPECT_EQ(crop_result.embeddings_size(), 1);
  const FeatureVector& crop_feature_vector =
      crop_result.embeddings(0).feature_vector();
  EXPECT_EQ(crop_feature_vector.value_string().size(), 1024);
  // Check cosine similarity.
  SUPPORT_ASSERT_OK_AND_ASSIGN(double similarity,
                       ImageEmbedder::CosineSimilarity(image_feature_vector,
                                                       crop_feature_vector));
  // Close to but expectedly different from the above tests due to slight loss
  // of precision during quantization:
  double expected_similarity = 0.929717;
  EXPECT_LE(abs(similarity - expected_similarity), kSimilarityTolerancy);
}

// Extracts feature vectors on both the cropped image and the original image
// with a region of interest set to correspond to the cropped image, and checks
// that cosine similarity is close to 1.
TEST(EmbedTest, SucceedsWithRegionOfInterest) {
  // Create embedder.
  ImageEmbedderOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kMobileNetV3));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageEmbedder> embedder,
                       ImageEmbedder::CreateFromOptions(options));
  // Load images: one is a crop of the other.
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData image, LoadImage("burger.jpg"));
  std::unique_ptr<FrameBuffer> image_frame_buffer = CreateFromRgbRawBuffer(
      image.pixel_data, FrameBuffer::Dimension{image.width, image.height});
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData crop, LoadImage("burger_crop.jpg"));
  std::unique_ptr<FrameBuffer> crop_frame_buffer = CreateFromRgbRawBuffer(
      crop.pixel_data, FrameBuffer::Dimension{crop.width, crop.height});
  // Bounding box in "burger.jpg" corresponding to "burger_crop.jpg".
  BoundingBox roi;
  roi.set_origin_x(0);
  roi.set_origin_y(0);
  roi.set_width(400);
  roi.set_height(325);

  // Extract both embeddings.
  SUPPORT_ASSERT_OK_AND_ASSIGN(const EmbeddingResult& image_result,
                       embedder->Embed(*image_frame_buffer, roi));
  ImageDataFree(&image);
  SUPPORT_ASSERT_OK_AND_ASSIGN(const EmbeddingResult& crop_result,
                       embedder->Embed(*crop_frame_buffer));
  ImageDataFree(&crop);

  // Check results sizes
  EXPECT_EQ(image_result.embeddings_size(), 1);
  const FeatureVector& image_feature_vector =
      image_result.embeddings(0).feature_vector();
  EXPECT_EQ(image_feature_vector.value_float_size(), 1024);
  EXPECT_EQ(crop_result.embeddings_size(), 1);
  const FeatureVector& crop_feature_vector =
      crop_result.embeddings(0).feature_vector();
  EXPECT_EQ(crop_feature_vector.value_float_size(), 1024);
  // Check cosine similarity.
  SUPPORT_ASSERT_OK_AND_ASSIGN(double similarity,
                       ImageEmbedder::CosineSimilarity(image_feature_vector,
                                                       crop_feature_vector));
  double expected_similarity = 0.999914;
  EXPECT_LE(abs(similarity - expected_similarity), kSimilarityTolerancy);
}

TEST(GetEmbeddingDimension, Succeeds) {
  // Create embedder.
  ImageEmbedderOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kMobileNetV3));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageEmbedder> embedder,
                       ImageEmbedder::CreateFromOptions(options));

  EXPECT_EQ(embedder->GetEmbeddingDimension(0), 1024);
  EXPECT_EQ(embedder->GetEmbeddingDimension(1), -1);
}

TEST(GetNumberOfOutputLayers, Succeeds) {
  // Create embedder.
  ImageEmbedderOptions options;
  options.mutable_model_file_with_metadata()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kMobileNetV3));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageEmbedder> embedder,
                       ImageEmbedder::CreateFromOptions(options));

  EXPECT_EQ(embedder->GetNumberOfOutputLayers(), 1);
}

}  // namespace
}  // namespace vision
}  // namespace task
}  // namespace tflite
