// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/fragment.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

#include "ipcz/buffer_id.h"
#include "ipcz/driver_memory.h"
#include "ipcz/driver_memory_mapping.h"
#include "reference_drivers/sync_reference_driver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ipcz {
namespace {

const IpczDriver& kTestDriver = reference_drivers::kSyncReferenceDriver;

using FragmentTest = testing::Test;

TEST_F(FragmentTest, FromDescriptorUnsafe) {
  char kBuffer[] = "Hello, world!";

  Fragment f = Fragment::FromDescriptorUnsafe({BufferId{0}, 1, 4}, kBuffer + 1);
  EXPECT_FALSE(f.is_null());
  EXPECT_FALSE(f.is_pending());
  EXPECT_EQ(1u, f.offset());
  EXPECT_EQ(4u, f.size());
  EXPECT_EQ("ello", std::string(f.bytes().begin(), f.bytes().end()));

  f = Fragment::FromDescriptorUnsafe({BufferId{0}, 7, 6}, kBuffer + 7);
  EXPECT_FALSE(f.is_null());
  EXPECT_FALSE(f.is_pending());
  EXPECT_EQ(7u, f.offset());
  EXPECT_EQ(6u, f.size());
  EXPECT_EQ("world!", std::string(f.bytes().begin(), f.bytes().end()));
}

TEST_F(FragmentTest, PendingFromDescriptor) {
  Fragment f = Fragment::PendingFromDescriptor({BufferId{0}, 5, 42});
  EXPECT_TRUE(f.is_pending());
  EXPECT_FALSE(f.is_null());
  EXPECT_EQ(5u, f.offset());
  EXPECT_EQ(42u, f.size());

  f = Fragment::PendingFromDescriptor({kInvalidBufferId, 0, 0});
  EXPECT_TRUE(f.is_null());
  EXPECT_FALSE(f.is_pending());
}

TEST_F(FragmentTest, NullMappedFromDescriptor) {
  constexpr size_t kDataSize = 32;
  DriverMemory memory(kTestDriver, kDataSize);
  auto mapping = memory.Map();

  Fragment f =
      Fragment::MappedFromDescriptor({kInvalidBufferId, 0, 0}, mapping);
  EXPECT_TRUE(f.is_null());
}

TEST_F(FragmentTest, InvalidMappedFromDescriptor) {
  constexpr size_t kDataSize = 32;
  DriverMemory memory(kTestDriver, kDataSize);
  auto mapping = memory.Map();

  Fragment f;

  // Offset out of bounds
  f = Fragment::MappedFromDescriptor({BufferId{0}, kDataSize, 1}, mapping);
  EXPECT_TRUE(f.is_null());

  // Tail out of bounds
  f = Fragment::MappedFromDescriptor({BufferId{0}, 0, kDataSize + 5}, mapping);
  EXPECT_TRUE(f.is_null());

  // Tail overflow
  f = Fragment::MappedFromDescriptor(
      {BufferId{0}, std::numeric_limits<uint32_t>::max(), 2}, mapping);
  EXPECT_TRUE(f.is_null());
}

TEST_F(FragmentTest, ValidMappedFromDescriptor) {
  const char kData[] = "0123456789abcdef";
  DriverMemory memory(kTestDriver, std::size(kData));
  auto mapping = memory.Map();
  memcpy(mapping.bytes().data(), kData, std::size(kData));

  Fragment f = Fragment::MappedFromDescriptor({BufferId{0}, 2, 11}, mapping);
  EXPECT_FALSE(f.is_null());
  EXPECT_FALSE(f.is_pending());
  EXPECT_EQ(2u, f.offset());
  EXPECT_EQ(11u, f.size());
  EXPECT_EQ("23456789abc", std::string(f.bytes().begin(), f.bytes().end()));
}

}  // namespace
}  // namespace ipcz
