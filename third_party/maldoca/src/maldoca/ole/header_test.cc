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

// Unit-tests for the OLEHeader class.

#include "maldoca/ole/header.h"

#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "maldoca/base/file.h"
#include "maldoca/base/logging.h"
#include "maldoca/base/testing/test_utils.h"
#include "maldoca/ole/fat.h"

namespace {

using ::maldoca::OLEHeader;
using ::maldoca::SectorConstant;

std::string TestFilename(absl::string_view filename) {
  return maldoca::testing::OleTestFilename(filename);
}

std::string GetTestContent(absl::string_view filename) {
  std::string content;
  auto status =
      maldoca::testing::GetTestContents(TestFilename(filename), &content);
  EXPECT_TRUE(status.ok()) << status;
  return content;
}

// Test how the code reacts to bogus input.
TEST(OLEReaderTest, BogusInput) {
  OLEHeader header;

  // The constructor leaves the header not initialized.
  EXPECT_FALSE(header.IsInitialized());

  // Attempt to read from an input that's too short to be a header
  std::string empty;
  EXPECT_FALSE(OLEHeader::ParseHeader(empty, &header));
  EXPECT_FALSE(header.IsInitialized());

  // Attempt to read from a sizeable input that's not a OLE header
  std::string bogus = std::string(512, 'a');
  EXPECT_FALSE(OLEHeader::ParseHeader(bogus, &header));
  EXPECT_FALSE(header.IsInitialized());

  // Read a valid OLE file but change one of the few uint16_t that needs
  // to have a fixed value. Here, we're changing MiniSectorCutoff, 4
  // bytes at offset 56 (4096 is expected.)
  std::string content;
  content = GetTestContent("vba1_xor_0x42_encoded.bin");
  content.replace(56, 4, "LOL!");
  EXPECT_FALSE(OLEHeader::ParseHeader(bogus, &header));
  EXPECT_FALSE(header.IsInitialized());
}

// Test how the code handles good input and check against
// expectations.
TEST(OLEReaderTest, GoodInput) {
  OLEHeader header;

  EXPECT_FALSE(header.IsInitialized());

  std::string content;
  content = GetTestContent("vba1_xor_0x42_encoded.bin");
  EXPECT_TRUE(OLEHeader::ParseHeader(content, &header));
  EXPECT_EQ(header.TotalNumSector(), 45);
  EXPECT_EQ(header.SectorSize(), 512);
  EXPECT_EQ(header.FATNumSector(), 1);
  EXPECT_EQ(header.DIFATFirstSector(), SectorConstant::EndOfChain);
  EXPECT_EQ(header.DIFATNumSector(), 0);
  EXPECT_EQ(header.DirectoryFirstSector(), 1);
  EXPECT_EQ(header.MiniFATFirstSector(), 2);
  EXPECT_EQ(header.MiniFATNumSector(), 2);
  EXPECT_EQ(header.MiniFATSectorSize(), 64);
  EXPECT_EQ(header.MiniFATStreamCutoff(), 4096);
}
}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
#ifdef MALDOCA_CHROME
  // mini_chromium needs InitLogging
  maldoca::InitLogging();
#else
  ::benchmark::RunSpecifiedBenchmarks();
#endif
  return RUN_ALL_TESTS();
}
