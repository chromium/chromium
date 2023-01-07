// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "gtest/gtest.h"

#include "puffin/file_stream.h"
#include "puffin/memory_stream.h"
#include "puffin/src/include/puffin/common.h"
#include "puffin/src/include/puffin/utils.h"
#include "puffin/src/unittest_common.h"

using std::string;
using std::vector;

namespace puffin {

namespace {
const uint8_t kZipEntries[] = {
    0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x02, 0x00, 0x08, 0x00, 0xfc, 0x88,
    0x28, 0x4c, 0xcb, 0x86, 0xe1, 0x80, 0x06, 0x00, 0x00, 0x00, 0x09, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x1c, 0x00, 0x31, 0x55, 0x54, 0x09, 0x00, 0x03,
    0xec, 0x15, 0x54, 0x5a, 0x49, 0x10, 0x54, 0x5a, 0x75, 0x78, 0x0b, 0x00,
    0x01, 0x04, 0x8f, 0x66, 0x05, 0x00, 0x04, 0x88, 0x13, 0x00, 0x00, 0x33,
    0x34, 0x84, 0x00, 0x2e, 0x00, 0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x02,
    0x00, 0x08, 0x00, 0x01, 0x89, 0x28, 0x4c, 0xe0, 0xe8, 0x6f, 0x6d, 0x06,
    0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x1c, 0x00, 0x32,
    0x55, 0x54, 0x09, 0x00, 0x03, 0xf1, 0x15, 0x54, 0x5a, 0x38, 0x10, 0x54,
    0x5a, 0x75, 0x78, 0x0b, 0x00, 0x01, 0x04, 0x8f, 0x66, 0x05, 0x00, 0x04,
    0x88, 0x13, 0x00, 0x00, 0x33, 0x32, 0x82, 0x01, 0x2e, 0x00};

// (echo "666666" > 2 && zip -fd test.zip 2 &&
//  cat test.zip | hexdump -v -e '10/1 "0x%02x, " "\n"')
const uint8_t kZipEntryWithDataDescriptor[] = {
    0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x08, 0x00, 0x08, 0x00, 0x0b, 0x74,
    0x2b, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x1c, 0x00, 0x32, 0x55, 0x54, 0x09, 0x00, 0x03,
    0xf5, 0xe5, 0x57, 0x5a, 0xf2, 0xe5, 0x57, 0x5a, 0x75, 0x78, 0x0b, 0x00,
    0x01, 0x04, 0x8f, 0x66, 0x05, 0x00, 0x04, 0x88, 0x13, 0x00, 0x00, 0x33,
    0x33, 0x03, 0x01, 0x2e, 0x00, 0x50, 0x4b, 0x07, 0x08, 0xb4, 0xa0, 0xf2,
    0x36, 0x06, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x50, 0x4b, 0x03,
    0x04, 0x14, 0x00, 0x08, 0x00, 0x08, 0x00, 0x0b, 0x74, 0x2b, 0x4c, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x1c, 0x00, 0x32, 0x55, 0x54, 0x09, 0x00, 0x03, 0xf5, 0xe5, 0x57,
    0x5a, 0xf2, 0xe5, 0x57, 0x5a, 0x75, 0x78, 0x0b, 0x00, 0x01, 0x04, 0x8f,
    0x66, 0x05, 0x00, 0x04, 0x88, 0x13, 0x00, 0x00, 0x33, 0x33, 0x03, 0x01,
    0x2e, 0x00, 0xb4, 0xa0, 0xf2, 0x36, 0x06, 0x00, 0x00, 0x00, 0x07, 0x00,
    0x00, 0x00};

void CheckFindPuffLocation(const Buffer& compressed,
                           const vector<BitExtent>& deflates,
                           const vector<ByteExtent>& expected_puffs,
                           uint64_t expected_puff_size) {
  auto src = MemoryStream::CreateForRead(compressed);
  vector<ByteExtent> puffs;
  uint64_t puff_size;
  ASSERT_TRUE(FindPuffLocations(src, deflates, &puffs, &puff_size));
  EXPECT_EQ(puffs, expected_puffs);
  EXPECT_EQ(puff_size, expected_puff_size);
}
}  // namespace

// Test Simple Puffing of the source.
TEST(UtilsTest, FindPuffLocations1Test) {
  CheckFindPuffLocation(kDeflatesSample1, kSubblockDeflateExtentsSample1,
                        kPuffExtentsSample1, kPuffsSample1.size());
}

TEST(UtilsTest, FindPuffLocations2Test) {
  CheckFindPuffLocation(kDeflatesSample2, kSubblockDeflateExtentsSample2,
                        kPuffExtentsSample2, kPuffsSample2.size());
}

TEST(UtilsTest, LocateDeflatesInZipArchiveSmoke) {
  Buffer zip_entries(kZipEntries, std::end(kZipEntries));
  vector<BitExtent> deflates;
  vector<BitExtent> expected_deflates = {{472, 46}, {992, 46}};
  EXPECT_TRUE(LocateDeflatesInZipArchive(zip_entries, &deflates));
  EXPECT_EQ(deflates, expected_deflates);
}

TEST(UtilsTest, LocateDeflatesInZipArchiveWithDataDescriptor) {
  Buffer zip_entries(kZipEntryWithDataDescriptor,
                     std::end(kZipEntryWithDataDescriptor));
  vector<BitExtent> deflates;
  vector<BitExtent> expected_deflates = {{472, 46}, {1120, 46}};
  EXPECT_TRUE(LocateDeflatesInZipArchive(zip_entries, &deflates));
  EXPECT_EQ(deflates, expected_deflates);
}

TEST(UtilsTest, LocateDeflatesInZipArchiveErrorChecks) {
  Buffer zip_entries(kZipEntries, std::end(kZipEntries));
  // Construct a invalid zip entry whose size overflows.
  zip_entries[29] = 0xff;
  vector<BitExtent> deflates_overflow;
  vector<BitExtent> expected_deflates = {{992, 46}};
  EXPECT_TRUE(LocateDeflatesInZipArchive(zip_entries, &deflates_overflow));
  EXPECT_EQ(deflates_overflow, expected_deflates);

  zip_entries.resize(128);
  vector<BitExtent> deflates_incomplete;
  EXPECT_TRUE(LocateDeflatesInZipArchive(zip_entries, &deflates_incomplete));
  EXPECT_TRUE(deflates_incomplete.empty());
}

TEST(UtilsTest, RemoveEqualBitExtents) {
  Buffer data1 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  Buffer data2 = {1, 2, 3, 4, 5, 5, 6, 7, 8, 9};
  vector<BitExtent> ext1 = {{0, 10}, {10, 14}, {25, 15}, {40, 8}, {50, 23}};
  vector<BitExtent> ext2 = {{0, 10}, {17, 15}, {32, 8}, {40, 8}, {50, 23}};
  RemoveEqualBitExtents(data1, data2, &ext1, &ext2);
  vector<BitExtent> expected_ext1 = {{0, 10}, {10, 14}};
  EXPECT_EQ(expected_ext1, ext1);
  vector<BitExtent> expected_ext2 = {{0, 10}};
  EXPECT_EQ(expected_ext2, ext2);
  RemoveEqualBitExtents(data1, data2, &ext1, &ext1);
  EXPECT_EQ(expected_ext1, ext1);
  RemoveEqualBitExtents(data1, data1, &ext1, &ext1);
  EXPECT_TRUE(ext1.empty());
  expected_ext1 = ext1 = {{0, 0}, {1, 1}, {2, 7}};
  RemoveEqualBitExtents(data1, data2, &ext1, &ext2);
  EXPECT_EQ(expected_ext1, ext1);
  EXPECT_EQ(expected_ext2, ext2);
}

TEST(UtilsTest, RemoveDeflatesWithBadDistanceCaches) {
  vector<BitExtent> deflates(kProblematicCacheDeflateExtents), empty;
  EXPECT_TRUE(
      RemoveDeflatesWithBadDistanceCaches(kProblematicCache, &deflates));
  EXPECT_EQ(deflates, empty);

  // Just a sanity check to make sure this function is not removing anything
  // else.
  deflates = kSubblockDeflateExtentsSample1;
  EXPECT_TRUE(RemoveDeflatesWithBadDistanceCaches(kDeflatesSample1, &deflates));
  EXPECT_EQ(deflates, kSubblockDeflateExtentsSample1);

  // Now combine three deflates and make sure it is doing the right job.
  Buffer data;
  data.insert(data.end(), kDeflatesSample1.begin(), kDeflatesSample1.end());
  data.insert(data.end(), kProblematicCache.begin(), kProblematicCache.end());
  data.insert(data.end(), kDeflatesSample1.begin(), kDeflatesSample1.end());

  deflates = kSubblockDeflateExtentsSample1;
  size_t offset = kDeflatesSample1.size() * 8;
  for (const auto& deflate : kProblematicCacheDeflateExtents) {
    deflates.emplace_back(deflate.offset + offset, deflate.length);
  }
  offset += kProblematicCache.size() * 8;
  for (const auto& deflate : kSubblockDeflateExtentsSample1) {
    deflates.emplace_back(deflate.offset + offset, deflate.length);
  }

  auto expected_deflates(deflates);
  expected_deflates.erase(expected_deflates.begin() +
                          kSubblockDeflateExtentsSample1.size());

  EXPECT_TRUE(RemoveDeflatesWithBadDistanceCaches(data, &deflates));
  EXPECT_EQ(deflates, expected_deflates);
}

}  // namespace puffin
