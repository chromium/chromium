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

#include "tensorflow_lite_support/cc/task/vision/image_searcher.h"

#include <memory>
#include <string>

#include "absl/flags/flag.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/cord.h"  // from @com_google_absl
#include "absl/strings/str_cat.h"  // from @com_google_absl
#include "tensorflow/lite/test_util.h"
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/status_matchers.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/proto/base_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/embedding_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_result.pb.h"
#include "tensorflow_lite_support/cc/task/vision/core/frame_buffer.h"
#include "tensorflow_lite_support/cc/task/vision/proto/image_searcher_options.pb.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"
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
using ::tflite::task::processor::NearestNeighbor;
using ::tflite::task::processor::SearchResult;

constexpr char kTestDataDirectory[] =
    "/tensorflow_lite_support/cc/test/testdata/task/"
    "vision/";
// Test embedder model. Float inputs, produces feature vectors that are not
// L2-normalized as this model doesn't include a L2_NORMALIZATION TFLite Op.
constexpr char kMobileNetV3Embedder[] =
    "mobilenet_v3_small_100_224_embedder.tflite";
// Standalone test index.
constexpr char kIndex[] = "searcher_index.ldb";
// Test searcher model. Identical to kMobileNetV3Embedder, but with the contents
// of kIndex baked into the model metadata.
constexpr char kMobileNetV3Searcher[] =
    "mobilenet_v3_small_100_224_searcher.tflite";

StatusOr<ImageData> LoadImage(std::string image_name) {
  return DecodeImageFromFile(JoinPath("./" /*test src dir*/,
                                      kTestDataDirectory, image_name));
}

// Checks that the two provided `SearchResult`  protos are equal, with a
// tolerancy on floating-point scores to account for numerical instabilities.
void ExpectApproximatelyEqual(const SearchResult& actual,
                              const SearchResult& expected) {
  const float kPrecision = 1e-5;
  EXPECT_EQ(actual.nearest_neighbors_size(), expected.nearest_neighbors_size());
  for (int i = 0; i < actual.nearest_neighbors_size(); ++i) {
    const NearestNeighbor& a = actual.nearest_neighbors(i);
    const NearestNeighbor& b = expected.nearest_neighbors(i);
    EXPECT_EQ(a.metadata(), b.metadata());
    EXPECT_NEAR(a.distance(), b.distance(), kPrecision);
  }
}

class CreateFromOptionsTest : public tflite::testing::Test {};

TEST_F(CreateFromOptionsTest, SucceedsWithStandaloneIndex) {
  ImageSearcherOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetV3Embedder));
  options.mutable_embedding_options()->set_l2_normalize(true);
  options.mutable_search_options()->mutable_index_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory, kIndex));

  SUPPORT_ASSERT_OK(ImageSearcher::CreateFromOptions(options));
}

TEST_F(CreateFromOptionsTest, SucceedsWithMetadataIndex) {
  ImageSearcherOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetV3Searcher));
  options.mutable_embedding_options()->set_l2_normalize(true);

  SUPPORT_ASSERT_OK(ImageSearcher::CreateFromOptions(options));
}

TEST_F(CreateFromOptionsTest, FailsWithMissingModel) {
  ImageSearcherOptions options;
  options.mutable_embedding_options()->set_l2_normalize(true);
  options.mutable_search_options()->mutable_index_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory, kIndex));

  StatusOr<std::unique_ptr<ImageSearcher>> image_searcher_or =
      ImageSearcher::CreateFromOptions(options);

  EXPECT_EQ(image_searcher_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(
      image_searcher_or.status().message(),
      HasSubstr("Missing mandatory `model_file` field in `base_options`"));
  EXPECT_THAT(image_searcher_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST_F(CreateFromOptionsTest, FailsWithMissingIndex) {
  ImageSearcherOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetV3Embedder));
  options.mutable_embedding_options()->set_l2_normalize(true);

  StatusOr<std::unique_ptr<ImageSearcher>> image_searcher_or =
      ImageSearcher::CreateFromOptions(options);

  EXPECT_EQ(image_searcher_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(
      image_searcher_or.status().message(),
      HasSubstr("Unable to find index file: SearchOptions.index_file is not "
                "set and no AssociatedFile with type SCANN_INDEX_FILE could be "
                "found in the output tensor metadata."));
  EXPECT_THAT(image_searcher_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(absl::StrCat(
                  TfLiteSupportStatus::kMetadataAssociatedFileNotFoundError))));
}

TEST_F(CreateFromOptionsTest, FailsWithQuantization) {
  ImageSearcherOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetV3Embedder));
  options.mutable_embedding_options()->set_l2_normalize(true);
  options.mutable_embedding_options()->set_quantize(true);
  options.mutable_search_options()->mutable_index_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory, kIndex));

  StatusOr<std::unique_ptr<ImageSearcher>> image_searcher_or =
      ImageSearcher::CreateFromOptions(options);

  EXPECT_EQ(image_searcher_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(image_searcher_or.status().message(),
              HasSubstr("Setting EmbeddingOptions.quantize = true is not "
                        "allowed in searchers"));
  EXPECT_THAT(image_searcher_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST_F(CreateFromOptionsTest, FailsWithInvalidMaxResults) {
  ImageSearcherOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetV3Embedder));
  options.mutable_embedding_options()->set_l2_normalize(true);
  options.mutable_search_options()->mutable_index_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory, kIndex));
  options.mutable_search_options()->set_max_results(-1);

  StatusOr<std::unique_ptr<ImageSearcher>> image_searcher_or =
      ImageSearcher::CreateFromOptions(options);

  EXPECT_EQ(image_searcher_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(image_searcher_or.status().message(),
              HasSubstr("SearchOptions.max_results must be > 0, found -1"));
  EXPECT_THAT(image_searcher_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST(SearchTest, SucceedsWithStandaloneIndex) {
  // Create Searcher.
  ImageSearcherOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetV3Embedder));
  options.mutable_embedding_options()->set_l2_normalize(true);
  options.mutable_search_options()->mutable_index_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory, kIndex));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageSearcher> searcher,
                       ImageSearcher::CreateFromOptions(options));
  // Load image.
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData image, LoadImage("burger.jpg"));
  std::unique_ptr<FrameBuffer> frame_buffer = CreateFromRgbRawBuffer(
      image.pixel_data, FrameBuffer::Dimension{image.width, image.height});

  // Perform search.
  SUPPORT_ASSERT_OK_AND_ASSIGN(const SearchResult& result,
                       searcher->Search(*frame_buffer));
  ImageDataFree(&image);

  // Check results.
  ExpectApproximatelyEqual(
      result, ParseTextProtoOrDie<SearchResult>(R"pb(
        nearest_neighbors { metadata: "burger" distance: 0.0 }
        nearest_neighbors { metadata: "car" distance: 1.82244 }
        nearest_neighbors { metadata: "bird" distance: 1.93094 }
        nearest_neighbors { metadata: "dog" distance: 2.04736 }
        nearest_neighbors { metadata: "cat" distance: 2.07587 }
      )pb"));
}

TEST(SearchTest, SucceedsWithMetadataIndex) {
  // Create Searcher.
  ImageSearcherOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetV3Searcher));
  options.mutable_embedding_options()->set_l2_normalize(true);
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageSearcher> searcher,
                       ImageSearcher::CreateFromOptions(options));
  // Load image.
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData image, LoadImage("burger.jpg"));
  std::unique_ptr<FrameBuffer> frame_buffer = CreateFromRgbRawBuffer(
      image.pixel_data, FrameBuffer::Dimension{image.width, image.height});

  // Perform search.
  SUPPORT_ASSERT_OK_AND_ASSIGN(const SearchResult& result,
                       searcher->Search(*frame_buffer));
  ImageDataFree(&image);

  // Check results.
  ExpectApproximatelyEqual(
      result, ParseTextProtoOrDie<SearchResult>(R"pb(
        nearest_neighbors { metadata: "burger" distance: 0.0 }
        nearest_neighbors { metadata: "car" distance: 1.82244 }
        nearest_neighbors { metadata: "bird" distance: 1.93094 }
        nearest_neighbors { metadata: "dog" distance: 2.04736 }
        nearest_neighbors { metadata: "cat" distance: 2.07587 }
      )pb"));
}

TEST(SearchTest, SucceedsWithMaxResults) {
  // Create Searcher.
  ImageSearcherOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetV3Embedder));
  options.mutable_embedding_options()->set_l2_normalize(true);
  options.mutable_search_options()->mutable_index_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory, kIndex));
  options.mutable_search_options()->set_max_results(2);
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ImageSearcher> searcher,
                       ImageSearcher::CreateFromOptions(options));
  // Load image.
  SUPPORT_ASSERT_OK_AND_ASSIGN(ImageData image, LoadImage("burger.jpg"));
  std::unique_ptr<FrameBuffer> frame_buffer = CreateFromRgbRawBuffer(
      image.pixel_data, FrameBuffer::Dimension{image.width, image.height});

  // Perform search.
  SUPPORT_ASSERT_OK_AND_ASSIGN(const SearchResult& result,
                       searcher->Search(*frame_buffer));
  ImageDataFree(&image);

  // Check results.
  ExpectApproximatelyEqual(
      result, ParseTextProtoOrDie<SearchResult>(R"pb(
        nearest_neighbors { metadata: "burger" distance: 0.0 }
        nearest_neighbors { metadata: "car" distance: 1.82244 }
      )pb"));
}

}  // namespace
}  // namespace vision
}  // namespace task
}  // namespace tflite
