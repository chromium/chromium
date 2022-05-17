// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gtest/gtest.h"

#include "puffin/src/puff_reader.h"
#include "puffin/src/puff_writer.h"
#include "puffin/src/unittest_common.h"

namespace puffin {

namespace {
void TestLiteralLength(size_t length) {
  Buffer buf(length + 10);
  PuffData pd;

  BufferPuffWriter pw(buf.data(), buf.size());
  // We need to insert a metadata otherwise it will fail.
  pd.type = PuffData::Type::kBlockMetadata;
  pd.length = 1;
  ASSERT_TRUE(pw.Insert(pd));

  BufferPuffReader pr(buf.data(), buf.size());
  ASSERT_TRUE(pr.GetNext(&pd));
  ASSERT_EQ(pd.type, PuffData::Type::kBlockMetadata);
  ASSERT_EQ(pd.length, 1);

  // We insert |length| bytes.
  pd.type = PuffData::Type::kLiterals;
  pd.length = length;
  pd.read_fn = [](uint8_t* buffer, size_t count) {
    std::fill(buffer, buffer + count, 10);
    return true;
  };
  ASSERT_TRUE(pw.Insert(pd));
  ASSERT_TRUE(pw.Flush());

  pd.type = PuffData::Type::kLenDist;
  pd.distance = 1;
  pd.length = 3;
  ASSERT_TRUE(pw.Insert(pd));

  ASSERT_TRUE(pr.GetNext(&pd));
  if (length == 0) {
    // If length is zero, then nothing should've been inserted.
    ASSERT_EQ(pd.type, PuffData::Type::kLenDist);
  } else {
    // We have to see |length| bytes.
    ASSERT_EQ(pd.type, PuffData::Type::kLiterals);
    ASSERT_EQ(pd.length, length);
    for (size_t i = 0; i < pd.length; i++) {
      uint8_t byte;
      pd.read_fn(&byte, 1);
      EXPECT_EQ(byte, 10);
    }
  }
}
}  // namespace

// Testing read/write from/into a puff buffer using |PuffReader|/|PuffWriter|.
TEST(PuffIOTest, InputOutputTest) {
  Buffer buf(100);
  BufferPuffReader pr(buf.data(), buf.size());
  BufferPuffWriter pw(buf.data(), buf.size());
  BufferPuffWriter epw(nullptr, 0);
  uint8_t block = 123;

  {
    PuffData pd;
    pd.type = PuffData::Type::kBlockMetadata;
    pd.block_metadata[0] = 0xCC;  // header
    memcpy(&pd.block_metadata[1], &block, sizeof(block));
    pd.length = sizeof(block) + 1;
    ASSERT_TRUE(pw.Insert(pd));
    ASSERT_TRUE(epw.Insert(pd));
    ASSERT_TRUE(pw.Flush());
    ASSERT_TRUE(epw.Flush());
  }
  {
    PuffData pd;
    ASSERT_TRUE(pr.GetNext(&pd));
    ASSERT_EQ(pd.type, PuffData::Type::kBlockMetadata);
    ASSERT_EQ(pd.length, sizeof(block) + 1);
    ASSERT_EQ(pd.block_metadata[0], 0xCC);
    ASSERT_EQ(pd.block_metadata[1], block);
  }
  {
    PuffData pd;
    pd.type = PuffData::Type::kLenDist;
    pd.distance = 321;
    pd.length = 3;
    ASSERT_TRUE(pw.Insert(pd));
    ASSERT_TRUE(epw.Insert(pd));
    pd.length = 127;
    ASSERT_TRUE(pw.Insert(pd));
    ASSERT_TRUE(epw.Insert(pd));
    pd.length = 258;
    ASSERT_TRUE(pw.Insert(pd));
    ASSERT_TRUE(epw.Insert(pd));
    ASSERT_TRUE(pw.Flush());
    ASSERT_TRUE(epw.Flush());

    pd.length = 259;
    ASSERT_FALSE(pw.Insert(pd));
    ASSERT_FALSE(epw.Insert(pd));
  }
  {
    PuffData pd;
    ASSERT_TRUE(pr.GetNext(&pd));
    ASSERT_EQ(pd.type, PuffData::Type::kLenDist);
    ASSERT_EQ(pd.distance, 321);
    ASSERT_EQ(pd.length, 3);
    ASSERT_TRUE(pr.GetNext(&pd));
    ASSERT_EQ(pd.type, PuffData::Type::kLenDist);
    ASSERT_EQ(pd.length, 127);
    ASSERT_TRUE(pr.GetNext(&pd));
    ASSERT_EQ(pd.type, PuffData::Type::kLenDist);
    ASSERT_EQ(pd.length, 258);
  }
  {
    PuffData pd;
    pd.type = PuffData::Type::kEndOfBlock;
    ASSERT_TRUE(pw.Insert(pd));
    ASSERT_TRUE(epw.Insert(pd));
    ASSERT_TRUE(pw.Flush());
    ASSERT_TRUE(epw.Flush());
  }
  {
    PuffData pd;
    ASSERT_TRUE(pr.GetNext(&pd));
    ASSERT_EQ(pd.type, PuffData::Type::kEndOfBlock);
  }
  {
    PuffData pd;
    pd.type = PuffData::Type::kBlockMetadata;
    block = 123;
    pd.block_metadata[0] = 0xCC;  // header
    memcpy(&pd.block_metadata[1], &block, sizeof(block));
    pd.length = sizeof(block) + 1;
    ASSERT_TRUE(pw.Insert(pd));
    ASSERT_TRUE(epw.Insert(pd));
    ASSERT_TRUE(pw.Flush());
    ASSERT_TRUE(epw.Flush());
  }
  {
    PuffData pd;
    ASSERT_TRUE(pr.GetNext(&pd));
    ASSERT_EQ(pd.type, PuffData::Type::kBlockMetadata);
    ASSERT_EQ(pd.length, sizeof(block) + 1);
    ASSERT_EQ(pd.block_metadata[0], 0xCC);
    ASSERT_EQ(pd.block_metadata[1], block);
  }

  uint8_t tmp[] = {1, 2, 100};
  {
    PuffData pd;
    size_t index = 0;
    pd.type = PuffData::Type::kLiterals;
    pd.length = 3;
    pd.read_fn = [&tmp, &index](uint8_t* buffer, size_t count) {
      if (count > 3 - index)
        return false;
      if (buffer != nullptr) {
        memcpy(buffer, &tmp[index], count);
      }
      index += count;
      return true;
    };
    ASSERT_TRUE(pw.Insert(pd));
    ASSERT_TRUE(pw.Flush());
    // We have to refresh the read_fn function for the second insert.
    index = 0;
    ASSERT_TRUE(epw.Insert(pd));
    ASSERT_TRUE(epw.Flush());
  }
  {
    PuffData pd;
    pd.type = PuffData::Type::kLiteral;
    pd.byte = 10;
    ASSERT_TRUE(pw.Insert(pd));
    ASSERT_TRUE(epw.Insert(pd));
    ASSERT_TRUE(pw.Flush());
    ASSERT_TRUE(epw.Flush());
  }

  uint8_t tmp3[3];
  {
    PuffData pd;
    ASSERT_TRUE(pr.GetNext(&pd));
    ASSERT_EQ(pd.type, PuffData::Type::kLiterals);
    ASSERT_EQ(pd.length, 3);
    ASSERT_TRUE(pd.read_fn(tmp3, 3));
    ASSERT_FALSE(pd.read_fn(tmp3, 1));
    ASSERT_EQ(0, memcmp(tmp3, tmp, 3));
  }
  {
    PuffData pd;
    ASSERT_TRUE(pr.GetNext(&pd));
    ASSERT_EQ(pd.type, PuffData::Type::kLiterals);
    ASSERT_EQ(pd.length, 1);
    ASSERT_TRUE(pd.read_fn(tmp3, 1));
    ASSERT_EQ(tmp3[0], 10);
    ASSERT_FALSE(pd.read_fn(tmp3, 2));
  }
  {
    PuffData pd;
    pd.type = PuffData::Type::kEndOfBlock;
    ASSERT_TRUE(pw.Insert(pd));
    ASSERT_TRUE(epw.Insert(pd));
    ASSERT_TRUE(pw.Flush());
    ASSERT_TRUE(epw.Flush());
  }
  {
    PuffData pd;
    ASSERT_TRUE(pr.GetNext(&pd));
    ASSERT_EQ(pd.type, PuffData::Type::kEndOfBlock);
  }

  ASSERT_EQ(buf.size() - pr.BytesLeft(), pw.Size());
  ASSERT_EQ(buf.size() - pr.BytesLeft(), epw.Size());
}

// Testing metadata boundary.
TEST(PuffIOTest, MetadataBoundaryTest) {
  PuffData pd;
  Buffer buf(3);
  BufferPuffWriter pw(buf.data(), buf.size());

  // Block metadata takes two + varied bytes, so on a thre byte buffer, only one
  // bytes is left for the varied part of metadata.
  pd.type = PuffData::Type::kBlockMetadata;
  pd.length = 2;
  ASSERT_FALSE(pw.Insert(pd));
  pd.length = 0;  // length should be at least 1.
  ASSERT_FALSE(pw.Insert(pd));
  pd.length = 1;
  ASSERT_TRUE(pw.Insert(pd));

  Buffer puff_buffer = {0x00, 0x03, 0x02, 0x00, 0x00};
  BufferPuffReader pr(puff_buffer.data(), puff_buffer.size());
  ASSERT_FALSE(pr.GetNext(&pd));
}

TEST(PuffIOTest, InvalidCopyLengthsDistanceTest) {
  PuffData pd;
  Buffer puff_buffer(20);
  BufferPuffWriter pw(puff_buffer.data(), puff_buffer.size());

  // Invalid Lenght values.
  pd.type = PuffData::Type::kLenDist;
  pd.distance = 1;
  pd.length = 0;
  EXPECT_FALSE(pw.Insert(pd));
  pd.length = 1;
  EXPECT_FALSE(pw.Insert(pd));
  pd.length = 2;
  EXPECT_FALSE(pw.Insert(pd));
  pd.length = 3;
  EXPECT_TRUE(pw.Insert(pd));
  pd.length = 259;
  EXPECT_FALSE(pw.Insert(pd));
  pd.length = 258;
  EXPECT_TRUE(pw.Insert(pd));

  // Invalid distance values.
  pd.length = 3;
  pd.distance = 0;
  EXPECT_FALSE(pw.Insert(pd));
  pd.distance = 1;
  EXPECT_TRUE(pw.Insert(pd));
  pd.distance = 32769;
  EXPECT_FALSE(pw.Insert(pd));
  pd.distance = 32768;
  EXPECT_TRUE(pw.Insert(pd));

  // First three bytes header, four bytes value lit/len, and four bytes
  // invalid lit/len.
  puff_buffer = {0x00, 0x00, 0xFF, 0xFF, 0x80, 0x00,
                 0x00, 0xFF, 0x82, 0x00, 0x00};
  BufferPuffReader pr(puff_buffer.data(), puff_buffer.size());
  EXPECT_TRUE(pr.GetNext(&pd));
  EXPECT_EQ(pd.type, PuffData::Type::kBlockMetadata);
  EXPECT_TRUE(pr.GetNext(&pd));
  EXPECT_EQ(pd.type, PuffData::Type::kLenDist);
  EXPECT_FALSE(pr.GetNext(&pd));
}

TEST(PuffIOTest, InvalidCopyLenghtDistanceBoundaryTest) {
  PuffData pd;
  Buffer puff_buffer(5);

  pd.type = PuffData::Type::kLenDist;
  pd.distance = 1;
  pd.length = 129;
  for (size_t i = 1; i < 2; i++) {
    BufferPuffWriter pw(puff_buffer.data(), i);
    EXPECT_FALSE(pw.Insert(pd));
  }

  pd.length = 130;
  for (size_t i = 1; i < 3; i++) {
    BufferPuffWriter pw(puff_buffer.data(), i);
    EXPECT_FALSE(pw.Insert(pd));
  }

  // First three bytes header, three bytes value lit/len.
  puff_buffer = {0x00, 0x00, 0xFF, 0xFF, 0x80, 0x00};
  BufferPuffReader pr(puff_buffer.data(), puff_buffer.size());
  EXPECT_TRUE(pr.GetNext(&pd));
  EXPECT_EQ(pd.type, PuffData::Type::kBlockMetadata);
  EXPECT_FALSE(pr.GetNext(&pd));
}

TEST(PuffIOTest, LiteralsTest) {
  TestLiteralLength(0);
  TestLiteralLength(1);
  TestLiteralLength(2);
  TestLiteralLength(126);
  TestLiteralLength(127);
  TestLiteralLength(128);
}

// Testing maximum literals length.
TEST(PuffIOTest, MaxLiteralsTest) {
  Buffer buf((1 << 16) + 127 + 20);
  PuffData pd;

  BufferPuffWriter pw(buf.data(), buf.size());
  // We need to insert a metadata otherwise it will fail.
  pd.type = PuffData::Type::kBlockMetadata;
  pd.length = 1;
  ASSERT_TRUE(pw.Insert(pd));

  pd.type = PuffData::Type::kLiterals;
  pd.length = (1 << 16);
  pd.read_fn = [](uint8_t* buffer, size_t count) {
    std::fill(buffer, buffer + count, 10);
    return true;
  };
  ASSERT_TRUE(pw.Insert(pd));
  ASSERT_TRUE(pw.Flush());

  BufferPuffReader pr(buf.data(), buf.size());
  ASSERT_TRUE(pr.GetNext(&pd));
  ASSERT_EQ(pd.type, PuffData::Type::kBlockMetadata);
  ASSERT_EQ(pd.length, 1);

  ASSERT_TRUE(pr.GetNext(&pd));
  ASSERT_EQ(pd.type, PuffData::Type::kLiterals);
  ASSERT_EQ(pd.length, 1 << 16);
  for (size_t i = 0; i < pd.length; i++) {
    uint8_t byte;
    pd.read_fn(&byte, 1);
    ASSERT_EQ(byte, 10);
  }

  BufferPuffWriter pw2(buf.data(), buf.size());
  pd.type = PuffData::Type::kBlockMetadata;
  pd.length = 1;
  ASSERT_TRUE(pw2.Insert(pd));

  pd.type = PuffData::Type::kLiteral;
  pd.length = 1;
  pd.byte = 12;
  // We have to be able to fill 65663 bytes.
  for (size_t i = 0; i < ((1 << 16) + 127); i++) {
    ASSERT_TRUE(pw2.Insert(pd));
  }
  // If we add one more, then it should have been flushed.
  pd.byte = 13;
  ASSERT_TRUE(pw2.Insert(pd));
  ASSERT_TRUE(pw2.Flush());

  // Now read it back.
  BufferPuffReader pr2(buf.data(), buf.size());
  ASSERT_TRUE(pr2.GetNext(&pd));
  ASSERT_EQ(pd.type, PuffData::Type::kBlockMetadata);

  // Now we should read on kLiterals with lenght 1 << 16 and just one literal
  // after that.
  ASSERT_TRUE(pr2.GetNext(&pd));
  ASSERT_EQ(pd.type, PuffData::Type::kLiterals);
  ASSERT_EQ(pd.length, (1 << 16) + 127);
  for (size_t i = 0; i < pd.length; i++) {
    uint8_t byte;
    pd.read_fn(&byte, 1);
    ASSERT_EQ(byte, 12);
  }

  ASSERT_TRUE(pr2.GetNext(&pd));
  ASSERT_EQ(pd.type, PuffData::Type::kLiterals);
  ASSERT_EQ(pd.length, 1);
  uint8_t byte;
  pd.read_fn(&byte, 1);
  ASSERT_EQ(byte, 13);
}

}  // namespace puffin
