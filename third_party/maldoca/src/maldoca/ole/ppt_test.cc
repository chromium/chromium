// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "maldoca/ole/ppt.h"

#include "absl/strings/escaping.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "maldoca/base/digest.h"
#include "maldoca/base/file.h"
#include "maldoca/base/testing/status_matchers.h"
#include "maldoca/base/testing/test_utils.h"
#include "maldoca/ole/oss_utils.h"
#include "maldoca/ole/vba_extract.h"

namespace {
using ::maldoca::PPT97ExtractVBAStorage;
using ::maldoca::RecordHeader;
using ::maldoca::VBAProjectStorage;

std::string TestFilename(absl::string_view filename) {
  return maldoca::testing::OleTestFilename(filename, "ole/");
}
std::string GetTestContent(absl::string_view filename) {
  std::string content;
  auto status =
      maldoca::testing::GetTestContents(TestFilename(filename), &content);
  EXPECT_TRUE(status.ok()) << status;
  return content;
}
TEST(RecordHeader, ParseValid) {
  // Get a reference PPT doc
  std::string content = GetTestContent("95dc7b31c9ba45a066f580e6e2c5c914");

  // Get a RecordHeader from the reference doc.  A RecordHeader offset can
  // be found by parsing the PowerPoint Document stream, or searching for a
  // specific RecordHeader byte pattern.
  const size_t kRecordHeaderOffset = 0xa1a4;
  auto header_content = content.substr(kRecordHeaderOffset);

  RecordHeader header;

  // RecordHeader is parsed correctly
  MALDOCA_EXPECT_OK(header.Parse(header_content));

  // Fields are extracted correctly
  EXPECT_EQ(header.Version(), 0);
  EXPECT_EQ(header.Type(), 0x1011);
  EXPECT_EQ(header.Length(), 2361);
  EXPECT_EQ(header.Instance(), 1);
}

TEST(RecordHeader, ParseInvalid) {
  // Read test data
  std::string content = GetTestContent("95dc7b31c9ba45a066f580e6e2c5c914");

  const size_t kCompressedPayloadOffset = 0xa1a4;

  // Parsing should fail when the input string is too small.
  for (size_t i = 1; i < RecordHeader::SizeOf(); ++i) {
    auto too_short = content.substr(kCompressedPayloadOffset, i);
    RecordHeader header;
    ASSERT_FALSE(header.Parse(too_short).ok()) << "Input length: " << i;
  }
}

TEST(VBAProjectStorage, ExtractCompressedStorageValid) {
  // Read test data
  std::string content = GetTestContent("95dc7b31c9ba45a066f580e6e2c5c914");

  const size_t kCompressedPayloadOffset = 0xa1a4;
  auto project_storage = content.substr(kCompressedPayloadOffset);
  MALDOCA_ASSERT_OK_AND_ASSIGN(
      auto uncompressed, VBAProjectStorage::ExtractContent(project_storage));
  // The VBA storage has a DocFile header
  EXPECT_EQ(static_cast<uint8_t>(uncompressed[0]), 0xd0u);
  EXPECT_EQ(static_cast<uint8_t>(uncompressed[1]), 0xcfu);
  EXPECT_EQ(static_cast<uint8_t>(uncompressed[2]), 0x11u);
  EXPECT_EQ(static_cast<uint8_t>(uncompressed[3]), 0xe0u);

  // Check digest
  EXPECT_EQ(maldoca::Sha256HexString(uncompressed),
            "ff08fb1a014e7d860c353fce8313bfd357e5aa5b47ab3fc251e24c8d70fb6737");
}

TEST(VBAProjectStorage, ExtractCompressedStorageInvalid) {
  // Read test data
  std::string content = GetTestContent("95dc7b31c9ba45a066f580e6e2c5c914");

  const size_t kCompressedPayloadOffset = 0xa1a4;

  // Frob reference data: version and instance fields
  std::string frob_version_instance = content.substr(kCompressedPayloadOffset);
  frob_version_instance[0] ^= 0x42;

  // Expect to fail, the header is invalid
  EXPECT_FALSE(VBAProjectStorage::ExtractContent(frob_version_instance).ok());

  // Frob reference data: type field
  std::string frob_type = content.substr(kCompressedPayloadOffset);
  frob_type[2] ^= 0x42;

  // Expect to fail, the header is invalid
  EXPECT_FALSE(VBAProjectStorage::ExtractContent(frob_type).ok());

  // Frob reference data: length field
  std::string frob_length = content.substr(kCompressedPayloadOffset);
  // Intentionally destroying header
  memset(&frob_length[4], 0x42, 4);

  // Expect to fail, the header is invalid
  EXPECT_FALSE(VBAProjectStorage::ExtractContent(frob_length).ok());

  // Make input data too short
  auto more_appropriate_size = content.substr(kCompressedPayloadOffset, 42);
  EXPECT_FALSE(VBAProjectStorage::ExtractContent(more_appropriate_size).ok());
}

TEST(PPT97Document, ExtractVBAStorage) {
  // Read test data
  std::string content = GetTestContent("95dc7b31c9ba45a066f580e6e2c5c914");

  MALDOCA_ASSERT_OK_AND_ASSIGN(auto vba_storage,
                               PPT97ExtractVBAStorage(content));

  // One VBA storage is found
  EXPECT_THAT(vba_storage, testing::SizeIs(1));

  // The VBA storage has a DocFile header
  MALDOCA_EXPECT_OK(VBAProjectStorage::IsContentValid(vba_storage[0]));

  // Check digest
  EXPECT_EQ(maldoca::Sha256HexString(vba_storage[0]),
            "ff08fb1a014e7d860c353fce8313bfd357e5aa5b47ab3fc251e24c8d70fb6737");
}

}  // namespace
