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

// Unit-tests for the FAT reading code.

#include "maldoca/ole/fat.h"

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/string_view.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "maldoca/base/file.h"
#include "maldoca/base/testing/test_utils.h"
#include "maldoca/ole/oss_utils.h"

using ::maldoca::FAT;
using ::maldoca::OLEHeader;
using ::maldoca::SectorConstant;

namespace {
std::string TestFilename(absl::string_view file_name) {
  return maldoca::testing::OleTestFilename(file_name);
}

std::string GetTestContent(absl::string_view filename) {
  std::string content;
  auto status =
      maldoca::testing::GetTestContents(TestFilename(filename), &content);
  EXPECT_TRUE(status.ok()) << status;
  return content;
}

class FATTest : public testing::Test {
 protected:
  void SetUp() override {
    content_ = GetTestContent("vba1_xor_0x42_encoded.bin");
    EXPECT_TRUE(OLEHeader::ParseHeader(content_, &header));
  }

  OLEHeader header;
  std::string content_;
};

TEST(SectorReaderTest, SectorToArrayOfIndexes) {
  // One sector index value in input isn't complete.
  absl::string_view short_sector("\x00\x00\x00\x01\x00\x00\x02", 7);
  std::vector<uint32_t> array1;
  ASSERT_DEATH(FAT::SectorToArrayOfIndexes(short_sector, &array1),
               "Check failed: sector\\.size\\(\\) == 0 \\(3 vs\\. 0\\)");

  // Two full sector indexes.
  absl::string_view two_sectors("\x01\x00\x00\x00\x02\x00\x00\x00", 8);

  // Can't write to a nullptr destination.
  ASSERT_DEATH(FAT::SectorToArrayOfIndexes(two_sectors, nullptr), "array");
  // Can't overwrite the destination array of indexes.
  std::vector<uint32_t> array2 = {1};
  ASSERT_DEATH(FAT::SectorToArrayOfIndexes(two_sectors, &array2),
               "Check failed: array->size\\(\\) == 0 \\(1 vs\\. 0\\)");

  // Successful read.
  std::vector<uint32_t> array3;
  FAT::SectorToArrayOfIndexes(two_sectors, &array3);
  std::vector<uint32_t> array3_expected_values = {1, 2};
  EXPECT_EQ(array3, array3_expected_values);
}

TEST(SectorReaderTest, ReadSectorAt) {
  // Three made up sector indexes: 1, 2 and 3.
  absl::string_view input("\x01\x00\x00\x00\x02\x00\x00\x00\x03\x00\x00\x00",
                          12);

  // Destination can not be nullptr.
  ASSERT_DEATH(FAT::ReadSectorAt(input, 50, 12, false, nullptr), "sector");

  // Can't read past the size of the input.
  absl::string_view sector;
  EXPECT_FALSE(FAT::ReadSectorAt(input, 12, 512, false, &sector));

  // Can not read less than the specified sector size when
  // allow_short_sector_read is false.
  EXPECT_FALSE(FAT::ReadSectorAt(input, 0, 512, false, &sector));
  EXPECT_EQ(sector.size(), input.size());

  // Can read less than the specified sector size when
  // allow_short_sector_read is true.
  EXPECT_TRUE(FAT::ReadSectorAt(input, 4, 512, true, &sector));
  EXPECT_EQ(sector, absl::string_view("\x02\x00\x00\x00\x03\x00\x00\x00", 8));
}

// This tests attempts to read a FAT with bogus input.
TEST_F(FATTest, BogusInput) {
  std::vector<uint32_t> fat;
  std::string empty_input;
  // Input needs to have something that at least the size of a FAT.
  EXPECT_FALSE(FAT::Read(empty_input, header, &fat));
  EXPECT_EQ(fat.size(), 0);

  OLEHeader empty_header;
  // Can't read a FAT if you haven't read a header first.
  ASSERT_DEATH(FAT::Read(content_, empty_header, &fat),
               "Check failed: header\\.IsInitialized\\(\\)");
  EXPECT_EQ(fat.size(), 0);
}

TEST_F(FATTest, AlreadyInitialized) {
  std::vector<uint32_t> fat = {1, 2, 3};
  // Can't fill a FAT if it's been already initialized.
  ASSERT_DEATH(FAT::Read(content_, header, &fat),
               "Check failed: fat->empty\\(\\)");
}

// Test reading a real FAT.
TEST_F(FATTest, GoodInput) {
  std::vector<uint32_t> fat;
  EXPECT_TRUE(FAT::Read(content_, header, &fat));
  EXPECT_EQ(fat.size(), header.TotalNumSector());
  std::vector<uint32_t> expected = {
      SectorConstant::FATSectorInFAT,
      9,
      43,
      4,
      5,
      6,
      7,
      8,
      10,
      35,
      11,
      12,
      36,
      14,
      15,
      16,
      17,
      18,
      19,
      20,
      21,
      22,
      23,
      24,
      25,
      26,
      27,
      28,
      29,
      30,
      31,
      32,
      33,
      34,
      SectorConstant::EndOfChain,
      SectorConstant::EndOfChain,
      37,
      38,
      39,
      40,
      41,
      42,
      44,
      SectorConstant::EndOfChain,
      SectorConstant::EndOfChain,
  };
}
}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  maldoca::InitLogging();
#if !defined(_WIN32)
  ::benchmark::RunSpecifiedBenchmarks();
#endif
  return RUN_ALL_TESTS();
}
