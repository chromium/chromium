// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "gtest/gtest.h"

#include "puffin/src/bit_reader.h"
#include "puffin/src/bit_writer.h"
#include "puffin/src/logging.h"
#include "puffin/src/unittest_common.h"

namespace puffin {

// Testing |BufferBitReader| and |BufferBitWriter|.
TEST(BitIOTest, BitWriterAndBitReaderTest) {
  static const size_t kSize = 5;
  uint8_t buf[kSize];

  puffin::BufferBitWriter bw(buf, kSize);
  ASSERT_TRUE(bw.WriteBits(0, 0x05));
  ASSERT_TRUE(bw.WriteBits(3, 0x05));
  ASSERT_TRUE(bw.WriteBits(8, 0xFF));
  ASSERT_TRUE(bw.WriteBoundaryBits(0x0F));
  uint8_t tmp[] = {1, 2};
  size_t index = 0;
  ASSERT_TRUE(bw.WriteBytes(2, [&tmp, &index](uint8_t* buffer, size_t count) {
    if (count > 2 - index)
      return false;
    memcpy(buffer, &tmp[index], count);
    return true;
  }));
  ASSERT_FALSE(bw.WriteBits(9, 0x1C));
  ASSERT_TRUE(bw.WriteBits(4, 0x0A));
  ASSERT_TRUE(bw.WriteBoundaryBits(0xBB));
  ASSERT_TRUE(bw.Flush());
  ASSERT_EQ(5, bw.Size());

  puffin::BufferBitReader br(buf, kSize);
  ASSERT_TRUE(br.CacheBits(11));
  ASSERT_EQ(br.ReadBits(3), 0x05);
  br.DropBits(3);
  ASSERT_EQ(br.ReadBits(8), 0xFF);
  br.DropBits(8);
  ASSERT_EQ(br.ReadBoundaryBits(), 0x0F);
  ASSERT_EQ(br.SkipBoundaryBits(), 5);
  std::function<bool(uint8_t*, size_t)> read_fn;
  ASSERT_TRUE(br.GetByteReaderFn(2, &read_fn));
  ASSERT_TRUE(read_fn(tmp, 2));
  ASSERT_EQ(br.Offset(), 4);
  ASSERT_TRUE(read_fn(tmp, 0));
  ASSERT_FALSE(read_fn(tmp, 1));
  ASSERT_FALSE(br.CacheBits(9));
  ASSERT_TRUE(br.CacheBits(8));
  ASSERT_EQ(br.ReadBits(4), 0x0A);
  br.DropBits(4);
  ASSERT_EQ(br.ReadBoundaryBits(), 0x0B);
  ASSERT_EQ(br.SkipBoundaryBits(), 4);
  ASSERT_EQ(br.ReadBoundaryBits(), 0);
  ASSERT_EQ(br.SkipBoundaryBits(), 0);
  ASSERT_FALSE(br.CacheBits(1));
}

TEST(BitIOTest, BitsRemaining) {
  const size_t kSize = 5;
  uint8_t buf[kSize];

  BufferBitReader br(buf, kSize);
  EXPECT_EQ(br.BitsRemaining(), 40);
  ASSERT_TRUE(br.CacheBits(1));
  br.DropBits(1);
  EXPECT_EQ(br.BitsRemaining(), 39);

  ASSERT_TRUE(br.CacheBits(7));
  br.DropBits(7);
  EXPECT_EQ(br.BitsRemaining(), 32);

  ASSERT_TRUE(br.CacheBits(31));
  br.DropBits(31);
  EXPECT_EQ(br.BitsRemaining(), 1);

  ASSERT_TRUE(br.CacheBits(1));
  br.DropBits(1);
  EXPECT_EQ(br.BitsRemaining(), 0);
}
}  // namespace puffin
