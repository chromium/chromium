// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_mac.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
namespace {

TEST(NativeThemeMacTest, ThumbSize) {
  EXPECT_EQ(gfx::Size(6.0, 18.0), NativeThemeMac::GetThumbMinSize(true, 1.0));
  EXPECT_EQ(gfx::Size(18.0, 6.0), NativeThemeMac::GetThumbMinSize(false, 1.0));
  EXPECT_EQ(gfx::Size(12.0, 36.0), NativeThemeMac::GetThumbMinSize(true, 2.0));
  EXPECT_EQ(gfx::Size(36.0, 12.0), NativeThemeMac::GetThumbMinSize(false, 2.0));
}

}  // namespace
}  // namespace ui
