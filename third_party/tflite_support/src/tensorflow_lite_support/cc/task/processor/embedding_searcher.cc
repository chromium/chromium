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

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <vector>

#include "tensorflow_lite_support/scann_ondevice/cc/core/partitioner.h"
#include "tensorflow_lite_support/scann_ondevice/cc/core/processor.h"
#include "tensorflow_lite_support/scann_ondevice/cc/core/searcher.h"
#include "tensorflow_lite_support/scann_ondevice/cc/core/serialized_searcher.pb.h"
#include "tensorflow_lite_support/scann_ondevice/cc/core/top_n_amortized_constant.h"
#include "absl/memory/memory.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "absl/strings/string_view.h"  // from @com_google_absl
#include "absl/types/span.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/external_file_handler.h"
#include "tensorflow_lite_support/cc/task/processor/proto/embedding.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_result.pb.h"
#include "tensorflow_lite_support/scann_ondevice/cc/index.h"
#include "tensorflow_lite_support/scann_ondevice/proto/index_config.pb.h"

namespace tflite {
namespace task {
namespace processor {

namespace {

constexpr int kNoNeighborId = -1;

using ::tflite::scann_ondevice::core::AsymmetricHashFindNeighbors;
using ::tflite::scann_ondevice::core::DistanceMeasure;
using ::tflite::scann_ondevice::core::FloatFindNeighbors;
using ::tflite::scann_ondevice::core::QueryInfo;
using ::tflite::scann_ondevice::core::ScannOnDeviceConfig;
using ::tflite::scann_ondevice::core::TopN;
using ::tflite::scann_ondevice::Index;
using ::tflite::scann_ondevice::IndexConfig;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::task::core::ExternalFileHandler;
using ::tflite::task::processor::Embedding;

using Matrix8u =
    Eigen::Matrix<uint8_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>;

absl::Status SanityCheckOptions(const SearchOptions& options) {
  if (options.max_results() < 1) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("SearchOptions.max_results must be > 0, found %d.",
                        options.max_results()),
        TfLiteSupportStatus::kInvalidArgumentError);
  }
  return absl::OkStatus();
}

absl::Status SanityCheckIndexConfig(const IndexConfig& config) {
  switch (config.embedding_type()) {
    case IndexConfig::UNSPECIFIED:
      return CreateStatusWithPayload(
          absl::StatusCode::kInvalidArgument,
          "Invalid IndexConfig: embedding_type must not be left UNSPECIFIED.",
          TfLiteSupportStatus::kInvalidArgumentError);
    case IndexConfig::FLOAT:
      if (config.scann_config().has_indexer()) {
        return CreateStatusWithPayload(
            absl::StatusCode::kInvalidArgument,
            "Invalid IndexConfig: embedding_type is set to FLOAT but ScaNN "
            "config specifies a product quantization codebook.",
            TfLiteSupportStatus::kInvalidArgumentError);
      }
      break;
    case IndexConfig::UINT8:
      if (!config.scann_config().has_indexer()) {
        return CreateStatusWithPayload(
            absl::StatusCode::kInvalidArgument,
            "Invalid IndexConfig: embedding_type is set to UINT8 but ScaNN "
            "config doesn't specify a product quantization codebook.",
            TfLiteSupportStatus::kInvalidArgumentError);
      }
      break;
    default:
      return CreateStatusWithPayload(
          absl::StatusCode::kInternal,
          "Invalid IndexConfig: unexpected value for embedding_type.",
          TfLiteSupportStatus::kError);
  }
  return absl::OkStatus();
}

absl::StatusOr<DistanceMeasure> GetDistanceMeasure(
    const ScannOnDeviceConfig& config) {
  DistanceMeasure measure = config.query_distance();
  if (measure == tflite::scann_ondevice::core::UNSPECIFIED) {
    if (config.has_indexer() && config.indexer().has_asymmetric_hashing()) {
      measure = config.indexer().asymmetric_hashing().query_distance();
    } else if (config.has_partitioner()) {
      measure = config.partitioner().query_distance();
    } else {
      return CreateStatusWithPayload(
          absl::StatusCode::kInvalidArgument,
          "ScaNN config does not provide mandatory DistanceMeasure.",
          TfLiteSupportStatus::kInvalidArgumentError);
    }

    if (measure == tflite::scann_ondevice::core::UNSPECIFIED) {
      return CreateStatusWithPayload(
          absl::StatusCode::kInvalidArgument,
          "UNSPECIFIED is not a valid value for ScaNN config DistanceMeasure.",
          TfLiteSupportStatus::kInvalidArgumentError);
    }

    // Make sure the query distance in different places are consistent.
    if (config.has_partitioner()) {
      DistanceMeasure partitioner_measure =
          config.partitioner().query_distance();
      if (measure != partitioner_measure) {
        return CreateStatusWithPayload(
            absl::StatusCode::kInvalidArgument,
            absl::StrFormat("DistanceMeasure %s is different from "
                            "DistanceMeasure %s found in partitioner config.",
                            DistanceMeasure_Name(measure),
                            DistanceMeasure_Name(partitioner_measure)),
            TfLiteSupportStatus::kInvalidArgumentError);
      }
    }
  }
  return measure;
}

absl::Status ConvertEmbeddingToEigenMatrix(const Embedding& embedding,
                                           Eigen::MatrixXf* matrix) {
  if (embedding.feature_vector().value_float().empty()) {
    // This should be caught upstream at EmbeddingPostprocessor creation.
    return CreateStatusWithPayload(absl::StatusCode::kInternal,
                                   "Float query embedding is empty.",
                                   TfLiteSupportStatus::kError);
  }
  Eigen::Map<const Eigen::VectorXf> query_ptr(
      embedding.feature_vector().value_float().data(),
      embedding.feature_vector().value_float().size());
  matrix->resize(embedding.feature_vector().value_float().size(), 1);
  matrix->col(0) = query_ptr;
  return absl::OkStatus();
}

}  // namespace

/* static */
StatusOr<std::unique_ptr<EmbeddingSearcher>> EmbeddingSearcher::Create(
    std::unique_ptr<SearchOptions> search_options,
    std::optional<absl::string_view> optional_index_file_content) {
  auto embedding_searcher = std::make_unique<EmbeddingSearcher>();

  TFLITE_RETURN_IF_ERROR(
      embedding_searcher->Init(
          std::move(search_options), optional_index_file_content));
  return embedding_searcher;
}

StatusOr<SearchResult> EmbeddingSearcher::Search(const Embedding& embedding) {
  // Convert embedding to Eigen matrix, as expected by ScaNN.
  Eigen::MatrixXf query;
  TFLITE_RETURN_IF_ERROR(ConvertEmbeddingToEigenMatrix(embedding, &query));

  // Identify partitions to search.
  std::vector<std::vector<int>> leaves_to_search(
      1, std::vector<int>(num_leaves_to_search_, -1));
  if (!partitioner_->Partition(query, &leaves_to_search)) {
    return CreateStatusWithPayload(absl::StatusCode::kInternal,
                                   "Partitioning failed.",
                                   TfLiteSupportStatus::kError);
  }

  // Prepare search results.
  std::vector<TopN> top_n;
  top_n.emplace_back(
      options_->max_results(),
      std::make_pair(std::numeric_limits<float>::max(), kNoNeighborId));
  // Perform search.
  if (quantizer_) {
    TFLITE_RETURN_IF_ERROR(
        QuantizedSearch(query, leaves_to_search[0], absl::MakeSpan(top_n)));
  } else {
    TFLITE_RETURN_IF_ERROR(
        LinearSearch(query, leaves_to_search[0], absl::MakeSpan(top_n)));
  }

  // Build results.
  SearchResult search_result;
  for (const auto& [distance, id] : top_n[0].Take()) {
    if (id == kNoNeighborId) {
      break;
    }
    TFLITE_ASSIGN_OR_RETURN(auto metadata, index_->GetMetadataAtIndex(id));
    NearestNeighbor* nearest_neighbor = search_result.add_nearest_neighbors();
    nearest_neighbor->set_distance(distance);
    nearest_neighbor->set_metadata(std::string(metadata));
  }
  return search_result;
}

StatusOr<absl::string_view> EmbeddingSearcher::GetUserInfo() {
  return index_->GetUserInfo();
}

absl::Status EmbeddingSearcher::Init(
    std::unique_ptr<SearchOptions> options,
    std::optional<absl::string_view> optional_index_file_content) {
  TFLITE_RETURN_IF_ERROR(SanityCheckOptions(*options));
  options_ = std::move(options);

  // Initialize index.
  absl::string_view index_file_content;
  if (options_->has_index_file()) {
    TFLITE_ASSIGN_OR_RETURN(
        index_file_handler_,
        ExternalFileHandler::CreateFromExternalFile(&options_->index_file()));
    index_file_content = index_file_handler_->GetFileContent();
  } else {
    if (!optional_index_file_content) {
      absl::Status status = CreateStatusWithPayload(
          absl::StatusCode::kInvalidArgument,
          "Index File Content is expected when index_file option is not set.");
      LOG(ERROR) << "EmbeddingSearcher: " << status;
      return status;
    }
    index_file_content = *optional_index_file_content;
  }
  TFLITE_ASSIGN_OR_RETURN(index_,
                   Index::CreateFromIndexBuffer(index_file_content.data(),
                                                index_file_content.size()));
  TFLITE_ASSIGN_OR_RETURN(index_config_, index_->GetIndexConfig());
  TFLITE_RETURN_IF_ERROR(SanityCheckIndexConfig(index_config_));
  // Get distance measure once and for all.
  TFLITE_ASSIGN_OR_RETURN(distance_measure_,
                   GetDistanceMeasure(index_config_.scann_config()));

  // Initialize partitioner.
  if (index_config_.scann_config().has_partitioner()) {
    partitioner_ = tflite::scann_ondevice::core::Partitioner::Create(
        index_config_.scann_config().partitioner());
    num_leaves_to_search_ = std::min(
        static_cast<int>(ceilf(
            partitioner_->NumPartitions() *
            index_config_.scann_config().partitioner().search_fraction())),
        partitioner_->NumPartitions());
  } else {
    partitioner_ = absl::make_unique<tflite::scann_ondevice::core::NoOpPartitioner>();
    num_leaves_to_search_ = partitioner_->NumPartitions();
  }

  // Initialize product quantizer if needed.
  if (index_config_.scann_config().has_indexer()) {
    quantizer_ = tflite::scann_ondevice::core::AsymmetricHashQuerier::Create(
        index_config_.scann_config().indexer().asymmetric_hashing());
  }

  return absl::OkStatus();
}

absl::Status EmbeddingSearcher::QuantizedSearch(
    Eigen::Ref<Eigen::MatrixXf> query, std::vector<int> leaves_to_search,
    absl::Span<TopN> top_n) {
  int dim = index_config_.embedding_dim();
  // Prepare QueryInfo used for all leaves.
  QueryInfo query_info;
  if (!quantizer_->Process(query, &query_info)) {
    return CreateStatusWithPayload(absl::StatusCode::kInternal,
                                   "Query quantization failed.",
                                   TfLiteSupportStatus::kError);
  }
  for (int leaf_id : leaves_to_search) {
    // Load partition into Eigen matrix.
    TFLITE_ASSIGN_OR_RETURN(auto partition, index_->GetPartitionAtIndex(leaf_id));
    int partition_size = partition.size() / dim;
    Eigen::Map<const Matrix8u> database(
        reinterpret_cast<const uint8_t*>(partition.data()), dim,
        partition_size);
    // Perform search.
    int global_offset = index_config_.global_partition_offsets(leaf_id);
    if (!AsymmetricHashFindNeighbors(query_info, database, global_offset,
                                     top_n)) {
      return CreateStatusWithPayload(absl::StatusCode::kInternal,
                                     "Nearest neighbor search failed.",
                                     TfLiteSupportStatus::kError);
    }
  }
  return absl::OkStatus();
}

absl::Status EmbeddingSearcher::LinearSearch(Eigen::Ref<Eigen::MatrixXf> query,
                                             std::vector<int> leaves_to_search,
                                             absl::Span<TopN> top_n) {
  int dim = index_config_.embedding_dim();
  for (int leaf_id : leaves_to_search) {
    // Load partition into Eigen matrix.
    TFLITE_ASSIGN_OR_RETURN(auto partition, index_->GetPartitionAtIndex(leaf_id));
    int partition_size = partition.size() / (dim * sizeof(float));
    Eigen::Map<const Eigen::MatrixXf> database(
        reinterpret_cast<const float*>(partition.data()), dim, partition_size);
    // Perform search.
    int global_offset = index_config_.global_partition_offsets(leaf_id);
    if (!FloatFindNeighbors(query, database, global_offset, distance_measure_,
                            top_n)) {
      return CreateStatusWithPayload(absl::StatusCode::kInternal,
                                     "Nearest neighbor search failed.",
                                     TfLiteSupportStatus::kError);
    }
  }
  return absl::OkStatus();
}

}  // namespace processor
}  // namespace task
}  // namespace tflite
