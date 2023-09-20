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

#include "tensorflow_lite_support/scann_ondevice/cc/index.h"

#include <cstddef>
#include <memory>

#include "absl/memory/memory.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/status/statusor.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "absl/strings/string_view.h"  // from @com_google_absl
#include "leveldb/cache.h"  // from @com_google_leveldb
#include "leveldb/iterator.h"  // from @com_google_leveldb
#include "leveldb/options.h"  // from @com_google_leveldb
#include "leveldb/slice.h"  // from @com_google_leveldb
#include "leveldb/status.h"  // from @com_google_leveldb
#include "leveldb/table.h"  // from @com_google_leveldb
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/scann_ondevice/cc/mem_random_access_file.h"
#include "tensorflow_lite_support/scann_ondevice/cc/utils.h"
#include "tensorflow_lite_support/scann_ondevice/proto/index_config.pb.h"

namespace tflite {
namespace scann_ondevice {

namespace {

// Helper function to get the iterator value associated to the provided key.
//
// Important: the underlying storage for the returned string view is owned by
// the provided iterator, and only valid until this iterator is used again with
// a different key. See:
// https://github.com/google/leveldb/blob/main/include/leveldb/iterator.h
absl::StatusOr<absl::string_view> GetValueForKey(leveldb::Iterator* iterator,
                                                 std::string& key) {
  iterator->Seek(key);
  if (!iterator->Valid() || iterator->key() != key ||
      !iterator->status().ok()) {
    return absl::NotFoundError(
        absl::StrFormat("Unable to find key in the index: %s", key));
  }
  leveldb::Slice value = iterator->value();
  return absl::string_view(value.data(), value.size());
}
}  // namespace

/* static */
absl::StatusOr<std::unique_ptr<Index>> Index::CreateFromIndexBuffer(
    const char* buffer_data, size_t buffer_size) {
  // Use absl::WrapUnique() to call private constructor:
  // https://abseil.io/tips/126.
  std::unique_ptr<Index> index = absl::WrapUnique(new Index());
  TFLITE_RETURN_IF_ERROR(index->InitFromBuffer(buffer_data, buffer_size));
  return index;
}

absl::StatusOr<IndexConfig> Index::GetIndexConfig() const {
  std::string key(kIndexConfigKey);
  TFLITE_ASSIGN_OR_RETURN(absl::string_view value,
                   GetValueForKey(config_iterator_.get(), key));
  IndexConfig config;
  if (!config.ParseFromString(std::string(value))) {
    return absl::InternalError("Unable to parse IndexConfig proto");
  }
  return config;
}

absl::StatusOr<absl::string_view> Index::GetUserInfo() const {
  std::string key(kUserInfoKey);
  // Intercept NotFound errors and return empty string instead.
  auto user_info_or = GetValueForKey(info_iterator_.get(), key);
  if (user_info_or.status().code() == absl::StatusCode::kNotFound) {
    return "";
  }
  return user_info_or;
}

absl::StatusOr<absl::string_view> Index::GetPartitionAtIndex(uint32_t i) const {
  std::string key(GetPartitionKey(i));
  return GetValueForKey(embedding_iterator_.get(), key);
}

absl::StatusOr<absl::string_view> Index::GetMetadataAtIndex(uint32_t i) const {
  std::string key(GetMetadataKey(i));
  return GetValueForKey(metadata_iterator_.get(), key);
}

absl::Status Index::InitFromBuffer(const char* buffer_data,
                                   size_t buffer_size) {
  // Sanity check.
  if (buffer_data == nullptr) {
    return absl::InvalidArgumentError("Buffer cannot be null");
  }
  // Create file from buffer.
  file_ = absl::make_unique<MemRandomAccessFile>(buffer_data, buffer_size);
  // Create options with cache disabled, as this saves memory and has negligible
  // impact on performance in this setup as any key can be accessed anytime.
  leveldb::Options options;
  cache_ = absl::WrapUnique(leveldb::NewLRUCache(0));
  options.block_cache = cache_.get();
  // Build Table from file and options.
  leveldb::Table* table;
  leveldb::Status status =
      leveldb::Table::Open(options, file_.get(), buffer_size, &table);
  if (!status.ok()) {
    return absl::InternalError(
        absl::StrFormat("Unable to open levelDB table: %s", status.ToString()));
  }
  table_ = absl::WrapUnique(table);
  // Create iterators.
  config_iterator_ =
      absl::WrapUnique(table_->NewIterator(leveldb::ReadOptions()));
  info_iterator_ =
      absl::WrapUnique(table_->NewIterator(leveldb::ReadOptions()));
  embedding_iterator_ =
      absl::WrapUnique(table_->NewIterator(leveldb::ReadOptions()));
  metadata_iterator_ =
      absl::WrapUnique(table_->NewIterator(leveldb::ReadOptions()));
  return absl::OkStatus();
}

}  // namespace scann_ondevice
}  // namespace tflite
