// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/decoration/shadow.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/decoration/decoration.h"
#include "ui/decoration/decoration_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/shadow_value.h"

namespace ui::decoration {
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
  auto values = Shadow::MakeShadowValues(
      elevation, Shadow::Style::kMaterialDesign, std::nullopt, is_pill_shaped);
  gfx::Rect bounds(0, 0, 1, 1);
  bounds.Inset(
      -ShadowGenerator::GetNineboxApertureInsets(values, rounded_corners));
  return bounds.size();
}

// Calculates the minimum shadow content size for given elevation and corner
// radius.
gfx::Size GetMinContentSize(
    int elevation,
    const gfx::RoundedCornersF& rounded_corners = gfx::RoundedCornersF(),
    bool is_pill_shaped = false) {
  auto values = Shadow::MakeShadowValues(
      elevation, Shadow::Style::kMaterialDesign, std::nullopt, is_pill_shaped);
  gfx::Insets insets =
      ShadowGenerator::GetNineboxApertureInsets(values, rounded_corners);
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
// Decoration::CreateShadow() yields a decoration drawn by a shadow configured
// as requested, with slightly rounded corners.
TEST_F(ShadowTest, CreateShadow) {
  const Shadow::ElevationToColorsMap color_map = {
      {kElevationSmall, {SK_ColorRED, SK_ColorBLUE}}};
  auto decoration = Decoration::CreateShadow(
      kElevationSmall, Shadow::Style::kMaterialDesign, color_map);

  const Shadow* shadow = decoration->GetSourceAs<Shadow>();
  ASSERT_TRUE(shadow);
  EXPECT_EQ(kElevationSmall, shadow->elevation());
  EXPECT_EQ(Shadow::Style::kMaterialDesign, shadow->style());
  EXPECT_EQ(color_map, shadow->color_map());
  EXPECT_EQ(gfx::RoundedCornersF(2.f), decoration->rounded_corners());
}

TEST_F(ShadowTest, SetContentBounds) {
  gfx::ScopedAnimationDurationScaleMode zero_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  // Verify that layer bounds are outset from content bounds.
  auto decoration = Decoration::CreateShadow(kElevationLarge);
  Shadow* shadow = decoration->GetSourceAs<Shadow>();
  {
    gfx::Rect content_bounds(100, 100, 300, 300);
    decoration->SetContentBounds(content_bounds);
    EXPECT_EQ(content_bounds, decoration->content_bounds());
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation(kElevationLarge));
    EXPECT_EQ(shadow_bounds, decoration->layer()->bounds());
  }

  {
    shadow->SetElevation(kElevationSmall);
    gfx::Rect content_bounds(100, 100, 300, 300);
    decoration->SetContentBounds(content_bounds);
    EXPECT_EQ(content_bounds, decoration->content_bounds());
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation(kElevationSmall));
    EXPECT_EQ(shadow_bounds, decoration->layer()->bounds());
  }
}

// Test that no nine-patch image is generated while the size-adjusted elevation
// is zero, and that images keep being generated once one has been created.
TEST_F(ShadowTest, ZeroElevationDoesNotGenerateDetails) {
  gfx::ScopedAnimationDurationScaleMode zero_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // A shadow that has never had a positive elevation generates no details, and
  // the decoration layer is left unconfigured, i.e. not outset by any margins.
  auto decoration = Decoration::CreateShadow(0);
  Shadow* shadow = decoration->GetSourceAs<Shadow>();
  const gfx::Rect content_bounds(100, 100, 300, 300);
  decoration->SetContentBounds(content_bounds);
  EXPECT_FALSE(shadow->details_for_testing());
  EXPECT_EQ(content_bounds, decoration->layer()->bounds());

  // The elevation is also clamped to zero when the content is too small to
  // support it, which likewise generates no details.
  auto small_decoration = Decoration::CreateShadow(kElevationLarge);
  Shadow* small_shadow = small_decoration->GetSourceAs<Shadow>();
  const gfx::Rect small_content_bounds(0, 0, 2, 2);
  small_decoration->SetContentBounds(small_content_bounds);
  EXPECT_FALSE(small_shadow->details_for_testing());
  EXPECT_EQ(small_content_bounds, small_decoration->layer()->bounds());

  // Once a positive elevation has generated details, returning to a zero
  // elevation keeps painting the shadow rather than dropping the image.
  shadow->SetElevation(kElevationSmall);
  ASSERT_TRUE(shadow->details_for_testing());
  shadow->SetElevation(0);
  EXPECT_TRUE(shadow->details_for_testing());
}

// Test that calling setters on a standalone Shadow before attaching to a
// Decoration does not crash and properties are properly applied when attached.
TEST_F(ShadowTest, ConfigureBeforeAttach) {
  auto shadow = std::make_unique<Shadow>(/*elevation=*/0);
  Shadow* shadow_ptr = shadow.get();

  shadow->SetElevation(kElevationSmall);
  EXPECT_EQ(kElevationSmall, shadow->elevation());

  Shadow::ElevationToColorsMap color_map;
  color_map[kElevationLarge] =
      Shadow::ElevationColors{SK_ColorRED, SK_ColorBLUE};
  shadow->SetColorMap(color_map);
  EXPECT_EQ(color_map, shadow->color_map());

  shadow->SetStyle(Shadow::Style::kMaterialDesign);
  EXPECT_EQ(Shadow::Style::kMaterialDesign, shadow->style());
  EXPECT_FALSE(shadow->details_for_testing());

  shadow->SetElevation(kElevationLarge);

  auto decoration = Decoration::Create(std::move(shadow));
  const gfx::Rect content_bounds(100, 100, 300, 300);
  const gfx::RoundedCornersF radii(10, 20, 30, 40);
  decoration->SetContentBounds(content_bounds);
  decoration->SetRoundedCorners(radii);

  EXPECT_TRUE(decoration->layer());
  EXPECT_TRUE(decoration->decoration_layer_for_testing());
  ASSERT_TRUE(shadow_ptr->details_for_testing());

  gfx::Rect shadow_bounds(content_bounds);
  shadow_bounds.Inset(InsetsForElevation(kElevationLarge));
  EXPECT_EQ(shadow_bounds, decoration->layer()->bounds());
  EXPECT_EQ(GetNineboxImageSize(kElevationLarge, radii),
            shadow_ptr->details_for_testing()->nine_patch_image.size());
  EXPECT_EQ(SK_ColorRED, shadow_ptr->details_for_testing()->spec[0].color());
  EXPECT_EQ(SK_ColorBLUE, shadow_ptr->details_for_testing()->spec[1].color());
}

// Test that the elevation is reduced when the contents are too small to handle
// the full elevation.
TEST_F(ShadowTest, AdjustElevationForSmallContents) {
  auto decoration = Decoration::CreateShadow(kElevationLarge);

  // Test with corner radius 0.
  gfx::RoundedCornersF radii;
  {
    gfx::Rect content_bounds(100, 100, 300, 300);
    decoration->SetContentBounds(content_bounds);
    decoration->SetRoundedCorners(radii);
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation(kElevationLarge));
    EXPECT_EQ(shadow_bounds, decoration->layer()->bounds());
  }

  {
    constexpr int kWidth = 80;
    gfx::Rect content_bounds(100, 100, kWidth, 300);
    decoration->SetContentBounds(content_bounds);
    decoration->SetRoundedCorners(radii);
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation(kWidth / 4));
    EXPECT_EQ(shadow_bounds, decoration->layer()->bounds());
  }

  {
    constexpr int kHeight = 80;
    gfx::Rect content_bounds(100, 100, 300, kHeight);
    decoration->SetContentBounds(content_bounds);
    decoration->SetRoundedCorners(radii);
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation(kHeight / 4));
    EXPECT_EQ(shadow_bounds, decoration->layer()->bounds());
  }

  // Test with default corner radius 2.
  radii = gfx::RoundedCornersF(2);
  {
    constexpr int kWidth = 80;
    gfx::Rect content_bounds(100, 100, kWidth, 300);
    decoration->SetContentBounds(content_bounds);
    decoration->SetRoundedCorners(radii);
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation((kWidth - 4) / 4));
    EXPECT_EQ(shadow_bounds, decoration->layer()->bounds());
  }

  {
    constexpr int kHeight = 80;
    gfx::Rect content_bounds(100, 100, 300, kHeight);
    decoration->SetContentBounds(content_bounds);
    decoration->SetRoundedCorners(radii);
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation((kHeight - 4) / 4));
    EXPECT_EQ(shadow_bounds, decoration->layer()->bounds());
  }

  // Test with pill shaped contents.
  radii = gfx::RoundedCornersF(40);
  {
    constexpr int kWidth = 80;
    gfx::Rect content_bounds(100, 100, kWidth, 300);
    decoration->SetContentBounds(content_bounds);
    decoration->SetRoundedCorners(radii);
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation(kWidth / 4));
    EXPECT_EQ(shadow_bounds, decoration->layer()->bounds());
  }

  // Test with variable rounded corners.
  radii = gfx::RoundedCornersF(10, 20, 30, 40);
  {
    constexpr int kWidth = 100;
    gfx::Rect content_bounds(100, 100, kWidth, 300);
    decoration->SetContentBounds(content_bounds);
    decoration->SetRoundedCorners(radii);
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation((kWidth - 2 * 40) / 4));
    EXPECT_EQ(shadow_bounds, decoration->layer()->bounds());
  }

  // Test with variable rounded corners that trigger pill-shape clamping.
  radii = gfx::RoundedCornersF(40, 40, 20, 20);
  {
    constexpr int kWidth = 80;
    gfx::Rect content_bounds(100, 100, kWidth, 300);
    decoration->SetContentBounds(content_bounds);
    decoration->SetRoundedCorners(radii);
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation(kWidth / 4));
    EXPECT_EQ(shadow_bounds, decoration->layer()->bounds());
  }
}

// Test that rounded corners are handled correctly.
TEST_F(ShadowTest, AdjustRoundedCorners) {
  auto decoration = Decoration::CreateShadow(kElevationSmall);
  Shadow* shadow = decoration->GetSourceAs<Shadow>();
  gfx::Rect content_bounds(100, 100, 300, 300);
  decoration->SetContentBounds(content_bounds);
  EXPECT_EQ(content_bounds, decoration->content_bounds());

  decoration->SetContentBounds(content_bounds);
  decoration->SetRoundedCorners(gfx::RoundedCornersF());
  gfx::Rect shadow_bounds(content_bounds);
  shadow_bounds.Inset(InsetsForElevation(kElevationSmall));
  EXPECT_EQ(shadow_bounds, decoration->layer()->bounds());
  EXPECT_EQ(GetNineboxImageSize(6, gfx::RoundedCornersF()),
            shadow->details_for_testing()->nine_patch_image.size());

  gfx::RoundedCornersF radii(10, 20, 30, 40);
  decoration->SetContentBounds(content_bounds);
  decoration->SetRoundedCorners(radii);
  EXPECT_EQ(shadow_bounds, decoration->layer()->bounds());
  EXPECT_EQ(GetNineboxImageSize(6, radii),
            shadow->details_for_testing()->nine_patch_image.size());

  decoration->SetContentBounds(content_bounds);
  decoration->SetRoundedCorners(gfx::RoundedCornersF(150));
  EXPECT_EQ(GetNineboxImageSize(6, gfx::RoundedCornersF(150),
                                /*is_pill_shaped=*/true),
            shadow->details_for_testing()->nine_patch_image.size());
}

// Test that the uniquely owned shadow image is evicted from the cache when new
// shadow details are created.
TEST_F(ShadowTest, EvictUniquelyOwnedDetail) {
  // Insert a new shadow with unique details which will evict existing details
  // from the cache.
  {
    auto new_decoration = Decoration::CreateShadow(kElevationUnique);

    const gfx::Size min_content_size = GetMinContentSize(kElevationUnique);
    new_decoration->SetContentBounds(gfx::Rect(min_content_size));
    // The cache size should be 1.
    EXPECT_EQ(1u, ShadowDetails::GetDetailsCacheSizeForTest());

    // Creating a shadow with the same detail won't increase the cache size.
    auto same_decoration = Decoration::CreateShadow(kElevationUnique);
    same_decoration->SetContentBounds(
        gfx::Rect(gfx::Point(10, 10), min_content_size + gfx::Size(50, 50)));
    // The cache size is unchanged.
    EXPECT_EQ(1u, ShadowDetails::GetDetailsCacheSizeForTest());

    // Creating a new uniquely owned detail will increase the cache size.
    ShadowDetails::Get(gfx::RoundedCornersF(3),
                       Shadow::MakeShadowValues(kElevationUnique));
    EXPECT_EQ(2u, ShadowDetails::GetDetailsCacheSizeForTest());

    // Creating a shadow with different details will replace the uniquely owned
    // detail.
    auto small_decoration = Decoration::CreateShadow(kElevationSmall);
    const gfx::Rect small_content_bounds(GetMinContentSize(kElevationSmall));
    small_decoration->SetContentBounds(small_content_bounds);
    EXPECT_EQ(2u, ShadowDetails::GetDetailsCacheSizeForTest());

    // Changing the shadow appearance will insert a new detail in the cache and
    // make the old detail uniquely owned.
    small_decoration->SetContentBounds(small_content_bounds);
    small_decoration->SetRoundedCorners(gfx::RoundedCornersF(3));
    EXPECT_EQ(3u, ShadowDetails::GetDetailsCacheSizeForTest());

    // Changing the shadow with another appearance will replace the uniquely
    // owned detail.
    small_decoration->SetContentBounds(small_content_bounds);
    small_decoration->SetRoundedCorners(gfx::RoundedCornersF(4));
    EXPECT_EQ(3u, ShadowDetails::GetDetailsCacheSizeForTest());

    // Changing the shadow to be pill shaped will replace the uniquely owned
    // detail.
    small_decoration->SetContentBounds(
        gfx::Rect(GetMinContentSize(kElevationSmall, gfx::RoundedCornersF(14),
                                    /*is_pill_shaped=*/true)));
    small_decoration->SetRoundedCorners(gfx::RoundedCornersF(14));
    EXPECT_EQ(3u, ShadowDetails::GetDetailsCacheSizeForTest());
  }

  // After destroying all the shadows, the cache has 3 uniquely owned details.
  EXPECT_EQ(3u, ShadowDetails::GetDetailsCacheSizeForTest());

  // After inserting a new detail, the uniquely owned details will be evicted.
  auto large_decoration = Decoration::CreateShadow(kElevationLarge);
  large_decoration->SetContentBounds(
      gfx::Rect(GetMinContentSize(kElevationLarge)));
  // The cache size is unchanged.
  EXPECT_EQ(1u, ShadowDetails::GetDetailsCacheSizeForTest());
}

class ShadowColorTest : public ShadowTest,
                        public testing::WithParamInterface<Shadow::Style> {
 public:
  ShadowColorTest() = default;
  ShadowColorTest(const ShadowColorTest&) = delete;
  ShadowColorTest& operator=(const ShadowColorTest&) = delete;
  ~ShadowColorTest() override = default;

  static std::vector<Shadow::Style> GetTestParamValues() {
#if BUILDFLAG(IS_CHROMEOS)
    return {Shadow::Style::kMaterialDesign, Shadow::Style::kChromeOSSystemUI};
#else
    return {Shadow::Style::kMaterialDesign};
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
  auto decoration = Decoration::CreateShadow(kElevationSmall, GetParam());
  Shadow* shadow = decoration->GetSourceAs<Shadow>();
  // Set the content bounds which is big enough for the large elevation.
  decoration->SetContentBounds(gfx::Rect(GetMinContentSize(kElevationLarge)));

  // Cache the default colors.
  const auto& values = shadow->details_for_testing()->spec;
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
  shadow->SetColorMap(color_map);

  // A lambda to get key and ambient shadow colors.
  auto get_colors = [](const Shadow& shadow) -> ElevationColors {
    const auto& values = shadow.details_for_testing()->spec;
    return ElevationColors{values[0].color(), values[1].color()};
  };

  // Check if shadow colors are updated.
  EXPECT_EQ(get_colors(*shadow),
            (ElevationColors{small_key_color, small_ambient_color}));

  // Check if shadow colors are updated when the shadow changes to another
  // specified elevation.
  shadow->SetElevation(kElevationLarge);
  EXPECT_EQ(get_colors(*shadow),
            (ElevationColors{large_key_color, large_ambient_color}));

  // Check if the shadow colors change back to default colors when the shadow
  // changes to a non-specified elevation.
  shadow->SetElevation(kElevationSmall + 1);
  EXPECT_EQ(get_colors(*shadow),
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
}  // namespace ui::decoration
