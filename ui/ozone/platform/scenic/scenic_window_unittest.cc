// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_window.h"

#include "testing/gtest/include/gtest/gtest.h"

using fuchsia::ui::gfx::vec3;
using gfx::Insets;

namespace ui {
namespace {

constexpr float kDevicePixelRatio = 10.0;

fuchsia::ui::gfx::ViewProperties CreateViewProperties(const vec3& inset_min,
                                                      const vec3& inset_max) {
  fuchsia::ui::gfx::ViewProperties output;
  output.inset_from_min = inset_min;
  output.inset_from_max = inset_max;
  return output;
}

TEST(ScenicInsets, InsetsNotSet) {
  EXPECT_EQ(ScenicWindow::ConvertInsets(kDevicePixelRatio,
                                        CreateViewProperties({0, 0}, {0, 0})),
            Insets(0, 0, 0, 0));
}

TEST(ScenicInsets, SingleBorder) {
  // Top-aligned.
  EXPECT_EQ(ScenicWindow::ConvertInsets(kDevicePixelRatio,
                                        CreateViewProperties({0, 50}, {0, 0})),
            Insets(500, 0, 0, 0));

  // Left-aligned.
  EXPECT_EQ(ScenicWindow::ConvertInsets(kDevicePixelRatio,
                                        CreateViewProperties({50, 0}, {0, 0})),
            Insets(0, 500, 0, 0));

  // Right-aligned.
  EXPECT_EQ(ScenicWindow::ConvertInsets(kDevicePixelRatio,
                                        CreateViewProperties({0, 0}, {50, 0})),
            Insets(0, 0, 0, 500));

  // Bottom-aligned.
  EXPECT_EQ(ScenicWindow::ConvertInsets(kDevicePixelRatio,
                                        CreateViewProperties({0, 0}, {0, 50})),
            Insets(0, 0, 500, 0));
}

TEST(ScenicInsets, Combinations) {
  // All but top.
  EXPECT_EQ(ScenicWindow::ConvertInsets(
                kDevicePixelRatio, CreateViewProperties({50, 0}, {150, 50})),
            Insets(0, 500, 500, 1500));

  // All but left.
  EXPECT_EQ(ScenicWindow::ConvertInsets(
                kDevicePixelRatio, CreateViewProperties({0, 50}, {150, 50})),
            Insets(500, 0, 500, 1500));

  // All but right.
  EXPECT_EQ(ScenicWindow::ConvertInsets(
                kDevicePixelRatio, CreateViewProperties({50, 50}, {0, 50})),
            Insets(500, 500, 500, 0));

  // All but bottom.
  EXPECT_EQ(ScenicWindow::ConvertInsets(
                kDevicePixelRatio, CreateViewProperties({50, 50}, {150, 0})),
            Insets(500, 500, 0, 1500));

  // Surrounded on all sides.
  EXPECT_EQ(ScenicWindow::ConvertInsets(
                kDevicePixelRatio, CreateViewProperties({50, 50}, {150, 50})),
            Insets(500, 500, 500, 1500));
}

}  // namespace
}  // namespace ui
