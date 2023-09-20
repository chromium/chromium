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

#include "tensorflow_lite_support/cc/task/processor/search_postprocessor.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "absl/strings/string_view.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/tflite_engine.h"
#include "tensorflow_lite_support/cc/task/processor/embedding_postprocessor.h"
#include "tensorflow_lite_support/cc/task/processor/embedding_searcher.h"
#include "tensorflow_lite_support/cc/task/processor/proto/embedding.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/embedding_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_result.pb.h"
#include "tensorflow_lite_support/metadata/cc/metadata_extractor.h"
#include "tensorflow_lite_support/metadata/metadata_schema_generated.h"

namespace tflite {
namespace task {
namespace processor {

namespace {

using ::tflite::TensorMetadata;
using ::tflite::metadata::ModelMetadataExtractor;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::task::core::TfLiteEngine;
using ::tflite::task::processor::Embedding;

absl::StatusOr<std::unique_ptr<EmbeddingPostprocessor>>
CreateEmbeddingPostprocessor(TfLiteEngine* engine,
                             const std::initializer_list<int> output_indices,
                             std::unique_ptr<EmbeddingOptions> options) {
  if (options->quantize()) {
    // ScaNN only supports searching from float embeddings.
    return CreateStatusWithPayload(absl::StatusCode::kInvalidArgument,
                                   "Setting EmbeddingOptions.quantize = true "
                                   "is not allowed in searchers.",
                                   TfLiteSupportStatus::kInvalidArgumentError);
  }
  return EmbeddingPostprocessor::Create(engine, output_indices,
                                        std::move(options));
}

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

}  // namespace

/* static */
StatusOr<std::unique_ptr<SearchPostprocessor>> SearchPostprocessor::Create(
    TfLiteEngine* engine, int output_index,
    std::unique_ptr<SearchOptions> search_options,
    std::unique_ptr<EmbeddingOptions> embedding_options) {
  TFLITE_ASSIGN_OR_RETURN(auto embedding_postprocessor,
                   CreateEmbeddingPostprocessor(engine, {output_index},
                                                std::move(embedding_options)));

  TFLITE_ASSIGN_OR_RETURN(auto search_processor,
                   Processor::Create<SearchPostprocessor>(
                       /* num_expected_tensors =*/1, engine, {output_index},
                       /* requires_metadata =*/false));

  TFLITE_RETURN_IF_ERROR(search_processor->Init(std::move(embedding_postprocessor),
                                         std::move(search_options)));
  return search_processor;
}

StatusOr<SearchResult> SearchPostprocessor::Postprocess() {
  // Extract embedding.
  Embedding embedding;
  TFLITE_RETURN_IF_ERROR(embedding_postprocessor_->Postprocess(&embedding));

  // Search the nearest-neighbor embedding.
  TFLITE_ASSIGN_OR_RETURN(SearchResult search_result,
                   embedding_searcher_->Search(embedding));
  return search_result;
}

StatusOr<absl::string_view> SearchPostprocessor::GetUserInfo() {
  return embedding_searcher_->GetUserInfo();
}

absl::Status SearchPostprocessor::Init(
    std::unique_ptr<EmbeddingPostprocessor> embedding_postprocessor,
    std::unique_ptr<SearchOptions> options) {
  embedding_postprocessor_ = std::move(embedding_postprocessor);

  if (options->has_index_file()) {
    TFLITE_ASSIGN_OR_RETURN(embedding_searcher_,
                     EmbeddingSearcher::Create(std::move(options)));
  } else {
    // Index File is expected in the metadata if not provided in the options.
    TFLITE_ASSIGN_OR_RETURN(absl::string_view index_file_content,
                     GetIndexFileContentFromMetadata(*GetMetadataExtractor(),
                                                     *GetTensorMetadata()));
    TFLITE_ASSIGN_OR_RETURN(
        embedding_searcher_,
        EmbeddingSearcher::Create(std::move(options), index_file_content));
  }

  return absl::OkStatus();
}

}  // namespace processor
}  // namespace task
}  // namespace tflite
