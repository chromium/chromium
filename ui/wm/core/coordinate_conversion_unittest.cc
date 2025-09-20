// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/coordinate_conversion.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"

namespace wm {

typedef aura::test::AuraTestBase CoordinateConversionTest;

TEST_F(CoordinateConversionTest, ConvertRect) {
  std::unique_ptr<aura::Window> w = aura::test::CreateTestWindow(
      {.parent = root_window(), .bounds = {10, 20, 100, 200}});

  gfx::Rect r1(10, 20, 100, 120);
  ConvertRectFromScreen(w.get(), &r1);
  EXPECT_EQ("0,0 100x120", r1.ToString());

  gfx::Rect r2(0, 0, 100, 200);
  ConvertRectFromScreen(w.get(), &r2);
  EXPECT_EQ("-10,-20 100x200", r2.ToString());

  gfx::Rect r3(30, 30, 100, 200);
  ConvertRectToScreen(w.get(), &r3);
  EXPECT_EQ("40,50 100x200", r3.ToString());

  gfx::Rect r4(-10, -20, 100, 200);
  ConvertRectToScreen(w.get(), &r4);
  EXPECT_EQ("0,0 100x200", r4.ToString());
}

}  // namespace wm
