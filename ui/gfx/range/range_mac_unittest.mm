// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/range/range.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

TEST(RangeTest, FromNSRange) {
  NSRange nsr = NSMakeRange(10, 3);
  gfx::Range r(nsr);
  EXPECT_EQ(nsr.location, r.start());
  EXPECT_EQ(13U, r.end());
  EXPECT_EQ(nsr.length, r.length());
  EXPECT_FALSE(r.is_reversed());
  EXPECT_TRUE(r.IsValid());
}

TEST(RangeTest, ToNSRange) {
  gfx::Range r(10, 12);
  NSRange nsr = r.ToNSRange();
  EXPECT_EQ(10U, nsr.location);
  EXPECT_EQ(2U, nsr.length);
}

TEST(RangeTest, ReversedToNSRange) {
  gfx::Range r(20, 10);
  NSRange nsr = r.ToNSRange();
  EXPECT_EQ(10U, nsr.location);
  EXPECT_EQ(10U, nsr.length);
}

TEST(RangeTest, FromNSRangeInvalid) {
  NSRange nsr = NSMakeRange(NSNotFound, 0);
  gfx::Range r(nsr);
  EXPECT_FALSE(r.IsValid());
}

TEST(RangeTest, ToNSRangeInvalid) {
  gfx::Range r(gfx::Range::InvalidRange());
  NSRange nsr = r.ToNSRange();
  EXPECT_EQ(static_cast<NSUInteger>(NSNotFound), nsr.location);
  EXPECT_EQ(0U, nsr.length);
}

TEST(RangeTest, FromPossiblyInvalidNSRange) {
  constexpr uint32_t range_max = std::numeric_limits<uint32_t>::max();
  EXPECT_NE(
      gfx::Range::FromPossiblyInvalidNSRange(NSMakeRange(range_max - 1, 1)),
      gfx::Range::InvalidRange());
  EXPECT_EQ(gfx::Range::FromPossiblyInvalidNSRange(NSMakeRange(range_max, 1)),
            gfx::Range::InvalidRange());
  EXPECT_EQ(gfx::Range::FromPossiblyInvalidNSRange(
                NSMakeRange(static_cast<int64_t>(range_max) + 1, 0)),
            gfx::Range::InvalidRange());
  EXPECT_EQ(gfx::Range::FromPossiblyInvalidNSRange(
                NSMakeRange(0, static_cast<int64_t>(range_max) + 1)),
            gfx::Range::InvalidRange());
}
