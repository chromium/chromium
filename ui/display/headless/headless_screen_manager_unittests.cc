// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/headless/headless_screen_manager.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"

namespace display {
namespace {

class HeadlessDisplayGeometryTest : public ::testing::Test {
 public:
  HeadlessDisplayGeometryTest() = default;
  ~HeadlessDisplayGeometryTest() override = default;

  HeadlessDisplayGeometryTest(const HeadlessDisplayGeometryTest&) = delete;
  HeadlessDisplayGeometryTest& operator=(const HeadlessDisplayGeometryTest&) =
      delete;

 protected:
  void SetDisplayGeometry(const gfx::Rect& bounds_in_pixels,
                          const gfx::Insets& work_area_insets_pixels,
                          float device_pixel_ratio) {
    HeadlessScreenManager::SetDisplayGeometry(display_, bounds_in_pixels,
                                              work_area_insets_pixels,
                                              device_pixel_ratio);
  }

  Display display_;
};

TEST_F(HeadlessDisplayGeometryTest, DefaultDisplay) {
  SetDisplayGeometry(gfx::Rect(0, 0, 800, 600), gfx::Insets(), 1.0f);

  EXPECT_EQ(display_.bounds(), gfx::Rect(0, 0, 800, 600));
  EXPECT_EQ(display_.work_area(), gfx::Rect(0, 0, 800, 600));
  EXPECT_EQ(display_.device_scale_factor(), 1.0f);
}

TEST_F(HeadlessDisplayGeometryTest, DefaultDisplayWorkArea) {
  SetDisplayGeometry(gfx::Rect(0, 0, 800, 600), gfx::Insets(10), 1.0f);

  EXPECT_EQ(display_.bounds(), gfx::Rect(0, 0, 800, 600));
  EXPECT_EQ(display_.work_area(), gfx::Rect(10, 10, 780, 580));
  EXPECT_EQ(display_.device_scale_factor(), 1.0f);
}

TEST_F(HeadlessDisplayGeometryTest, Scaled2xDisplay) {
  SetDisplayGeometry(gfx::Rect(100, 100, 1600, 1200), gfx::Insets(), 2.0f);

  EXPECT_EQ(display_.bounds(), gfx::Rect(100, 100, 800, 600));
  EXPECT_EQ(display_.work_area(), gfx::Rect(100, 100, 800, 600));
  EXPECT_EQ(display_.device_scale_factor(), 2.0f);
}

TEST_F(HeadlessDisplayGeometryTest, Scaled2xDisplayWorkArea) {
  SetDisplayGeometry(gfx::Rect(100, 100, 1600, 1200), gfx::Insets(10), 2.0f);

  EXPECT_EQ(display_.bounds(), gfx::Rect(100, 100, 800, 600));
  EXPECT_EQ(display_.work_area(), gfx::Rect(105, 105, 790, 590));
  EXPECT_EQ(display_.device_scale_factor(), 2.0f);
}

}  // namespace
}  // namespace display
