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

#include "absl/flags/flag.h"  // from @com_google_absl
#include "absl/memory/memory.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "absl/strings/string_view.h"  // from @com_google_absl
#include "absl/types/span.h"  // from @com_google_absl
#include "leveldb/env.h"  // from @com_google_leveldb
#include "leveldb/iterator.h"  // from @com_google_leveldb
#include "leveldb/options.h"  // from @com_google_leveldb
#include "leveldb/slice.h"  // from @com_google_leveldb
#include "leveldb/status.h"  // from @com_google_leveldb
#include "leveldb/table.h"  // from @com_google_leveldb
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/status_matchers.h"
#include "tensorflow_lite_support/cc/test/message_matchers.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"
#include "tensorflow_lite_support/scann_ondevice/proto/index_config.pb.h"

namespace tflite {
namespace scann_ondevice {
namespace {

using ::testing::Bool;
using ::testing::ElementsAreArray;
using ::testing::TestWithParam;
using ::tflite::support::EqualsProto;
using ::tflite::task::ParseTextProtoOrDie;

absl::Status SetContents(absl::string_view file_name,
                         absl::string_view content) {
  FILE* fp = fopen(file_name.data(), "w");
  if (fp == NULL) {
    return absl::InternalError(
        absl::StrFormat("Can't open file: %s", file_name));
  }

  fwrite(content.data(), sizeof(char), content.size(), fp);
  size_t write_error = ferror(fp);
  if (fclose(fp) != 0 || write_error) {
    return absl::InternalError(
        absl::StrFormat("Error while writing file: %s. Error message: %s",
                        file_name, strerror(write_error)));
  }
  return absl::OkStatus();
}

absl::StatusOr<std::string> LookupKey(leveldb::Iterator* iterator,
                                      absl::string_view key) {
  iterator->Seek({key.data(), key.size()});
  if (!iterator->Valid() || iterator->key().ToString() != key ||
      !iterator->status().ok()) {
    return absl::NotFoundError("Failed to lookup key");
  }
  return iterator->value().ToString();
}

constexpr size_t kDimensions = 2;
constexpr size_t kNumEmbeddings = 24;
constexpr size_t kNumPartitions = 12;

IndexConfig CreateExpectedConfigWithPartitioner(
    IndexConfig::Type embedding_type) {
  IndexConfig config = ParseTextProtoOrDie<IndexConfig>(R"pb(
    scann_config {
      partitioner {
        leaf { dimension: 0 dimension: 0 }
        leaf { dimension: 1 dimension: 1 }
        leaf { dimension: 2 dimension: 2 }
        leaf { dimension: 3 dimension: 3 }
        leaf { dimension: 4 dimension: 4 }
        leaf { dimension: 5 dimension: 5 }
        leaf { dimension: 6 dimension: 6 }
        leaf { dimension: 7 dimension: 7 }
        leaf { dimension: 8 dimension: 8 }
        leaf { dimension: 9 dimension: 9 }
        leaf { dimension: 10 dimension: 10 }
        leaf { dimension: 11 dimension: 11 }
      }
    }
    embedding_dim: 2
    embedding_type: UINT8
    global_partition_offsets: 0
    global_partition_offsets: 2
    global_partition_offsets: 4
    global_partition_offsets: 6
    global_partition_offsets: 8
    global_partition_offsets: 10
    global_partition_offsets: 12
    global_partition_offsets: 14
    global_partition_offsets: 16
    global_partition_offsets: 18
    global_partition_offsets: 20
    global_partition_offsets: 22
  )pb");
  config.set_embedding_type(embedding_type);
  return config;
}

IndexConfig CreateExpectedConfigWithoutPartitioner(
    IndexConfig::Type embedding_type) {
  IndexConfig config = ParseTextProtoOrDie<IndexConfig>(R"pb(
    scann_config { query_distance: SQUARED_L2_DISTANCE }
    embedding_dim: 2
    global_partition_offsets: 0
  )pb");
  config.set_embedding_type(embedding_type);
  return config;
}

class PopulateIndexFileTest : public TestWithParam<bool /*compression*/> {};

TEST_P(PopulateIndexFileTest, WritesHashedDatabaseWithPartitioner) {
  const std::string db_path =
      tflite::task::JoinPath(getenv("TEST_TMPDIR"), "hashed");
  const bool compression = GetParam();

  {
    tflite::scann_ondevice::core::ScannOnDeviceConfig config =
        ParseTextProtoOrDie<tflite::scann_ondevice::core::ScannOnDeviceConfig>(R"pb(
          partitioner: {
            leaf { dimension: 0 dimension: 0 }
            leaf { dimension: 1 dimension: 1 }
            leaf { dimension: 2 dimension: 2 }
            leaf { dimension: 3 dimension: 3 }
            leaf { dimension: 4 dimension: 4 }
            leaf { dimension: 5 dimension: 5 }
            leaf { dimension: 6 dimension: 6 }
            leaf { dimension: 7 dimension: 7 }
            leaf { dimension: 8 dimension: 8 }
            leaf { dimension: 9 dimension: 9 }
            leaf { dimension: 10 dimension: 10 }
            leaf { dimension: 11 dimension: 11 }
          }
        )pb");
    std::vector<uint8_t> hashed_database;
    hashed_database.reserve(kNumEmbeddings * kDimensions);
    for (int i = 0; i < kNumEmbeddings; ++i) {
      for (int j = 0; j < kDimensions; ++j) {
        hashed_database.push_back(i);
      }
    }
    std::vector<uint32_t> partition_assignment;
    partition_assignment.reserve(kNumEmbeddings);
    for (int i = 0; i < kNumEmbeddings; ++i) {
      partition_assignment.push_back(i % kNumPartitions);
    }
    std::vector<std::string> metadata;
    metadata.reserve(kNumEmbeddings);
    for (int i = 0; i < kNumEmbeddings; ++i) {
      metadata.push_back(absl::StrFormat("%d", i));
    }
    SUPPORT_ASSERT_OK_AND_ASSIGN(
        const std::string buffer,
        CreateIndexBuffer(
            {.config = config,
             .embedding_dim = kDimensions,
             .hashed_database = absl::Span<uint8_t>(hashed_database),
             .partition_assignment = absl::Span<uint32_t>(partition_assignment),
             .metadata = absl::Span<std::string>(metadata),
             .userinfo = "hashed_userinfo"},
            compression));
    SUPPORT_ASSERT_OK(SetContents(db_path, buffer));
  }

  auto* env = leveldb::Env::Default();
  leveldb::RandomAccessFile* hash_file;
  size_t hash_file_size;
  ASSERT_TRUE(env->NewRandomAccessFile(db_path, &hash_file).ok());
  auto hashed_file_unique = absl::WrapUnique(hash_file);
  ASSERT_TRUE(env->GetFileSize(db_path, &hash_file_size).ok());

  leveldb::Options options;
  options.compression =
      compression ? leveldb::kSnappyCompression : leveldb::kNoCompression;

  leveldb::Table* hashed_table;
  ASSERT_TRUE(
      leveldb::Table::Open(options, hash_file, hash_file_size, &hashed_table)
          .ok());
  auto hashed_table_unique = absl::WrapUnique(hashed_table);
  auto hashed_table_iterator =
      absl::WrapUnique(hashed_table->NewIterator(leveldb::ReadOptions()));

  SUPPORT_ASSERT_OK_AND_ASSIGN(std::string serialized_config,
                       LookupKey(hashed_table_iterator.get(), "INDEX_CONFIG"));
  IndexConfig index_config;
  EXPECT_TRUE(index_config.ParseFromString(serialized_config));
  EXPECT_THAT(
      index_config,
      EqualsProto(CreateExpectedConfigWithPartitioner(IndexConfig::UINT8)));

  SUPPORT_ASSERT_OK_AND_ASSIGN(std::string userinfo,
                       LookupKey(hashed_table_iterator.get(), "USER_INFO"));
  EXPECT_EQ(userinfo, "hashed_userinfo");

  // Partition assignment is based on i % kNumPartitions, so:
  // * partition 0 contains embeddings 0 and 12,
  // * partition 1 contains embeddings 1 and 13,
  // * etc
  for (int i = 0; i < kNumPartitions; ++i) {
    SUPPORT_ASSERT_OK_AND_ASSIGN(
        std::string raw_partition_hashed,
        LookupKey(hashed_table_iterator.get(), absl::StrFormat("E_%d", i)));
    std::vector<char> hashed_partition(raw_partition_hashed.begin(),
                                       raw_partition_hashed.end());
    std::vector<char> expected = {static_cast<char>(i), static_cast<char>(i),
                                  static_cast<char>(i + kNumPartitions),
                                  static_cast<char>(i + kNumPartitions)};
    EXPECT_THAT(hashed_partition, ElementsAreArray(expected));
  }

  // Similarly:
  // * metadata 0 contains metadata 0,
  // * metadata 1 contains metadata 12,
  // * metadata 2 contains metadata 1,
  // * metadata 3 contains metadata 13,
  // * etc
  // Hence the `i / 2 + (i % 2 ? kNumPartitions : 0)` formula here.
  for (int i = 0; i < kNumEmbeddings; ++i) {
    SUPPORT_ASSERT_OK_AND_ASSIGN(
        std::string metadata,
        LookupKey(hashed_table_iterator.get(), absl::StrFormat("M_%d", i)));
    EXPECT_EQ(metadata,
              absl::StrFormat("%d", i / 2 + (i % 2 ? kNumPartitions : 0)));
  }
}

TEST_P(PopulateIndexFileTest, WritesHashedDatabaseWithoutPartitioner) {
  const std::string db_path =
      tflite::task::JoinPath(getenv("TEST_TMPDIR"), "float");
  const bool compression = GetParam();

  {
    tflite::scann_ondevice::core::ScannOnDeviceConfig config =
        ParseTextProtoOrDie<tflite::scann_ondevice::core::ScannOnDeviceConfig>(R"pb(
          query_distance: SQUARED_L2_DISTANCE
        )pb");
    std::vector<uint8_t> hashed_database;
    hashed_database.reserve(kNumEmbeddings * kDimensions);
    for (int i = 0; i < kNumEmbeddings; ++i) {
      for (int j = 0; j < kDimensions; ++j) {
        hashed_database.push_back(i);
      }
    }
    std::vector<std::string> metadata;
    metadata.reserve(kNumEmbeddings);
    for (int i = 0; i < kNumEmbeddings; ++i) {
      metadata.push_back(absl::StrFormat("%d", i));
    }
    SUPPORT_ASSERT_OK_AND_ASSIGN(
        const std::string buffer,
        CreateIndexBuffer(
            {.config = config,
             .embedding_dim = kDimensions,
             .hashed_database = absl::Span<uint8_t>(hashed_database),
             .metadata = absl::Span<std::string>(metadata),
             .userinfo = "hashed_userinfo"},
            compression));
    SUPPORT_ASSERT_OK(SetContents(db_path, buffer));
  }

  auto* env = leveldb::Env::Default();
  leveldb::RandomAccessFile* float_file;
  size_t float_file_size;
  ASSERT_TRUE(env->NewRandomAccessFile(db_path, &float_file).ok());
  auto float_file_unique = absl::WrapUnique(float_file);
  ASSERT_TRUE(env->GetFileSize(db_path, &float_file_size).ok());

  leveldb::Options options;
  options.compression =
      compression ? leveldb::kSnappyCompression : leveldb::kNoCompression;

  leveldb::Table* float_table;
  ASSERT_TRUE(
      leveldb::Table::Open(options, float_file, float_file_size, &float_table)
          .ok());
  auto float_table_unique = absl::WrapUnique(float_table);
  auto float_table_iterator =
      absl::WrapUnique(float_table->NewIterator(leveldb::ReadOptions()));

  SUPPORT_ASSERT_OK_AND_ASSIGN(std::string serialized_config,
                       LookupKey(float_table_iterator.get(), "INDEX_CONFIG"));
  IndexConfig index_config;
  EXPECT_TRUE(index_config.ParseFromString(serialized_config));
  EXPECT_THAT(
      index_config,
      EqualsProto(CreateExpectedConfigWithoutPartitioner(IndexConfig::UINT8)));

  SUPPORT_ASSERT_OK_AND_ASSIGN(std::string userinfo,
                       LookupKey(float_table_iterator.get(), "USER_INFO"));
  EXPECT_EQ(userinfo, "hashed_userinfo");

  // Check that the unique embedding partition has the exact same contents as
  // the database used at construction time.
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::string raw_partition_hashed,
                       LookupKey(float_table_iterator.get(), "E_0"));
  std::vector<char> hashed_partition(raw_partition_hashed.begin(),
                                     raw_partition_hashed.end());
  std::vector<char> expected;
  expected.reserve(kNumEmbeddings * kDimensions);
  for (int i = 0; i < kNumEmbeddings; ++i) {
    for (int j = 0; j < kDimensions; ++j) {
      expected.push_back(i);
    }
  }
  EXPECT_THAT(hashed_partition, ElementsAreArray(expected));

  // Check metadata.
  for (int i = 0; i < kNumEmbeddings; ++i) {
    SUPPORT_ASSERT_OK_AND_ASSIGN(
        std::string metadata,
        LookupKey(float_table_iterator.get(), absl::StrFormat("M_%d", i)));
    EXPECT_EQ(metadata, absl::StrFormat("%d", i));
  }
}

TEST_P(PopulateIndexFileTest, WritesFloatDatabaseWithPartitioner) {
  const std::string db_path =
      tflite::task::JoinPath(getenv("TEST_TMPDIR"), "float");
  const bool compression = GetParam();

  {
    tflite::scann_ondevice::core::ScannOnDeviceConfig config =
        ParseTextProtoOrDie<tflite::scann_ondevice::core::ScannOnDeviceConfig>(R"pb(
          partitioner: {
            leaf { dimension: 0 dimension: 0 }
            leaf { dimension: 1 dimension: 1 }
            leaf { dimension: 2 dimension: 2 }
            leaf { dimension: 3 dimension: 3 }
            leaf { dimension: 4 dimension: 4 }
            leaf { dimension: 5 dimension: 5 }
            leaf { dimension: 6 dimension: 6 }
            leaf { dimension: 7 dimension: 7 }
            leaf { dimension: 8 dimension: 8 }
            leaf { dimension: 9 dimension: 9 }
            leaf { dimension: 10 dimension: 10 }
            leaf { dimension: 11 dimension: 11 }
          }
        )pb");
    std::vector<float> float_database;
    float_database.reserve(kNumEmbeddings * kDimensions);
    for (int i = 0; i < kNumEmbeddings; ++i) {
      for (int j = 0; j < kDimensions; ++j) {
        float_database.push_back(i);
      }
    }
    std::vector<uint32_t> partition_assignment;
    partition_assignment.reserve(kNumEmbeddings);
    for (int i = 0; i < kNumEmbeddings; ++i) {
      partition_assignment.push_back(i % kNumPartitions);
    }
    std::vector<std::string> metadata;
    metadata.reserve(kNumEmbeddings);
    for (int i = 0; i < kNumEmbeddings; ++i) {
      metadata.push_back(absl::StrFormat("%d", i));
    }
    SUPPORT_ASSERT_OK_AND_ASSIGN(
        const std::string buffer,
        CreateIndexBuffer(
            {.config = config,
             .embedding_dim = kDimensions,
             .float_database = absl::Span<float>(float_database),
             .partition_assignment = absl::Span<uint32_t>(partition_assignment),
             .metadata = absl::Span<std::string>(metadata),
             .userinfo = "float_userinfo"},
            compression));
    SUPPORT_ASSERT_OK(SetContents(db_path, buffer));
  }

  auto* env = leveldb::Env::Default();
  leveldb::RandomAccessFile* float_file;
  size_t float_file_size;
  ASSERT_TRUE(env->NewRandomAccessFile(db_path, &float_file).ok());
  auto float_file_unique = absl::WrapUnique(float_file);
  ASSERT_TRUE(env->GetFileSize(db_path, &float_file_size).ok());

  leveldb::Options options;
  options.compression =
      compression ? leveldb::kSnappyCompression : leveldb::kNoCompression;

  leveldb::Table* float_table;
  ASSERT_TRUE(
      leveldb::Table::Open(options, float_file, float_file_size, &float_table)
          .ok());
  auto float_table_unique = absl::WrapUnique(float_table);
  auto float_table_iterator =
      absl::WrapUnique(float_table->NewIterator(leveldb::ReadOptions()));

  SUPPORT_ASSERT_OK_AND_ASSIGN(std::string serialized_config,
                       LookupKey(float_table_iterator.get(), "INDEX_CONFIG"));
  IndexConfig index_config;
  EXPECT_TRUE(index_config.ParseFromString(serialized_config));
  EXPECT_THAT(
      index_config,
      EqualsProto(CreateExpectedConfigWithPartitioner(IndexConfig::FLOAT)));

  SUPPORT_ASSERT_OK_AND_ASSIGN(std::string userinfo,
                       LookupKey(float_table_iterator.get(), "USER_INFO"));
  EXPECT_EQ(userinfo, "float_userinfo");

  // Partition assignment is based on i % kNumPartitions, so:
  // * partition 0 contains embeddings 0 and 12,
  // * partition 1 contains embeddings 1 and 13,
  // * etc
  for (int i = 0; i < kNumPartitions; ++i) {
    SUPPORT_ASSERT_OK_AND_ASSIGN(
        std::string raw_partition_float,
        LookupKey(float_table_iterator.get(), absl::StrFormat("E_%d", i)));
    const float* raw_partition_float_ptr =
        reinterpret_cast<const float*>(raw_partition_float.data());
    std::vector<float> float_partition(
        raw_partition_float_ptr,
        raw_partition_float_ptr + raw_partition_float.size() / sizeof(float));
    std::vector<float> expected = {static_cast<float>(i), static_cast<float>(i),
                                   static_cast<float>(i + kNumPartitions),
                                   static_cast<float>(i + kNumPartitions)};
    EXPECT_THAT(float_partition, ElementsAreArray(expected));
  }

  // Similarly:
  // * metadata 0 contains metadata 0,
  // * metadata 1 contains metadata 12,
  // * metadata 2 contains metadata 1,
  // * metadata 3 contains metadata 13,
  // * etc
  // Hence the `i / 2 + (i % 2 ? kNumPartitions : 0)` formula here.
  for (int i = 0; i < kNumEmbeddings; ++i) {
    SUPPORT_ASSERT_OK_AND_ASSIGN(
        std::string metadata,
        LookupKey(float_table_iterator.get(), absl::StrFormat("M_%d", i)));
    EXPECT_EQ(metadata,
              absl::StrFormat("%d", i / 2 + (i % 2 ? kNumPartitions : 0)));
  }
}

TEST_P(PopulateIndexFileTest, WritesFloatDatabaseWithoutPartitioner) {
  const std::string db_path =
      tflite::task::JoinPath(getenv("TEST_TMPDIR"), "float");
  const bool compression = GetParam();

  {
    tflite::scann_ondevice::core::ScannOnDeviceConfig config =
        ParseTextProtoOrDie<tflite::scann_ondevice::core::ScannOnDeviceConfig>(R"pb(
          query_distance: SQUARED_L2_DISTANCE
        )pb");
    std::vector<float> float_database;
    float_database.reserve(kNumEmbeddings * kDimensions);
    for (int i = 0; i < kNumEmbeddings; ++i) {
      for (int j = 0; j < kDimensions; ++j) {
        float_database.push_back(i);
      }
    }
    std::vector<std::string> metadata;
    metadata.reserve(kNumEmbeddings);
    for (int i = 0; i < kNumEmbeddings; ++i) {
      metadata.push_back(absl::StrFormat("%d", i));
    }
    SUPPORT_ASSERT_OK_AND_ASSIGN(
        const std::string buffer,
        CreateIndexBuffer({.config = config,
                           .embedding_dim = kDimensions,
                           .float_database = absl::Span<float>(float_database),
                           .metadata = absl::Span<std::string>(metadata),
                           .userinfo = "float_userinfo"},
                          compression));
    SUPPORT_ASSERT_OK(SetContents(db_path, buffer));
  }

  auto* env = leveldb::Env::Default();
  leveldb::RandomAccessFile* float_file;
  size_t float_file_size;
  ASSERT_TRUE(env->NewRandomAccessFile(db_path, &float_file).ok());
  auto float_file_unique = absl::WrapUnique(float_file);
  ASSERT_TRUE(env->GetFileSize(db_path, &float_file_size).ok());

  leveldb::Options options;
  options.compression =
      compression ? leveldb::kSnappyCompression : leveldb::kNoCompression;

  leveldb::Table* float_table;
  ASSERT_TRUE(
      leveldb::Table::Open(options, float_file, float_file_size, &float_table)
          .ok());
  auto float_table_unique = absl::WrapUnique(float_table);
  auto float_table_iterator =
      absl::WrapUnique(float_table->NewIterator(leveldb::ReadOptions()));

  SUPPORT_ASSERT_OK_AND_ASSIGN(std::string serialized_config,
                       LookupKey(float_table_iterator.get(), "INDEX_CONFIG"));
  IndexConfig index_config;
  EXPECT_TRUE(index_config.ParseFromString(serialized_config));
  EXPECT_THAT(
      index_config,
      EqualsProto(CreateExpectedConfigWithoutPartitioner(IndexConfig::FLOAT)));

  SUPPORT_ASSERT_OK_AND_ASSIGN(std::string userinfo,
                       LookupKey(float_table_iterator.get(), "USER_INFO"));
  EXPECT_EQ(userinfo, "float_userinfo");

  // Check that the unique embedding partition has the exact same contents as
  // the database used at construction time.
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::string raw_partition_float,
                       LookupKey(float_table_iterator.get(), "E_0"));
  const float* raw_partition_float_ptr =
      reinterpret_cast<const float*>(raw_partition_float.data());
  std::vector<float> float_partition(
      raw_partition_float_ptr,
      raw_partition_float_ptr + raw_partition_float.size() / sizeof(float));
  std::vector<float> expected;
  expected.reserve(kNumEmbeddings * kDimensions);
  for (int i = 0; i < kNumEmbeddings; ++i) {
    for (int j = 0; j < kDimensions; ++j) {
      expected.push_back(i);
    }
  }
  EXPECT_THAT(float_partition, ElementsAreArray(expected));

  // Check metadata.
  for (int i = 0; i < kNumEmbeddings; ++i) {
    SUPPORT_ASSERT_OK_AND_ASSIGN(
        std::string metadata,
        LookupKey(float_table_iterator.get(), absl::StrFormat("M_%d", i)));
    EXPECT_EQ(metadata, absl::StrFormat("%d", i));
  }
}

INSTANTIATE_TEST_SUITE_P(PopulateIndexFileTest, PopulateIndexFileTest, Bool());

}  // namespace
}  // namespace scann_ondevice
}  // namespace tflite
