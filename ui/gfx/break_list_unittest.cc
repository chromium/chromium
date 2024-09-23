// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/break_list.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/range/range.h"

namespace gfx {

class BreakListTest : public testing::Test {};

TEST_F(BreakListTest, ClearAndSetInitialValue) {
  // Check the default values applied to new instances.
  BreakList<bool> style_breaks(false);
  EXPECT_TRUE(style_breaks.EqualsValueForTesting(false));
  style_breaks.ClearAndSetInitialValue(true);
  EXPECT_TRUE(style_breaks.EqualsValueForTesting(true));

  // Ensure that setting values works correctly.
  BreakList<SkColor> color_breaks(SK_ColorRED);
  EXPECT_TRUE(color_breaks.EqualsValueForTesting(SK_ColorRED));
  color_breaks.ClearAndSetInitialValue(SK_ColorBLACK);
  EXPECT_TRUE(color_breaks.EqualsValueForTesting(SK_ColorBLACK));
}

TEST_F(BreakListTest, ClearAndSetInitialValueChanged) {
  BreakList<bool> breaks(false);
  EXPECT_FALSE(breaks.ClearAndSetInitialValue(false));
  EXPECT_TRUE(breaks.ClearAndSetInitialValue(true));
  EXPECT_FALSE(breaks.ClearAndSetInitialValue(true));
  EXPECT_TRUE(breaks.ClearAndSetInitialValue(false));

  constexpr size_t max = 99;
  breaks.SetMax(max);
  breaks.ApplyValue(true, Range(0, 2));
  breaks.ApplyValue(true, Range(3, 6));
  EXPECT_TRUE(breaks.ClearAndSetInitialValue(false));
  EXPECT_FALSE(breaks.ClearAndSetInitialValue(false));
}

TEST_F(BreakListTest, Reset) {
  BreakList<bool> breaks(false);
  constexpr size_t max = 99;
  breaks.SetMax(max);
  EXPECT_EQ(breaks.breaks().size(), 1U);
  breaks.ApplyValue(true, Range(0, 2));
  EXPECT_EQ(breaks.breaks().size(), 2U);
  breaks.Reset();

  EXPECT_EQ(breaks.breaks().size(), 1U);
}

TEST_F(BreakListTest, ApplyValue) {
  BreakList<bool> breaks(false);
  constexpr size_t max = 99;
  breaks.SetMax(max);

  // Ensure ApplyValue is a no-op on invalid and empty ranges.
  breaks.ApplyValue(true, Range::InvalidRange());
  EXPECT_TRUE(breaks.EqualsValueForTesting(false));
  for (size_t i = 0; i < 3; ++i) {
    breaks.ApplyValue(true, Range(i, i));
    EXPECT_TRUE(breaks.EqualsValueForTesting(false));
  }

  // Apply a value to a valid range, check breaks; repeating should be no-op.
  std::vector<std::pair<size_t, bool> > expected;
  expected.push_back(std::pair<size_t, bool>(0, false));
  expected.push_back(std::pair<size_t, bool>(2, true));
  expected.push_back(std::pair<size_t, bool>(3, false));
  for (size_t i = 0; i < 2; ++i) {
    breaks.ApplyValue(true, Range(2, 3));
    EXPECT_TRUE(breaks.EqualsForTesting(expected));
  }

  // Ensure setting a value overrides the ranged value.
  breaks.ClearAndSetInitialValue(true);
  EXPECT_TRUE(breaks.EqualsValueForTesting(true));

  // Ensure applying a value over [0, |max|) is the same as setting a value.
  breaks.ApplyValue(false, Range(0, max));
  EXPECT_TRUE(breaks.EqualsValueForTesting(false));

  // Ensure applying a value that is already applied has no effect.
  breaks.ApplyValue(false, Range(0, 2));
  breaks.ApplyValue(false, Range(3, 6));
  breaks.ApplyValue(false, Range(7, max));
  EXPECT_TRUE(breaks.EqualsValueForTesting(false));

  // Ensure applying an identical neighboring value merges the ranges.
  breaks.ApplyValue(true, Range(0, 3));
  breaks.ApplyValue(true, Range(3, 6));
  breaks.ApplyValue(true, Range(6, max));
  EXPECT_TRUE(breaks.EqualsValueForTesting(true));

  // Ensure applying a value with the same range overrides the ranged value.
  breaks.ApplyValue(false, Range(2, 3));
  breaks.ApplyValue(true, Range(2, 3));
  EXPECT_TRUE(breaks.EqualsValueForTesting(true));

  // Ensure applying a value with a containing range overrides contained values.
  breaks.ApplyValue(false, Range(0, 1));
  breaks.ApplyValue(false, Range(2, 3));
  breaks.ApplyValue(true, Range(0, 3));
  EXPECT_TRUE(breaks.EqualsValueForTesting(true));
  breaks.ApplyValue(false, Range(4, 5));
  breaks.ApplyValue(false, Range(6, 7));
  breaks.ApplyValue(false, Range(8, 9));
  breaks.ApplyValue(true, Range(4, 9));
  EXPECT_TRUE(breaks.EqualsValueForTesting(true));

  // Ensure applying various overlapping values yields the intended results.
  breaks.ApplyValue(false, Range(1, 4));
  breaks.ApplyValue(false, Range(5, 8));
  breaks.ApplyValue(true, Range(0, 2));
  breaks.ApplyValue(true, Range(3, 6));
  breaks.ApplyValue(true, Range(7, max));
  std::vector<std::pair<size_t, bool> > overlap;
  overlap.push_back(std::pair<size_t, bool>(0, true));
  overlap.push_back(std::pair<size_t, bool>(2, false));
  overlap.push_back(std::pair<size_t, bool>(3, true));
  overlap.push_back(std::pair<size_t, bool>(6, false));
  overlap.push_back(std::pair<size_t, bool>(7, true));
  EXPECT_TRUE(breaks.EqualsForTesting(overlap));
}

TEST_F(BreakListTest, ApplyValueChanged) {
  BreakList<bool> breaks(false);
  constexpr size_t max = 99;
  breaks.SetMax(max);

  // Set two ranges.
  EXPECT_TRUE(breaks.ApplyValue(true, Range(0, 5)));
  EXPECT_TRUE(breaks.ApplyValue(true, Range(9, 10)));

  // Setting sub-ranges should be a no-op.
  EXPECT_FALSE(breaks.ApplyValue(true, Range(0, 2)));
  EXPECT_FALSE(breaks.ApplyValue(true, Range(1, 3)));

  // Merge the two ranges.
  EXPECT_TRUE(breaks.ApplyValue(true, Range(2, 10)));

  // Setting sub-ranges should be a no-op.
  EXPECT_FALSE(breaks.ApplyValue(true, Range(0, 2)));
  EXPECT_FALSE(breaks.ApplyValue(true, Range(1, 3)));
}

TEST_F(BreakListTest, SetMax) {
  // Ensure values adjust to accommodate max position changes.
  BreakList<bool> breaks(false);
  breaks.SetMax(9);
  breaks.ApplyValue(true, Range(0, 2));
  breaks.ApplyValue(true, Range(3, 6));
  breaks.ApplyValue(true, Range(7, 9));

  std::vector<std::pair<size_t, bool> > expected;
  expected.push_back(std::pair<size_t, bool>(0, true));
  expected.push_back(std::pair<size_t, bool>(2, false));
  expected.push_back(std::pair<size_t, bool>(3, true));
  expected.push_back(std::pair<size_t, bool>(6, false));
  expected.push_back(std::pair<size_t, bool>(7, true));
  EXPECT_TRUE(breaks.EqualsForTesting(expected));

  // Setting a smaller max should remove any corresponding breaks.
  breaks.SetMax(7);
  expected.resize(4);
  EXPECT_TRUE(breaks.EqualsForTesting(expected));
  breaks.SetMax(4);
  expected.resize(3);
  EXPECT_TRUE(breaks.EqualsForTesting(expected));
  breaks.SetMax(4);
  EXPECT_TRUE(breaks.EqualsForTesting(expected));

  // Setting a larger max should not change any breaks.
  breaks.SetMax(50);
  EXPECT_TRUE(breaks.EqualsForTesting(expected));
}

TEST_F(BreakListTest, GetBreakAndRange) {
  BreakList<bool> breaks(false);
  breaks.SetMax(8);
  breaks.ApplyValue(true, Range(1, 2));
  breaks.ApplyValue(true, Range(4, 6));

  struct Case {
    size_t position;
    size_t break_index;
    Range range;
  };
  const auto cases = std::to_array<Case>({
      {0, 0, Range(0, 1)},
      {1, 1, Range(1, 2)},
      {2, 2, Range(2, 4)},
      {3, 2, Range(2, 4)},
      {4, 3, Range(4, 6)},
      {5, 3, Range(4, 6)},
      {6, 4, Range(6, 8)},
      {7, 4, Range(6, 8)},
  });

  for (const auto& c : cases) {
    BreakList<bool>::const_iterator it = breaks.GetBreak(c.position);
    EXPECT_EQ(breaks.breaks()[c.break_index], *it);
    EXPECT_EQ(breaks.GetRange(it), c.range);
  }
}

}  // namespace gfx
