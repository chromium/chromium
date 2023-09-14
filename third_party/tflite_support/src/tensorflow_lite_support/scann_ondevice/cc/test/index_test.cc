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

#include <cstdint>
#include <memory>

#include "tensorflow_lite_support/scann_ondevice/cc/core/serialized_searcher.pb.h"
#include "absl/flags/flag.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/string_view.h"  // from @com_google_absl
#include "tensorflow/lite/test_util.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/status_matchers.h"
#include "tensorflow_lite_support/cc/task/core/external_file_handler.h"
#include "tensorflow_lite_support/cc/task/core/proto/external_file_proto_inc.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"
#include "tensorflow_lite_support/scann_ondevice/proto/index_config.pb.h"

namespace tflite {
namespace scann_ondevice {
namespace {

using ::absl::StatusCode;
using ::tflite::task::JoinPath;
using ::tflite::task::core::ExternalFile;
using ::tflite::task::core::ExternalFileHandler;

constexpr char kDummyIndexPath[] =
    "/tensorflow_lite_support/scann_ondevice/cc/test/"
    "testdata/dummy_index.ldb";

TEST(CreateFromOptionsTest, Succeeds) {
  // Load file in memory using ExternalFile.
  ExternalFile file;
  file.set_file_name(
      JoinPath("./" /*test src dir*/, kDummyIndexPath));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<ExternalFileHandler> handler,
                       ExternalFileHandler::CreateFromExternalFile(&file));
  absl::string_view file_contents = handler->GetFileContent();

  SUPPORT_EXPECT_OK(
      Index::CreateFromIndexBuffer(file_contents.data(), file_contents.size()));
}

class IndexTest : public tflite::testing::Test {
 public:
  IndexTest() {
    // Load file in memory using ExternalFile.
    ExternalFile file;
    file.set_file_name(
        JoinPath("./" /*test src dir*/, kDummyIndexPath));
    handler_ = ExternalFileHandler::CreateFromExternalFile(&file).value();
    absl::string_view file_contents = handler_->GetFileContent();
    // Build index.
    index_ =
        Index::CreateFromIndexBuffer(file_contents.data(), file_contents.size())
            .value();
  }

 protected:
  std::unique_ptr<Index> index_;

 private:
  std::unique_ptr<ExternalFileHandler> handler_;
};

TEST_F(IndexTest, GetIndexConfigSucceeds) {
  auto index_config_or = index_->GetIndexConfig();
  SUPPORT_EXPECT_OK(index_config_or);
  auto index_config = index_config_or.value();
  EXPECT_EQ(index_config.scann_config().partitioner().search_fraction(), 0.5);
  EXPECT_EQ(index_config.embedding_type(), IndexConfig::UINT8);
  EXPECT_EQ(index_config.embedding_dim(), 4);
  EXPECT_EQ(index_config.global_partition_offsets_size(), 2);
  EXPECT_EQ(index_config.global_partition_offsets(0), 0);
  EXPECT_EQ(index_config.global_partition_offsets(1), 2);
}

TEST_F(IndexTest, GetUserInfoSucceeds) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(auto userinfo, index_->GetUserInfo());
  EXPECT_EQ(userinfo, "user info");
}

TEST_F(IndexTest, GetPartitionAtIndexSucceeds) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(absl::string_view partition_0,
                       index_->GetPartitionAtIndex(0));
  EXPECT_EQ(partition_0.size(), 8);
  const uint8_t *partition =
      reinterpret_cast<const uint8_t *>(partition_0.data());
  for (int i = 0; i < 8; ++i) {
    EXPECT_EQ(partition[i], i);
  }

  SUPPORT_ASSERT_OK_AND_ASSIGN(absl::string_view partition_1,
                       index_->GetPartitionAtIndex(1));
  EXPECT_EQ(partition_1.size(), 4);
  partition = reinterpret_cast<const uint8_t *>(partition_1.data());
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(partition[i], i + 8);
  }
}

TEST_F(IndexTest, GetPartitionAtIndexFailsOutOfBounds) {
  EXPECT_EQ(index_->GetPartitionAtIndex(2).status().code(),
            StatusCode::kNotFound);
}

TEST_F(IndexTest, GetMetadataAtIndexSucceeds) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(absl::string_view metadata_0,
                       index_->GetMetadataAtIndex(0));
  EXPECT_EQ(metadata_0, "metadata_0");

  SUPPORT_ASSERT_OK_AND_ASSIGN(absl::string_view metadata_1,
                       index_->GetMetadataAtIndex(1));
  EXPECT_EQ(metadata_1, "metadata_1");

  SUPPORT_ASSERT_OK_AND_ASSIGN(absl::string_view metadata_2,
                       index_->GetMetadataAtIndex(2));
  EXPECT_EQ(metadata_2, "metadata_2");
}

TEST_F(IndexTest, GetMetadataAtIndexFailsOutOfBounds) {
  EXPECT_EQ(index_->GetMetadataAtIndex(3).status().code(),
            StatusCode::kNotFound);
}

}  // namespace
}  // namespace scann_ondevice
}  // namespace tflite
