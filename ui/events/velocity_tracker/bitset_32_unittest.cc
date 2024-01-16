// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/velocity_tracker/bitset_32.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

class BitSet32Test : public testing::Test {
 public:
  void TearDown() override {
    b1.clear();
    b2.clear();
  }

 protected:
  BitSet32 b1;
  BitSet32 b2;
};

TEST_F(BitSet32Test, Basic) {
  BitSet32 bits;

  // Test the empty set.
  EXPECT_EQ(0U, bits.count());
  EXPECT_TRUE(bits.is_empty());
  EXPECT_FALSE(bits.is_full());
  EXPECT_FALSE(bits.has_bit(0));
  EXPECT_FALSE(bits.has_bit(31));

  // Mark the first bit.
  bits.mark_bit(0);
  EXPECT_EQ(1U, bits.count());
  EXPECT_FALSE(bits.is_empty());
  EXPECT_FALSE(bits.is_full());
  EXPECT_TRUE(bits.has_bit(0));
  EXPECT_FALSE(bits.has_bit(31));
  EXPECT_EQ(0U, bits.first_marked_bit());
  EXPECT_EQ(0U, bits.last_marked_bit());
  EXPECT_EQ(1U, bits.first_unmarked_bit());

  // Mark the last bit.
  bits.mark_bit(31);
  EXPECT_EQ(2U, bits.count());
  EXPECT_FALSE(bits.is_empty());
  EXPECT_FALSE(bits.is_full());
  EXPECT_TRUE(bits.has_bit(0));
  EXPECT_TRUE(bits.has_bit(31));
  EXPECT_FALSE(bits.has_bit(15));
  EXPECT_EQ(0U, bits.first_marked_bit());
  EXPECT_EQ(31U, bits.last_marked_bit());
  EXPECT_EQ(1U, bits.first_unmarked_bit());
  EXPECT_EQ(0U, bits.get_index_of_bit(0));
  EXPECT_EQ(1U, bits.get_index_of_bit(1));
  EXPECT_EQ(1U, bits.get_index_of_bit(2));
  EXPECT_EQ(1U, bits.get_index_of_bit(31));

  // Clear the first bit.
  bits.clear_first_marked_bit();
  EXPECT_EQ(1U, bits.count());
  EXPECT_FALSE(bits.is_empty());
  EXPECT_FALSE(bits.is_full());
  EXPECT_FALSE(bits.has_bit(0));
  EXPECT_TRUE(bits.has_bit(31));
  EXPECT_EQ(31U, bits.first_marked_bit());
  EXPECT_EQ(31U, bits.last_marked_bit());
  EXPECT_EQ(0U, bits.first_unmarked_bit());
  EXPECT_EQ(0U, bits.get_index_of_bit(0));
  EXPECT_EQ(0U, bits.get_index_of_bit(1));
  EXPECT_EQ(0U, bits.get_index_of_bit(31));

  // Clear the last bit (the set should be empty).
  bits.clear_last_marked_bit();
  EXPECT_EQ(0U, bits.count());
  EXPECT_TRUE(bits.is_empty());
  EXPECT_FALSE(bits.is_full());
  EXPECT_FALSE(bits.has_bit(0));
  EXPECT_FALSE(bits.has_bit(31));
  EXPECT_EQ(0U, bits.get_index_of_bit(0));
  EXPECT_EQ(0U, bits.get_index_of_bit(31));
  EXPECT_EQ(BitSet32(), bits);

  // Mark the first unmarked bit (bit 0).
  bits.mark_first_unmarked_bit();
  EXPECT_EQ(1U, bits.count());
  EXPECT_FALSE(bits.is_empty());
  EXPECT_FALSE(bits.is_full());
  EXPECT_TRUE(bits.has_bit(0));
  EXPECT_EQ(0U, bits.first_marked_bit());
  EXPECT_EQ(0U, bits.last_marked_bit());
  EXPECT_EQ(1U, bits.first_unmarked_bit());

  // Mark the next unmarked bit (bit 1).
  bits.mark_first_unmarked_bit();
  EXPECT_EQ(2U, bits.count());
  EXPECT_FALSE(bits.is_empty());
  EXPECT_FALSE(bits.is_full());
  EXPECT_TRUE(bits.has_bit(0));
  EXPECT_TRUE(bits.has_bit(1));
  EXPECT_EQ(0U, bits.first_marked_bit());
  EXPECT_EQ(1U, bits.last_marked_bit());
  EXPECT_EQ(2U, bits.first_unmarked_bit());
  EXPECT_EQ(0U, bits.get_index_of_bit(0));
  EXPECT_EQ(1U, bits.get_index_of_bit(1));
  EXPECT_EQ(2U, bits.get_index_of_bit(2));
}

TEST_F(BitSet32Test, BitWiseOr) {
  b1.mark_bit(2);
  b2.mark_bit(4);

  BitSet32 tmp = b1 | b2;
  EXPECT_EQ(tmp.count(), 2u);
  EXPECT_TRUE(tmp.has_bit(2) && tmp.has_bit(4));
  // Check that the operator is symmetric.
  EXPECT_TRUE((b2 | b1) == (b1 | b2));

  b1 |= b2;
  EXPECT_EQ(b1.count(), 2u);
  EXPECT_TRUE(b1.has_bit(2) && b1.has_bit(4));
  EXPECT_TRUE(b2.has_bit(4) && b2.count() == 1u);
}

TEST_F(BitSet32Test, BitWiseAnd_Disjoint) {
  b1.mark_bit(2);
  b1.mark_bit(4);
  b1.mark_bit(6);

  BitSet32 tmp = b1 & b2;
  EXPECT_TRUE(tmp.is_empty());
  // Check that the operator is symmetric.
  EXPECT_TRUE((b2 & b1) == (b1 & b2));

  b2 &= b1;
  EXPECT_TRUE(b2.is_empty());
  EXPECT_EQ(b1.count(), 3u);
  EXPECT_TRUE(b1.has_bit(2) && b1.has_bit(4) && b1.has_bit(6));
}

TEST_F(BitSet32Test, BitWiseAnd_NonDisjoint) {
  b1.mark_bit(2);
  b1.mark_bit(4);
  b1.mark_bit(6);
  b2.mark_bit(3);
  b2.mark_bit(6);
  b2.mark_bit(9);

  BitSet32 tmp = b1 & b2;
  EXPECT_EQ(tmp.count(), 1u);
  EXPECT_TRUE(tmp.has_bit(6));
  // Check that the operator is symmetric.
  EXPECT_TRUE((b2 & b1) == (b1 & b2));

  b1 &= b2;
  EXPECT_EQ(b1.count(), 1u);
  EXPECT_EQ(b2.count(), 3u);
  EXPECT_TRUE(b2.has_bit(3) && b2.has_bit(6) && b2.has_bit(9));
}

TEST_F(BitSet32Test, MarkFirstUnmarkedBit) {
  b1.mark_bit(1);

  b1.mark_first_unmarked_bit();
  EXPECT_EQ(b1.count(), 2u);
  EXPECT_TRUE(b1.has_bit(0) && b1.has_bit(1));

  b1.mark_first_unmarked_bit();
  EXPECT_EQ(b1.count(), 3u);
  EXPECT_TRUE(b1.has_bit(0) && b1.has_bit(1) && b1.has_bit(2));
}

TEST_F(BitSet32Test, ClearFirstMarkedBit) {
  b1.mark_bit(0);
  b1.mark_bit(10);

  b1.clear_first_marked_bit();
  EXPECT_EQ(b1.count(), 1u);
  EXPECT_TRUE(b1.has_bit(10));

  b1.mark_bit(30);
  b1.clear_first_marked_bit();
  EXPECT_EQ(b1.count(), 1u);
  EXPECT_TRUE(b1.has_bit(30));
}

TEST_F(BitSet32Test, ClearLastMarkedBit) {
  b1.mark_bit(10);
  b1.mark_bit(31);

  b1.clear_last_marked_bit();
  EXPECT_EQ(b1.count(), 1u);
  EXPECT_TRUE(b1.has_bit(10));

  b1.mark_bit(5);
  b1.clear_last_marked_bit();
  EXPECT_EQ(b1.count(), 1u);
  EXPECT_TRUE(b1.has_bit(5));
}

TEST_F(BitSet32Test, FillAndClear) {
  EXPECT_TRUE(b1.is_empty());
  for (size_t i = 0; i < 32; i++) {
    b1.mark_first_unmarked_bit();
  }
  EXPECT_TRUE(b1.is_full());
  b1.clear();
  EXPECT_TRUE(b1.is_empty());
}

TEST_F(BitSet32Test, GetIndexOfBit) {
  b1.mark_bit(11);
  b1.mark_bit(29);
  EXPECT_EQ(b1.get_index_of_bit(11), 0u);
  EXPECT_EQ(b1.get_index_of_bit(29), 1u);
  b1.mark_first_unmarked_bit();
  EXPECT_EQ(b1.get_index_of_bit(11), 1u);
  EXPECT_EQ(b1.get_index_of_bit(29), 2u);
}

}  // namespace ui
