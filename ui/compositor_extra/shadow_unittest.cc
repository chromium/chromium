// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor_extra/shadow.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor_extra/decoration_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/shadow_value.h"

namespace ui {
namespace {

using ::testing::FieldsAre;

constexpr int kElevationLarge = 24;
constexpr int kElevationSmall = 6;

// A specific elevation used for testing EvictUniquelyOwnedDetail.
constexpr int kElevationUnique = 66;

gfx::Insets InsetsForElevation(int elevation) {
  return -gfx::Insets(2 * elevation) +
         gfx::Insets::TLBR(elevation, 0, -elevation, 0);
}

gfx::Size GetNineboxImageSize(int elevation,
                              const gfx::RoundedCornersF& rounded_corners,
                              bool is_pill_shaped = false) {
  auto values = gfx::ShadowValue::MakeMdShadowValues(elevation, SK_ColorBLACK,
                                                     is_pill_shaped);
  gfx::Rect bounds(0, 0, 1, 1);
  bounds.Inset(
      -gfx::ShadowDetails::GetNineboxApertureInsets(values, rounded_corners));
  return bounds.size();
}

// Calculates the minimum shadow content size for given elevation and corner
// radius.
gfx::Size GetMinContentSize(
    int elevation,
    const gfx::RoundedCornersF& rounded_corners = gfx::RoundedCornersF(),
    bool is_pill_shaped = false) {
  auto values = gfx::ShadowValue::MakeMdShadowValues(elevation, SK_ColorBLACK,
                                                     is_pill_shaped);
  gfx::Insets insets =
      gfx::ShadowDetails::GetNineboxApertureInsets(values, rounded_corners);
  return gfx::Size(insets.width(), insets.height());
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
  gfx::ScopedAnimationDurationScaleMode zero_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);
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

// Test if the shadow's layer bounds are modified, setting the same content
// bounds can reset the layer bounds.
TEST_F(ShadowTest, ResetLayerBoundsBySettingSameContentBounds) {
  Shadow shadow;
  shadow.Init(kElevationLarge);
  gfx::Rect content_bounds(100, 100, 300, 300);
  shadow.SetContentBounds(content_bounds);
  EXPECT_EQ(content_bounds, shadow.content_bounds());

  const gfx::Rect layer_bounds = shadow.layer()->bounds();

  // Change shadow's layer bounds.
  const gfx::Rect modified_bounds(200, 200, 150, 400);
  shadow.layer()->SetBounds(modified_bounds);
  EXPECT_EQ(shadow.layer()->bounds(), modified_bounds);

  // Reset layer bounds by setting the same content bounds.
  shadow.SetContentBounds(content_bounds);
  EXPECT_EQ(layer_bounds, shadow.layer()->bounds());
}

// Test that calling setters before Init() does not crash and properties are
// properly applied when Init() is called.
TEST_F(ShadowTest, ConfigureBeforeInit) {
  Shadow shadow;

  // Set properties before Init(). These should not crash or create layers.
  const gfx::Rect content_bounds(100, 100, 300, 300);
  shadow.SetContentBounds(content_bounds);
  EXPECT_EQ(content_bounds, shadow.content_bounds());

  shadow.SetElevation(kElevationSmall);
  EXPECT_EQ(kElevationSmall, shadow.elevation());

  const gfx::RoundedCornersF radii(10, 20, 30, 40);
  shadow.SetRoundedCorners(radii);
  EXPECT_EQ(radii, shadow.rounded_corners());

  Shadow::ElevationToColorsMap color_map;
  color_map[kElevationLarge] =
      Shadow::ElevationColors{SK_ColorRED, SK_ColorBLUE};
  shadow.SetColorMap(color_map);
  EXPECT_EQ(color_map, shadow.color_map());

  shadow.SetStyle(Shadow::Style::kMaterialDesign);
  EXPECT_EQ(Shadow::Style::kMaterialDesign, shadow.style());

  EXPECT_FALSE(shadow.layer());
  EXPECT_FALSE(shadow.shadow_layer());
  EXPECT_FALSE(shadow.details_for_testing());

  // Call Init() and verify that the shadow appearance is correctly updated with
  // the previously configured properties.
  shadow.Init(kElevationLarge);
  EXPECT_TRUE(shadow.layer());
  EXPECT_TRUE(shadow.shadow_layer());
  ASSERT_TRUE(shadow.details_for_testing());

  gfx::Rect shadow_bounds(content_bounds);
  shadow_bounds.Inset(InsetsForElevation(kElevationLarge));
  EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  EXPECT_EQ(GetNineboxImageSize(kElevationLarge, radii),
            shadow.details_for_testing()->nine_patch_image.size());
  EXPECT_EQ(SK_ColorRED, shadow.details_for_testing()->values[0].color());
  EXPECT_EQ(SK_ColorBLUE, shadow.details_for_testing()->values[1].color());
}

// Test that the elevation is reduced when the contents are too small to handle
// the full elevation.
TEST_F(ShadowTest, AdjustElevationForSmallContents) {
  Shadow shadow;
  shadow.Init(kElevationLarge);

  // Test with corner radius 0.
  shadow.SetRoundedCorners(gfx::RoundedCornersF());
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
    shadow_bounds.Inset(InsetsForElevation(kWidth / 4));
    EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  }

  {
    constexpr int kHeight = 80;
    gfx::Rect content_bounds(100, 100, 300, kHeight);
    shadow.SetContentBounds(content_bounds);
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation(kHeight / 4));
    EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  }

  // Test with default corner radius 2.
  shadow.SetRoundedCorners(gfx::RoundedCornersF(2));
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

  // Test with pill shaped contents.
  shadow.SetRoundedCorners(gfx::RoundedCornersF(40));
  {
    constexpr int kWidth = 80;
    gfx::Rect content_bounds(100, 100, kWidth, 300);
    shadow.SetContentBounds(content_bounds);
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation(kWidth / 4));
    EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  }

  // Test with variable rounded corners.
  shadow.SetRoundedCorners(gfx::RoundedCornersF(10, 20, 30, 40));
  {
    constexpr int kWidth = 100;
    gfx::Rect content_bounds(100, 100, kWidth, 300);
    shadow.SetContentBounds(content_bounds);
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation((kWidth - 2 * 40) / 4));
    EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  }

  // Test with variable rounded corners that trigger pill-shape clamping.
  shadow.SetRoundedCorners(gfx::RoundedCornersF(40, 40, 20, 20));
  {
    constexpr int kWidth = 80;
    gfx::Rect content_bounds(100, 100, kWidth, 300);
    shadow.SetContentBounds(content_bounds);
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation(kWidth / 4));
    EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  }
}

// Test that rounded corners are handled correctly.
TEST_F(ShadowTest, AdjustRoundedCorners) {
  Shadow shadow;
  shadow.Init(kElevationSmall);
  gfx::Rect content_bounds(100, 100, 300, 300);
  shadow.SetContentBounds(content_bounds);
  EXPECT_EQ(content_bounds, shadow.content_bounds());

  shadow.SetRoundedCorners(gfx::RoundedCornersF());
  gfx::Rect shadow_bounds(content_bounds);
  shadow_bounds.Inset(InsetsForElevation(kElevationSmall));
  EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  EXPECT_EQ(GetNineboxImageSize(6, gfx::RoundedCornersF()),
            shadow.details_for_testing()->nine_patch_image.size());

  gfx::RoundedCornersF radii(10, 20, 30, 40);
  shadow.SetRoundedCorners(radii);
  EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  EXPECT_EQ(GetNineboxImageSize(6, radii),
            shadow.details_for_testing()->nine_patch_image.size());

  shadow.SetRoundedCorners(gfx::RoundedCornersF(150));
  EXPECT_EQ(GetNineboxImageSize(6, gfx::RoundedCornersF(150),
                                /*is_pill_shaped=*/true),
            shadow.details_for_testing()->nine_patch_image.size());
}

// Test that rounded corners are size-adjusted using floor precision when
// content bounds are small to support rounded corners.
TEST_F(ShadowTest, SizeAdjustedRoundedCorners) {
  Shadow shadow;
  shadow.Init(kElevationSmall);

  // Set rounded corners where some corners exceed half the smaller dimension
  // of the content bounds below (smaller_dimension = 39, half = 19.5).
  gfx::RoundedCornersF radii(10, 24, 24, 24);
  shadow.SetRoundedCorners(radii);

  // Set small content bounds (width = 50, height = 39, smaller_dimension = 39).
  // Max radius with floor precision: std::floor(39 / 2.0f) = 19.0.
  gfx::Rect small_content_bounds(100, 100, 50, 39);
  shadow.SetContentBounds(small_content_bounds);

  const gfx::RoundedCornersF expected_adjusted_radii(10, 19, 19, 19);
  EXPECT_EQ(GetNineboxImageSize(kElevationSmall, expected_adjusted_radii,
                                /*is_pill_shaped=*/true),
            shadow.details_for_testing()->nine_patch_image.size());
}

// Test that the uniquely owned shadow image is evicted from the cache when new
// shadow details are created.
TEST_F(ShadowTest, EvictUniquelyOwnedDetail) {
  // Insert a new shadow with unique details which will evict existing details
  // from the cache.
  {
    Shadow shadow_new;
    shadow_new.Init(kElevationUnique);
    shadow_new.SetRoundedCorners(gfx::RoundedCornersF(2));

    const gfx::Size min_content_size = GetMinContentSize(kElevationUnique);
    shadow_new.SetContentBounds(gfx::Rect(min_content_size));
    // The cache size should be 1.
    EXPECT_EQ(1u, gfx::ShadowDetails::GetDetailsCacheSizeForTest());

    // Creating a shadow with the same detail won't increase the cache size.
    Shadow shadow_same;
    shadow_same.Init(kElevationUnique);
    shadow_same.SetRoundedCorners(gfx::RoundedCornersF(2));
    shadow_same.SetContentBounds(
        gfx::Rect(gfx::Point(10, 10), min_content_size + gfx::Size(50, 50)));
    // The cache size is unchanged.
    EXPECT_EQ(1u, gfx::ShadowDetails::GetDetailsCacheSizeForTest());

    // Creating a new uniquely owned detail will increase the cache size.
    gfx::ShadowDetails::Get(kElevationUnique, gfx::RoundedCornersF(3));
    EXPECT_EQ(2u, gfx::ShadowDetails::GetDetailsCacheSizeForTest());

    // Creating a shadow with different details will replace the uniquely owned
    // detail.
    Shadow shadow_small;
    shadow_small.Init(kElevationSmall);
    shadow_small.SetRoundedCorners(gfx::RoundedCornersF(2));
    shadow_small.SetContentBounds(
        gfx::Rect(GetMinContentSize(kElevationSmall)));
    EXPECT_EQ(2u, gfx::ShadowDetails::GetDetailsCacheSizeForTest());

    // Changing the shadow appearance will insert a new detail in the cache and
    // make the old detail uniquely owned.
    shadow_small.SetRoundedCorners(gfx::RoundedCornersF(3));
    EXPECT_EQ(3u, gfx::ShadowDetails::GetDetailsCacheSizeForTest());

    // Changing the shadow with another appearance will replace the uniquely
    // owned detail.
    shadow_small.SetRoundedCorners(gfx::RoundedCornersF(4));
    EXPECT_EQ(3u, gfx::ShadowDetails::GetDetailsCacheSizeForTest());

    // Changing the shadow to be pill shaped will replace the uniquely owned
    // detail.
    shadow_small.SetContentBounds(gfx::Rect(GetMinContentSize(
        kElevationSmall, gfx::RoundedCornersF(14), /*is_pill_shaped=*/true)));
    shadow_small.SetRoundedCorners(gfx::RoundedCornersF(14));
    EXPECT_EQ(3u, gfx::ShadowDetails::GetDetailsCacheSizeForTest());
  }

  // After destroying the all the shadows, the cache has 3 uniquely owned
  // details.
  EXPECT_EQ(3u, gfx::ShadowDetails::GetDetailsCacheSizeForTest());

  // After inserting a new detail, the uniquely owned details will be evicted.
  Shadow shadow_large;
  shadow_large.Init(kElevationLarge);
  shadow_large.SetRoundedCorners(gfx::RoundedCornersF(2));
  shadow_large.SetContentBounds(gfx::Rect(GetMinContentSize(kElevationLarge)));
  // The cache size is unchanged.
  EXPECT_EQ(1u, gfx::ShadowDetails::GetDetailsCacheSizeForTest());
}

class ShadowColorTest : public ShadowTest,
                        public testing::WithParamInterface<ui::Shadow::Style> {
 public:
  ShadowColorTest() = default;
  ShadowColorTest(const ShadowColorTest&) = delete;
  ShadowColorTest& operator=(const ShadowColorTest&) = delete;
  ~ShadowColorTest() override = default;

  static std::vector<ui::Shadow::Style> GetTestParamValues() {
#if BUILDFLAG(IS_CHROMEOS)
    return {ui::Shadow::Style::kMaterialDesign,
            ui::Shadow::Style::kChromeOSSystemUI};
#else
    return {ui::Shadow::Style::kMaterialDesign};
#endif
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ShadowColorTest,
    testing::ValuesIn(ShadowColorTest::GetTestParamValues()));

// Tests the shadow colors are updated when setting elevation to colors map.
TEST_P(ShadowColorTest, ElevationToColorsMap) {
  using ElevationColors = Shadow::ElevationColors;
  Shadow shadow;
  shadow.Init(kElevationSmall);
  shadow.SetStyle(GetParam());
  // Set the content bounds which is big enough for the large elevation.
  shadow.SetContentBounds(gfx::Rect(GetMinContentSize(kElevationLarge)));

  // Cache the default colors.
  const auto& values = shadow.details_for_testing()->values;
  const SkColor default_key_color = values[0].color();
  const SkColor default_ambient_color = values[1].color();

  // Set a color map.
  const SkColor small_key_color = SkColorSetA(SK_ColorRED, 0x3d);
  const SkColor small_ambient_color = SkColorSetA(SK_ColorBLUE, 0x1a);
  const SkColor large_key_color = SkColorSetA(SK_ColorGREEN, 0x41);
  const SkColor large_ambient_color = SkColorSetA(SK_ColorYELLOW, 0x26);
  Shadow::ElevationToColorsMap color_map;
  color_map[kElevationSmall] =
      ElevationColors{small_key_color, small_ambient_color};
  color_map[kElevationLarge] =
      ElevationColors{large_key_color, large_ambient_color};
  shadow.SetColorMap(color_map);

  // A lambda to get key and ambient shadow colors.
  auto get_colors = [](const ui::Shadow& shadow) -> ElevationColors {
    const auto& values = shadow.details_for_testing()->values;
    return ElevationColors{values[0].color(), values[1].color()};
  };

  // Check if shadow colors are updated.
  EXPECT_EQ(get_colors(shadow),
            (ElevationColors{small_key_color, small_ambient_color}));

  // Check if shadow colors are updated when the shadow changes to another
  // specified elevation.
  shadow.SetElevation(kElevationLarge);
  EXPECT_EQ(get_colors(shadow),
            (ElevationColors{large_key_color, large_ambient_color}));

  // Check if the shadow colors change back to default colors when the shadow
  // changes to a non-specified elevation.
  shadow.SetElevation(kElevationSmall + 1);
  EXPECT_EQ(get_colors(shadow),
            (ElevationColors{default_key_color, default_ambient_color}));
}

// Tests MakeShadowValues static method with default and custom colors.
TEST(ShadowStaticTest, MakeShadowValues) {
  constexpr int kElevation = 6;
  const gfx::ShadowValues default_md_values =
      Shadow::MakeShadowValues(kElevation, Shadow::Style::kMaterialDesign);
  EXPECT_EQ(default_md_values,
            gfx::ShadowValue::MakeMdShadowValues(kElevation, SK_ColorBLACK));

  const SkColor key_color = SK_ColorRED;
  const SkColor ambient_color = SK_ColorBLUE;
  const Shadow::ElevationColors colors{key_color, ambient_color};
  const gfx::ShadowValues custom_md_values = Shadow::MakeShadowValues(
      kElevation, Shadow::Style::kMaterialDesign, colors);
  EXPECT_EQ(custom_md_values, gfx::ShadowValue::MakeMdShadowValues(
                                  kElevation, key_color, ambient_color));

#if BUILDFLAG(IS_CHROMEOS)
  const gfx::ShadowValues default_cros_values =
      Shadow::MakeShadowValues(kElevation, Shadow::Style::kChromeOSSystemUI);
  EXPECT_EQ(default_cros_values,
            gfx::ShadowValue::MakeChromeOSSystemUIShadowValues(kElevation,
                                                               SK_ColorBLACK));

  const gfx::ShadowValues custom_cros_values = Shadow::MakeShadowValues(
      kElevation, Shadow::Style::kChromeOSSystemUI, colors);
  EXPECT_EQ(custom_cros_values,
            gfx::ShadowValue::MakeChromeOSSystemUIShadowValues(
                kElevation, key_color, ambient_color));
#endif
}

}  // namespace
}  // namespace ui
