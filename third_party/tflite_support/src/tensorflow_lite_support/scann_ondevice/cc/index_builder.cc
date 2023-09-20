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

#include "tensorflow_lite_support/scann_ondevice/cc/index_builder.h"

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include "absl/container/btree_map.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "leveldb/options.h"  // from @com_google_leveldb
#include "leveldb/slice.h"  // from @com_google_leveldb
#include "leveldb/status.h"  // from @com_google_leveldb
#include "leveldb/table_builder.h"  // from @com_google_leveldb
#include "leveldb/write_batch.h"  // from @com_google_leveldb
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/scann_ondevice/cc/mem_writable_file.h"
#include "tensorflow_lite_support/scann_ondevice/cc/utils.h"
#include "tensorflow_lite_support/scann_ondevice/proto/index_config.pb.h"

namespace tflite {
namespace scann_ondevice {

namespace {

absl::Status LevelDBStatusToAbsl(leveldb::Status leveldb_status) {
  if (leveldb_status.ok()) {
    return absl::OkStatus();
  } else if (leveldb_status.IsInvalidArgument()) {
    return absl::InvalidArgumentError(leveldb_status.ToString());
  } else if (leveldb_status.IsNotFound()) {
    return absl::NotFoundError(leveldb_status.ToString());
  } else if (leveldb_status.IsNotSupportedError()) {
    return absl::UnimplementedError(leveldb_status.ToString());
  } else {
    return absl::InternalError(leveldb_status.ToString());
  }
}

template <typename T>
absl::StatusOr<std::string> CreateIndexBufferImpl(
    absl::Span<const T> database,
    absl::optional<absl::Span<const uint32_t>> partition_assignment,
    absl::Span<const std::string> metadata, const std::string& userinfo,
    IndexConfig index_config, bool compression) {
  size_t num_partitions = 1;
  if (partition_assignment) {
    if (partition_assignment->size() != metadata.size()) {
      return absl::InvalidArgumentError(
          "Size of partition assignment and metadata mismatch");
    }
    num_partitions = index_config.scann_config().partitioner().leaf_size();
  }

  if (database.size() / index_config.embedding_dim() != metadata.size()) {
    return absl::InvalidArgumentError(
        "Number of embeddings differs from number of metadata");
  }

  std::vector<std::vector<char>> partition_bytes(num_partitions);
  std::vector<std::vector<std::string>> partition_metadata(num_partitions);

  const size_t per_embedding_bytes = sizeof(T) * index_config.embedding_dim();
  const char* database_bytes = reinterpret_cast<const char*>(database.data());
  for (size_t i = 0; i < metadata.size(); ++i) {
    const size_t partition_idx =
        partition_assignment ? (*partition_assignment)[i] : 0;
    if (partition_idx >= num_partitions) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Partition index %d is larger than number of partitions: %d",
          partition_idx, num_partitions));
    }
    partition_bytes[partition_idx].insert(
        partition_bytes[partition_idx].end(),
        database_bytes + i * per_embedding_bytes,
        database_bytes + (i + 1) * per_embedding_bytes);
    partition_metadata[partition_idx].push_back(metadata[i]);
  }

  std::vector<std::string> flatten_metadata;
  flatten_metadata.reserve(metadata.size());
  for (auto partition : partition_metadata) {
    const size_t offset = flatten_metadata.size();
    index_config.mutable_global_partition_offsets()->Add(offset);
    flatten_metadata.insert(flatten_metadata.end(), partition.begin(),
                            partition.end());
    partition.clear();
    partition.shrink_to_fit();
  }

  std::string buffer;
  TFLITE_ASSIGN_OR_RETURN(auto mem_writable_file, MemWritableFile::Create(&buffer));

  leveldb::Options options;
  options.compression =
      compression ? leveldb::kSnappyCompression : leveldb::kNoCompression;
  leveldb::TableBuilder table_builder(options, mem_writable_file.get());

  // Keys must be added in ascending *lexical* order, e.g:
  // E_0, E_1, E_10, E_11, [...], E_18, E_19, E_2, E_20, E_21, [...]
  // We're using btree_map to reorder partition and metadata keys.
  absl::btree_map<std::string, size_t> ordered_partition_key_to_index;
  for (size_t i = 0; i < partition_bytes.size(); ++i) {
    ordered_partition_key_to_index[GetPartitionKey(i)] = i;
  }
  for (auto [key, index] : ordered_partition_key_to_index) {
    table_builder.Add(leveldb::Slice(key),
                      leveldb::Slice(partition_bytes[index].data(),
                                     partition_bytes[index].size()));
  }
  table_builder.Add(leveldb::Slice(kIndexConfigKey),
                    leveldb::Slice(index_config.SerializeAsString()));
  absl::btree_map<std::string, size_t> ordered_metadata_key_to_index;
  for (size_t i = 0; i < flatten_metadata.size(); ++i) {
    ordered_metadata_key_to_index[GetMetadataKey(i)] = i;
  }
  for (auto [key, index] : ordered_metadata_key_to_index) {
    table_builder.Add(leveldb::Slice(key),
                      leveldb::Slice(flatten_metadata[index]));
  }
  table_builder.Add(leveldb::Slice(kUserInfoKey), leveldb::Slice(userinfo));

  const auto status = table_builder.Finish();
  if (!status.ok()) {
    return LevelDBStatusToAbsl(status);
  }

  return buffer;
}

}  // namespace

absl::StatusOr<std::string> CreateIndexBuffer(
    const IndexedArtifacts& artifacts, bool compression) {
  if (artifacts.hashed_database.has_value() &&
      artifacts.float_database.has_value()) {
    return absl::InvalidArgumentError(
        "Can not have both float database and hashed database");
  }

  IndexConfig index_config;
  *index_config.mutable_scann_config() = artifacts.config;
  index_config.set_embedding_dim(artifacts.embedding_dim);
  if (artifacts.hashed_database.has_value()) {
    index_config.set_embedding_type(index_config.UINT8);
    return CreateIndexBufferImpl(artifacts.hashed_database.value(),
                                 artifacts.partition_assignment,
                                 artifacts.metadata, artifacts.userinfo,
                                 std::move(index_config), compression);
  } else if (artifacts.float_database.has_value()) {
    index_config.set_embedding_type(index_config.FLOAT);
    return CreateIndexBufferImpl(artifacts.float_database.value(),
                                 artifacts.partition_assignment,
                                 artifacts.metadata, artifacts.userinfo,
                                 std::move(index_config), compression);
  } else {
    return absl::InvalidArgumentError(
        "Need either hashed_database or float_database");
  }
}

}  // namespace scann_ondevice
}  // namespace tflite
