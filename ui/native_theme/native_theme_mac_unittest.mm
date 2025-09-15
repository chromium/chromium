// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_mac.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
namespace {

TEST(NativeThemeMacTest, ThumbSize) {
  EXPECT_EQ(NativeThemeMac::GetThumbMinSize(false, 1.0), gfx::Size(6, 18));
  EXPECT_EQ(NativeThemeMac::GetThumbMinSize(true, 1.0), gfx::Size(18, 6));
  EXPECT_EQ(NativeThemeMac::GetThumbMinSize(false, 2.0), gfx::Size(12, 36));
  EXPECT_EQ(NativeThemeMac::GetThumbMinSize(true, 2.0), gfx::Size(36, 12));
}

}  // namespace
}  // namespace ui
