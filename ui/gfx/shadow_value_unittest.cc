// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/shadow_value.h"

#include <stddef.h>

#include <array>
#include <vector>

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d.h"

namespace gfx {

TEST(ShadowValueTest, GetMargin) {
  struct TestCase {
    Insets expected_margin;
    ShadowValues shadows;
  };
  const auto kTestCases = std::to_array<TestCase>({
      {
          Insets(),
          {},
      },
      {
          Insets(-2),
          {
              {gfx::Vector2d(0, 0), 4, 0},
          },
      },
      {
          Insets::TLBR(0, -1, -4, -3),
          {
              {gfx::Vector2d(1, 2), 4, 0},
          },
      },
      {
          Insets::TLBR(-4, -3, 0, -1),
          {
              {gfx::Vector2d(-1, -2), 4, 0},
          },
      },
      {
          Insets::TLBR(0, -1, -5, -4),
          {
              {gfx::Vector2d(1, 2), 4, 0},
              {gfx::Vector2d(2, 3), 4, 0},
          },
      },
      {
          Insets::TLBR(-4, -3, -5, -4),
          {
              {gfx::Vector2d(-1, -2), 4, 0},
              {gfx::Vector2d(2, 3), 4, 0},
          },
      },
  });

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    Insets margin = ShadowValue::GetMargin(kTestCases[i].shadows);

    EXPECT_EQ(kTestCases[i].expected_margin, margin) << " i=" << i;
  }
}

}  // namespace gfx
