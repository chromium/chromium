// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor_extra/shadow.h"

#include "base/test/test_discardable_memory_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/shadow_util.h"
#include "ui/gfx/shadow_value.h"

namespace ui {
namespace {

constexpr int kElevationLarge = 24;
constexpr int kElevationSmall = 6;

gfx::Insets InsetsForElevation(int elevation) {
  return -gfx::Insets(2 * elevation) +
         gfx::Insets::TLBR(elevation, 0, -elevation, 0);
}

gfx::Size NineboxImageSizeForElevationAndCornerRadius(int elevation,
                                                      int corner_radius) {
  auto values = gfx::ShadowValue::MakeMdShadowValues(elevation);
  gfx::Rect bounds(0, 0, 1, 1);
  bounds.Inset(-gfx::ShadowValue::GetBlurRegion(values));
  bounds.Inset(-gfx::Insets(corner_radius));
  return bounds.size();
}

class ShadowTest : public testing::Test {
 public:
  ShadowTest(const ShadowTest&) = delete;
  ShadowTest& operator=(const ShadowTest&) = delete;

 protected:
  ShadowTest() {}
  ~ShadowTest() override {}

  void SetUp() override {
    base::DiscardableMemoryAllocator::SetInstance(
        &discardable_memory_allocator_);
  }

  void TearDown() override {
    base::DiscardableMemoryAllocator::SetInstance(nullptr);
  }

 private:
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;
};

// Test if the proper content bounds is calculated based on the current style.
TEST_F(ShadowTest, SetContentBounds) {
  ScopedAnimationDurationScaleMode zero_duration_mode(
      ScopedAnimationDurationScaleMode::ZERO_DURATION);
  // Verify that layer bounds are outset from content bounds.
  Shadow shadow;
  {
    shadow.Init(kElevationLarge);
    gfx::Rect content_bounds(100, 100, 300, 300);
    shadow.SetContentBounds(content_bounds);
    EXPECT_EQ(content_bounds, shadow.content_bounds());
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation(kElevationLarge));
    EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  }

  {
    shadow.SetElevation(kElevationSmall);
    gfx::Rect content_bounds(100, 100, 300, 300);
    shadow.SetContentBounds(content_bounds);
    EXPECT_EQ(content_bounds, shadow.content_bounds());
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation(kElevationSmall));
    EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  }
}

// Test that the elevation is reduced when the contents are too small to handle
// the full elevation.
TEST_F(ShadowTest, AdjustElevationForSmallContents) {
  Shadow shadow;
  shadow.Init(kElevationLarge);
  {
    gfx::Rect content_bounds(100, 100, 300, 300);
    shadow.SetContentBounds(content_bounds);
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation(kElevationLarge));
    EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  }

  {
    constexpr int kWidth = 80;
    gfx::Rect content_bounds(100, 100, kWidth, 300);
    shadow.SetContentBounds(content_bounds);
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation((kWidth - 4) / 4));
    EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  }

  {
    constexpr int kHeight = 80;
    gfx::Rect content_bounds(100, 100, 300, kHeight);
    shadow.SetContentBounds(content_bounds);
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation((kHeight - 4) / 4));
    EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  }
}

// Test that rounded corner radius is handled correctly.
TEST_F(ShadowTest, AdjustRoundedCornerRadius) {
  Shadow shadow;
  shadow.Init(kElevationSmall);
  gfx::Rect content_bounds(100, 100, 300, 300);
  shadow.SetContentBounds(content_bounds);
  EXPECT_EQ(content_bounds, shadow.content_bounds());
  shadow.SetRoundedCornerRadius(0);
  gfx::Rect shadow_bounds(content_bounds);
  shadow_bounds.Inset(InsetsForElevation(kElevationSmall));
  EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  EXPECT_EQ(NineboxImageSizeForElevationAndCornerRadius(6, 0),
            shadow.details_for_testing()->ninebox_image.size());
}

}  // namespace
}  // namespace ui
