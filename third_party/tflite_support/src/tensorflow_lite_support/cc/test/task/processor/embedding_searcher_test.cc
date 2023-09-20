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

#include "tensorflow_lite_support/cc/task/processor/embedding_searcher.h"

#include <memory>
#include <string>

#include "absl/flags/flag.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/string_view.h"  // from @com_google_absl
#include "tensorflow/lite/test_util.h"
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_matchers.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/external_file_handler.h"
#include "tensorflow_lite_support/cc/task/core/proto/external_file_proto_inc.h"
#include "tensorflow_lite_support/cc/task/core/tflite_engine.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_result.pb.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"
#include "tensorflow_lite_support/metadata/cc/metadata_extractor.h"
#include "tensorflow_lite_support/metadata/metadata_schema_generated.h"

namespace tflite {
namespace task {
namespace processor {
namespace {

using ::testing::HasSubstr;
using ::tflite::TensorMetadata;
using ::tflite::metadata::ModelMetadataExtractor;
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
// The embedding proto for burger.jpg image derived from kMobileNetV3Searcher.
constexpr char kBurgerJpgEmbeddingProto[] = "burger_jpg_embedding.pbtxt";

/** Returns the index file portion of the provided metadata. */
StatusOr<absl::string_view> GetIndexFileContentFromMetadata(
    const ModelMetadataExtractor& metadata_extractor,
    const TensorMetadata& tensor_metadata) {
  auto index_file_name = ModelMetadataExtractor::FindFirstAssociatedFileName(
      tensor_metadata, tflite::AssociatedFileType_SCANN_INDEX_FILE);
  if (index_file_name.empty()) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "Unable to find index file: SearchOptions.index_file is not set and no "
        "AssociatedFile with type SCANN_INDEX_FILE could be found in the "
        "output tensor metadata.",
        TfLiteSupportStatus::kMetadataAssociatedFileNotFoundError);
  }
  return metadata_extractor.GetAssociatedFile(index_file_name);
}

/** Returns the index file content of the provided model. */
StatusOr<std::string> GetIndexFileContentFromModelFile(
    const std::string& model_path) {
  auto engine = std::make_unique<core::TfLiteEngine>();
  TFLITE_RETURN_IF_ERROR(engine->BuildModelFromFile(model_path));

  const tflite::metadata::ModelMetadataExtractor* metadata_extractor =
      engine->metadata_extractor();
  const TensorMetadata* tensor_metadata =
      metadata_extractor->GetOutputTensorMetadata(0);
  TFLITE_ASSIGN_OR_RETURN(
      absl::string_view index_file_content,
      GetIndexFileContentFromMetadata(*metadata_extractor, *tensor_metadata));

  // Copy the content as the model file and subsequently its index_file_content
  // string_view is going out of scope.
  return std::string(index_file_content);
}

/** Returns the file content. */
StatusOr<std::string> GetFileContent(const std::string& file_path) {
  tflite::task::core::ExternalFile external_file;
  external_file.set_file_name(file_path);
  TFLITE_ASSIGN_OR_RETURN(
      auto handler,
      core::ExternalFileHandler::CreateFromExternalFile(&external_file));
  absl::string_view file_content = handler->GetFileContent();
  // Copy the content, The file and its string_view is going out of scope.
  return std::string(file_content);
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
  auto options = std::make_unique<SearchOptions>();
  options->mutable_index_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory, kIndex));

  SUPPORT_ASSERT_OK(EmbeddingSearcher::Create(std::move(options)));
}

TEST_F(CreateFromOptionsTest, SucceedsWithMetadataIndex) {
  StatusOr<std::string> index_file_content = GetIndexFileContentFromModelFile(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetV3Searcher));
  SUPPORT_ASSERT_OK(index_file_content);

  SUPPORT_ASSERT_OK(EmbeddingSearcher::Create(std::make_unique<SearchOptions>(),
                                      *index_file_content));
}

TEST_F(CreateFromOptionsTest, FailsWithMissingIndexAndMissingMetadataIndex) {
  // Default options with no index file.
  auto options = std::make_unique<SearchOptions>();

  // And not providing an explicit index file content param either.
  StatusOr<std::unique_ptr<EmbeddingSearcher>> embedding_searcher =
      EmbeddingSearcher::Create(std::move(options));

  EXPECT_EQ(embedding_searcher.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(
      embedding_searcher.status().message(),
      HasSubstr(
          "Index File Content is expected when index_file option is not set."));
}

TEST_F(CreateFromOptionsTest, FailsWithInvalidMaxResults) {
  auto options = std::make_unique<SearchOptions>();
  options->mutable_index_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory, kIndex));
  options->set_max_results(-1);

  StatusOr<std::unique_ptr<EmbeddingSearcher>> embedding_searcher =
      EmbeddingSearcher::Create(std::move(options));

  EXPECT_EQ(embedding_searcher.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(embedding_searcher.status().message(),
              HasSubstr("SearchOptions.max_results must be > 0, found -1"));
}

TEST(SearchTest, SucceedsWithStandaloneIndex) {
  // Create Searcher.
  auto options = std::make_unique<SearchOptions>();
  options->mutable_index_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory, kIndex));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<EmbeddingSearcher> embedding_searcher,
                       EmbeddingSearcher::Create(std::move(options)));

  // Load the embedding proto associated with burger.jpg.
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      std::string embedding_file_content,
      GetFileContent(JoinPath("./" /*test src dir*/,
                              kTestDataDirectory, kBurgerJpgEmbeddingProto)));
  Embedding embedding = ParseTextProtoOrDie<Embedding>(embedding_file_content);

  // Perform search.
  SUPPORT_ASSERT_OK_AND_ASSIGN(const SearchResult& result,
                       embedding_searcher->Search(embedding));

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
  StatusOr<std::string> index_file_content = GetIndexFileContentFromModelFile(
      JoinPath("./" /*test src dir*/, kTestDataDirectory,
               kMobileNetV3Searcher));
  SUPPORT_ASSERT_OK(index_file_content);

  // Create Searcher.
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<EmbeddingSearcher> embedding_searcher,
      EmbeddingSearcher::Create(std::make_unique<SearchOptions>(),
                                *index_file_content));

  // Load the embedding proto associated with burger.jpg.
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      std::string embedding_file_content,
      GetFileContent(JoinPath("./" /*test src dir*/,
                              kTestDataDirectory, kBurgerJpgEmbeddingProto)));
  Embedding embedding = ParseTextProtoOrDie<Embedding>(embedding_file_content);

  // Perform search.
  SUPPORT_ASSERT_OK_AND_ASSIGN(const SearchResult& result,
                       embedding_searcher->Search(embedding));

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
  auto options = std::make_unique<SearchOptions>();
  options->mutable_index_file()->set_file_name(
      JoinPath("./" /*test src dir*/, kTestDataDirectory, kIndex));
  options->set_max_results(2);
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<EmbeddingSearcher> embedding_searcher,
                       EmbeddingSearcher::Create(std::move(options)));

  // Load the embedding proto associated with burger.jpg.
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      std::string embedding_file_content,
      GetFileContent(JoinPath("./" /*test src dir*/,
                              kTestDataDirectory, kBurgerJpgEmbeddingProto)));
  Embedding embedding = ParseTextProtoOrDie<Embedding>(embedding_file_content);

  // Perform search.
  SUPPORT_ASSERT_OK_AND_ASSIGN(const SearchResult& result,
                       embedding_searcher->Search(embedding));
  // Check results.
  ExpectApproximatelyEqual(
      result, ParseTextProtoOrDie<SearchResult>(R"pb(
        nearest_neighbors { metadata: "burger" distance: 0.0 }
        nearest_neighbors { metadata: "car" distance: 1.82244 }
      )pb"));
}

}  // namespace
}  // namespace processor
}  // namespace task
}  // namespace tflite
