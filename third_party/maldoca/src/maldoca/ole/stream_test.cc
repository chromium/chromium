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

// Tests for the OLEStream class methods.

#include "maldoca/ole/stream.h"

#include "absl/strings/str_cat.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "maldoca/base/digest.h"
#include "maldoca/base/file.h"
#include "maldoca/base/testing/status_matchers.h"
#include "maldoca/base/testing/test_utils.h"
#include "maldoca/ole/dir.h"
#include "maldoca/ole/fat.h"
#include "maldoca/ole/header.h"
#include "maldoca/ole/oss_utils.h"

using ::maldoca::DirectoryStorageType;
using ::maldoca::FAT;
using ::maldoca::OLEDirectoryEntry;
using ::maldoca::OLEHeader;
using ::maldoca::OLEStream;

namespace {
std::string TestFilename(absl::string_view filename) {
  return maldoca::testing::OleTestFilename(filename);
}

std::string GetTestContent(absl::string_view filename) {
  std::string content;
  auto status =
      maldoca::testing::GetTestContents(TestFilename(filename), &content);
  MALDOCA_EXPECT_OK(status) << status;
  return content;
}

class StreamTest : public testing::Test {
 protected:
  void SetUp() override {
    input = GetTestContent("vba1_xor_0x42_encoded.bin");
    EXPECT_TRUE(OLEHeader::ParseHeader(input, &header));
    EXPECT_TRUE(header.IsInitialized());
    EXPECT_TRUE(FAT::Read(input, header, &fat));
    EXPECT_FALSE(fat.empty());
    std::vector<OLEDirectoryEntry *> dir_entries;
    std::string directory_stream;
    EXPECT_TRUE(OLEDirectoryEntry::ReadDirectory(
        input, header, fat, &root, &dir_entries, &directory_stream));
    EXPECT_TRUE(root.IsInitialized());
    vba_root = root.FindVBAContentRoot();
    EXPECT_NE(vba_root, nullptr);
    vba_dir = vba_root->FindChildByName("vba", DirectoryStorageType::Storage)
                  ->FindChildByName("dir", DirectoryStorageType::Stream);
    EXPECT_NE(vba_dir, nullptr);
  }

  std::string input;
  OLEHeader header;
  std::vector<uint32_t> fat;
  OLEDirectoryEntry root;
  OLEDirectoryEntry *vba_root;
  OLEDirectoryEntry *vba_dir;
};

TEST(StreamDecompression, BogusInputTest) {
  std::string output;
  ASSERT_DEATH(OLEStream::DecompressStream("", &output),
               "Check failed: !input_string\\.empty\\(\\)");
  output = "content";
  ASSERT_DEATH(OLEStream::DecompressStream("abc", &output),
               "Check failed: output->empty\\(\\)");

  // TODO(somebody) So here we should really inspect what's logged
  // to make sure the right code path are activated. This requires
  // that we use ScopedMockLog. For now, we're just going to manually
  // verify the log output as we write the test cases.

  // First byte isn't 0x01
  output.clear();
  EXPECT_FALSE(OLEStream::DecompressStream("abc", &output));
  // Can't read header past first byte of value 0x01
  EXPECT_FALSE(OLEStream::DecompressStream("\x01\x02", &output));
  // Chunk signature isn't 0x03
  EXPECT_FALSE(
      OLEStream::DecompressStream(std::string("\x01\x00\x40", 3), &output));
  // Unexpected chunk_size for chunk_flag of 1
  //
  // When strictly reading a uint16_t, a chunk size greater than 4098
  // can not be crafted with a valid signature of 0x03 (which we check
  // for first.) We can't test that but the code is ready to handle
  // the situation.

  // Unxpected chunk_size for chunk_flag of 0 (chunk_size must be 4098)
  EXPECT_FALSE(OLEStream::DecompressStream("\x01\xfe\x3f", &output));
  // Advertise 2001 characters in input when only 3 are available.
  EXPECT_FALSE(OLEStream::DecompressStream("\x01\xcd\xb7", &output));
}

TEST_F(StreamTest, GoodInputTest) {
  // There's no compression: chunk_flag = 0, chunk_size must be 4098,
  // which corresponds to 4096 bytes of data that wasn't compressed.
  std::string repeated_input(4096, 'a');
  std::string output;
  EXPECT_TRUE(OLEStream::DecompressStream(
      absl::StrCat("\x01\xff\x3f", repeated_input), &output));
  EXPECT_EQ(repeated_input, output);

  // Very simple compressed stream: the flag at the fourth byte
  // position indicates that the next eight bytes are just to be added
  // to the output.
  std::string output2;
  EXPECT_TRUE(OLEStream::DecompressStream(std::string("\x01\x08\xb0\x00"
                                                      "\x01\x02\x03\x04"
                                                      "\x05\x06\x07\x08",
                                                      12),
                                          &output2));
  EXPECT_EQ(output2, "\x01\x02\x03\x04\x05\x06\x07\x08");

  // Now for some compressed input. We're just going after some
  // existing content and independently verified output.
  std::string compressed_dir_stream, inflated_dir_stream;
  EXPECT_TRUE(OLEStream::ReadDirectoryContent(input, header, *vba_dir, fat,
                                              &compressed_dir_stream));
  EXPECT_TRUE(
      OLEStream::DecompressStream(compressed_dir_stream, &inflated_dir_stream));
  EXPECT_EQ(maldoca::Md5HexString(inflated_dir_stream),
            "e79af74b38c19001d047a0b61d600dfb");
}
}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
#ifdef MALDOCA_CHROME
  // mini_chromium needs InitLogging
  maldoca::InitLogging();
#endif
  ::benchmark::RunSpecifiedBenchmarks();
  return RUN_ALL_TESTS();
}
