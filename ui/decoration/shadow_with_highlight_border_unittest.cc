// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/decoration/shadow_with_highlight_border.h"

#include <memory>
#include <optional>

#include "base/memory/discardable_memory_allocator.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_nine_patch.h"
#include "ui/decoration/decoration.h"
#include "ui/decoration/decoration_source.h"
#include "ui/decoration/decoration_util.h"
#include "ui/decoration/highlight_border.h"
#include "ui/decoration/shadow.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"

namespace ui::decoration {
namespace {

constexpr int kElevation = 6;
constexpr int kThickness = 1;
constexpr SkColor kHighlightColor = SK_ColorGREEN;
constexpr SkColor kBorderColor = SK_ColorBLUE;

class ShadowWithHighlightBorderTest : public testing::Test {
 public:
  ShadowWithHighlightBorderTest() = default;
  ShadowWithHighlightBorderTest(const ShadowWithHighlightBorderTest&) = delete;
  ShadowWithHighlightBorderTest& operator=(
      const ShadowWithHighlightBorderTest&) = delete;
  ~ShadowWithHighlightBorderTest() override = default;

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

// When neither effect is present or visible, no ninebox image is generated.
TEST_F(ShadowWithHighlightBorderTest, NoDetailsWhenNeitherEffectIsVisible) {
  const gfx::Rect content_bounds(0, 0, 300, 300);
  const gfx::RoundedCornersF corners(10);

  // Both shadow and highlight border are null.
  ShadowWithHighlightBorder empty_source;
  EXPECT_FALSE(empty_source.shadow());
  EXPECT_FALSE(empty_source.highlight_border());
  EXPECT_FALSE(empty_source.GetDetails(content_bounds, corners).has_value());

  // Zero-elevation shadow and no highlight border.
  ShadowWithHighlightBorder zero_shadow_source(
      std::make_unique<Shadow>(/*elevation=*/0), nullptr);
  EXPECT_FALSE(
      zero_shadow_source.GetDetails(content_bounds, corners).has_value());

  // Content too small for both shadow and highlight border.
  ShadowWithHighlightBorder full_source(
      std::make_unique<Shadow>(kElevation),
      std::make_unique<HighlightBorder>(kHighlightColor, kBorderColor,
                                        kThickness));
  EXPECT_FALSE(
      full_source.GetDetails(gfx::Rect(0, 0, 1, 1), gfx::RoundedCornersF())
          .has_value());
}

// When only the highlight border is active (either because shadow is null or
// its elevation is zero), the combined source produces the same margins,
// aperture insets, and occlusion rect as a standalone HighlightBorder.
TEST_F(ShadowWithHighlightBorderTest, OnlyHighlightBorderActive) {
  const gfx::Rect content_bounds(0, 0, 300, 300);
  const gfx::RoundedCornersF corners(10);

  HighlightBorder standalone_border(kHighlightColor, kBorderColor, kThickness);
  const std::optional<DecorationSource::Details> border_details =
      standalone_border.GetDetails(content_bounds, corners);
  ASSERT_TRUE(border_details.has_value());

  ShadowWithHighlightBorder without_shadow(
      nullptr, std::make_unique<HighlightBorder>(kHighlightColor, kBorderColor,
                                                 kThickness));
  const std::optional<DecorationSource::Details> without_shadow_details =
      without_shadow.GetDetails(content_bounds, corners);
  ASSERT_TRUE(without_shadow_details.has_value());
  EXPECT_EQ(border_details->appearance.margins,
            without_shadow_details->appearance.margins);
  EXPECT_EQ(border_details->appearance.aperture_insets,
            without_shadow_details->appearance.aperture_insets);
  EXPECT_EQ(border_details->occlusion_rect,
            without_shadow_details->occlusion_rect);

  ShadowWithHighlightBorder zero_elevation(
      std::make_unique<Shadow>(/*elevation=*/0),
      std::make_unique<HighlightBorder>(kHighlightColor, kBorderColor,
                                        kThickness));
  const std::optional<DecorationSource::Details> zero_elevation_details =
      zero_elevation.GetDetails(content_bounds, corners);
  ASSERT_TRUE(zero_elevation_details.has_value());
  EXPECT_EQ(border_details->appearance.margins,
            zero_elevation_details->appearance.margins);
  EXPECT_EQ(border_details->appearance.aperture_insets,
            zero_elevation_details->appearance.aperture_insets);
  EXPECT_EQ(border_details->occlusion_rect,
            zero_elevation_details->occlusion_rect);
}

// When only the shadow is active, the combined source produces the same
// margins, aperture insets, and occlusion rect as a standalone Shadow.
TEST_F(ShadowWithHighlightBorderTest, OnlyShadowActive) {
  const gfx::Rect content_bounds(0, 0, 300, 300);
  const gfx::RoundedCornersF corners(10);

  Shadow standalone_shadow(kElevation);
  ShadowWithHighlightBorder combined(std::make_unique<Shadow>(kElevation),
                                     nullptr);

  const std::optional<DecorationSource::Details> shadow_details =
      standalone_shadow.GetDetails(content_bounds, corners);
  const std::optional<DecorationSource::Details> combined_details =
      combined.GetDetails(content_bounds, corners);
  ASSERT_TRUE(shadow_details.has_value());
  ASSERT_TRUE(combined_details.has_value());

  EXPECT_EQ(shadow_details->appearance.margins,
            combined_details->appearance.margins);
  EXPECT_EQ(shadow_details->appearance.aperture_insets,
            combined_details->appearance.aperture_insets);
  EXPECT_EQ(shadow_details->occlusion_rect, combined_details->occlusion_rect);
}

// When content is smaller than the requested shadow elevation and border
// thickness, both are clamped down to fit the content bounds.
TEST_F(ShadowWithHighlightBorderTest, SmallContentClampsShadowAndBorder) {
  ShadowWithHighlightBorder combined(
      std::make_unique<Shadow>(kElevation),
      std::make_unique<HighlightBorder>(kHighlightColor, kBorderColor,
                                        /*thickness=*/2));
  // 6x6 square content: both max_safe_elevation and max_safe_thickness are
  // 6 / 4 = 1.
  const gfx::Rect small_bounds(0, 0, 6, 6);
  const std::optional<DecorationSource::Details> details =
      combined.GetDetails(small_bounds, gfx::RoundedCornersF());
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(ShadowGenerator::GetMargins(Shadow::MakeShadowValues(1)),
            details->appearance.margins);
  EXPECT_EQ(ShadowGenerator::GetNineboxApertureInsets(
                Shadow::MakeShadowValues(1), gfx::RoundedCornersF()),
            details->appearance.aperture_insets);
  EXPECT_EQ(gfx::Rect(2, 2, 2, 2), details->occlusion_rect);
}

// When both shadow and highlight border are active, the decoration layer is
// outset by the combined margins and safe-castable as
// ShadowWithHighlightBorder.
TEST_F(ShadowWithHighlightBorderTest, CombinedGeometryAndLayerBounds) {
  gfx::ScopedAnimationDurationScaleMode zero_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  auto decoration = Decoration::CreateShadowWithHighlightBorder(
      std::make_unique<Shadow>(kElevation),
      std::make_unique<HighlightBorder>(kHighlightColor, kBorderColor,
                                        kThickness));
  EXPECT_EQ("Decoration-ShadowWithHighlightBorder", decoration->name());

  auto* source = decoration->GetSourceAs<ShadowWithHighlightBorder>();
  ASSERT_TRUE(source);
  ASSERT_TRUE(source->shadow());
  ASSERT_TRUE(source->highlight_border());

  const gfx::Rect content_bounds(100, 100, 300, 300);
  const gfx::RoundedCornersF corners(10);
  decoration->SetRoundedCorners(corners);
  decoration->SetContentBounds(content_bounds);

  const std::optional<DecorationSource::Details> details =
      source->GetDetails(content_bounds, corners);
  ASSERT_TRUE(details.has_value());

  gfx::Rect expected_layer_bounds = content_bounds;
  expected_layer_bounds.Outset(details->appearance.margins);
  EXPECT_EQ(expected_layer_bounds, decoration->layer()->bounds());

  // Occlusion rect is inset by two ring thicknesses (2) plus the corner
  // radius (10).
  EXPECT_EQ(gfx::Rect(12, 12, 276, 276), details->occlusion_rect);
}

// Pill-shaped content renders both shadow and highlight border without
// exceeding the nine-patch layer bounds or violating its occlusion invariant.
TEST_F(ShadowWithHighlightBorderTest, PillShapedGeometry) {
  gfx::ScopedAnimationDurationScaleMode zero_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  auto decoration = Decoration::CreateShadowWithHighlightBorder(
      std::make_unique<Shadow>(/*elevation=*/12),
      std::make_unique<HighlightBorder>(kHighlightColor, kBorderColor,
                                        kThickness));

  auto get_border_rect = [](const ui::LayerNinePatch* nine_patch) {
    const gfx::Rect& border = nine_patch->border();
    const gfx::Rect& bounds = nine_patch->bounds();
    return gfx::Rect(border.x(), border.y(), bounds.width() - border.width(),
                     bounds.height() - border.height());
  };

  // Even-height full pill (height == 2 * corner_radius): rounded corners
  // consume the entire height, leaving an empty occlusion rect.
  const gfx::Rect content_bounds(10, 20, 180, 36);
  const gfx::RoundedCornersF corners(18);
  decoration->SetRoundedCorners(corners);
  decoration->SetContentBounds(content_bounds);

  auto* source = decoration->GetSourceAs<ShadowWithHighlightBorder>();
  ASSERT_TRUE(source);
  const std::optional<ShadowWithHighlightBorderValue> spec =
      source->GetSpec(content_bounds, corners);
  ASSERT_TRUE(spec.has_value());
  ASSERT_TRUE(spec->Get<gfx::ShadowValues>().has_value());
  ASSERT_TRUE(spec->Get<HighlightBorderValue>().has_value());
  EXPECT_TRUE(spec->Get<gfx::ShadowValues>()->front().is_pill_shaped());
  EXPECT_TRUE(spec->Get<HighlightBorderValue>()->is_pill_shaped());

  ui::LayerNinePatch* nine_patch = decoration->decoration_layer_for_testing();
  EXPECT_TRUE(nine_patch->visible());
  EXPECT_LE(nine_patch->border().width(), nine_patch->bounds().width());
  EXPECT_LE(nine_patch->border().height(), nine_patch->bounds().height());
  EXPECT_TRUE(nine_patch->occlusion().IsEmpty());

  // Odd-height pill and partial pill leave a non-empty center that must be
  // covered by `nine_patch->occlusion()` (matching cc::NinePatchGenerator's
  // `border_rect` check).
  decoration->SetRoundedCorners(gfx::RoundedCornersF(18.5f));
  decoration->SetContentBounds(gfx::Rect(10, 20, 180, 37));
  EXPECT_LE(nine_patch->border().width(), nine_patch->bounds().width());
  EXPECT_LE(nine_patch->border().height(), nine_patch->bounds().height());
  EXPECT_FALSE(nine_patch->occlusion().IsEmpty());
  EXPECT_TRUE(nine_patch->occlusion().Contains(get_border_rect(nine_patch)));

  decoration->SetRoundedCorners(gfx::RoundedCornersF(25, 25, 0, 0));
  decoration->SetContentBounds(gfx::Rect(10, 20, 200, 50));
  EXPECT_LE(nine_patch->border().width(), nine_patch->bounds().width());
  EXPECT_LE(nine_patch->border().height(), nine_patch->bounds().height());
  EXPECT_FALSE(nine_patch->occlusion().IsEmpty());
  EXPECT_TRUE(nine_patch->occlusion().Contains(get_border_rect(nine_patch)));
}

// Setting or clearing the optional Shadow and HighlightBorder dynamically
// updates the decoration layer accordingly.
TEST_F(ShadowWithHighlightBorderTest, SetAndClearPartsDynamically) {
  gfx::ScopedAnimationDurationScaleMode zero_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  auto decoration = Decoration::CreateShadowWithHighlightBorder();
  auto* source = decoration->GetSourceAs<ShadowWithHighlightBorder>();
  ASSERT_TRUE(source);

  const gfx::Rect content_bounds(50, 50, 200, 200);
  decoration->SetRoundedCorners(gfx::RoundedCornersF(8));
  decoration->SetContentBounds(content_bounds);

  // Initially neither effect is set, so the decoration layer is hidden.
  EXPECT_FALSE(decoration->decoration_layer_for_testing()->visible());
  EXPECT_EQ(content_bounds, decoration->layer()->bounds());

  // Attach a highlight border; its rings sit inside the content bounds.
  source->SetHighlightBorder(std::make_unique<HighlightBorder>(
      kHighlightColor, kBorderColor, kThickness));
  EXPECT_TRUE(decoration->decoration_layer_for_testing()->visible());
  EXPECT_EQ(content_bounds, decoration->layer()->bounds());

  // Attach a shadow; layer bounds expand to fit the shadow.
  source->SetShadow(std::make_unique<Shadow>(kElevation));
  gfx::Rect expected_shadow_bounds = content_bounds;
  expected_shadow_bounds.Outset(
      ShadowGenerator::GetMargins(Shadow::MakeShadowValues(kElevation)));
  EXPECT_EQ(expected_shadow_bounds, decoration->layer()->bounds());

  // Clear the shadow; layer bounds shrink back to the content bounds.
  source->SetShadow(nullptr);
  EXPECT_FALSE(source->shadow());
  EXPECT_TRUE(decoration->decoration_layer_for_testing()->visible());
  EXPECT_EQ(content_bounds, decoration->layer()->bounds());

  // Clear the highlight border; layer is hidden again.
  source->SetHighlightBorder(nullptr);
  EXPECT_FALSE(source->highlight_border());
  EXPECT_FALSE(decoration->decoration_layer_for_testing()->visible());
  EXPECT_EQ(content_bounds, decoration->layer()->bounds());
}

// Changing shadow elevation cross-fades the combined decoration layer, while
// changing highlight border properties updates immediately.
TEST_F(ShadowWithHighlightBorderTest, ElevationChangeTriggersCrossFade) {
  auto decoration = Decoration::CreateShadowWithHighlightBorder(
      std::make_unique<Shadow>(kElevation),
      std::make_unique<HighlightBorder>(kHighlightColor, kBorderColor,
                                        kThickness));
  decoration->SetContentBounds(gfx::Rect(0, 0, 300, 300));
  EXPECT_FALSE(decoration->fading_layer_for_testing());

  auto* source = decoration->GetSourceAs<ShadowWithHighlightBorder>();
  ASSERT_TRUE(source);

  // Highlight border color and thickness changes do not create a fading layer.
  source->highlight_border()->SetColors(SK_ColorWHITE, SK_ColorBLACK);
  EXPECT_FALSE(decoration->fading_layer_for_testing());
  source->highlight_border()->SetThickness(2);
  EXPECT_FALSE(decoration->fading_layer_for_testing());

  // Shadow elevation changes trigger cross-fade animation.
  source->shadow()->SetElevation(kElevation * 2);
  EXPECT_TRUE(decoration->fading_layer_for_testing());

  decoration->OnImplicitAnimationsCompleted();
  EXPECT_FALSE(decoration->fading_layer_for_testing());
}

// Identical shadow + highlight border appearances share the same cached
// ninebox image, while changing any parameter produces a distinct image.
TEST_F(ShadowWithHighlightBorderTest, NineboxImageIsCachedPerAppearance) {
  const gfx::Rect content_bounds(0, 0, 300, 300);
  const gfx::RoundedCornersF corners(10);

  auto image_for = [&content_bounds](int elevation, SkColor highlight_color,
                                     SkColor border_color, int thickness,
                                     const gfx::RoundedCornersF& radii) {
    ShadowWithHighlightBorder source(
        std::make_unique<Shadow>(elevation),
        std::make_unique<HighlightBorder>(highlight_color, border_color,
                                          thickness));
    const std::optional<DecorationSource::Details> details =
        source.GetDetails(content_bounds, radii);
    CHECK(details.has_value());
    return details->appearance.nine_patch_image;
  };

  const gfx::ImageSkia image =
      image_for(kElevation, kHighlightColor, kBorderColor, kThickness, corners);

  EXPECT_TRUE(image.BackedBySameObjectAs(image_for(
      kElevation, kHighlightColor, kBorderColor, kThickness, corners)));

  EXPECT_FALSE(image.BackedBySameObjectAs(image_for(
      kElevation + 4, kHighlightColor, kBorderColor, kThickness, corners)));
  EXPECT_FALSE(image.BackedBySameObjectAs(
      image_for(kElevation, SK_ColorRED, kBorderColor, kThickness, corners)));
  EXPECT_FALSE(image.BackedBySameObjectAs(image_for(
      kElevation, kHighlightColor, SK_ColorRED, kThickness, corners)));
  EXPECT_FALSE(image.BackedBySameObjectAs(image_for(
      kElevation, kHighlightColor, kBorderColor, kThickness + 1, corners)));
  EXPECT_FALSE(image.BackedBySameObjectAs(image_for(kElevation, kHighlightColor,
                                                    kBorderColor, kThickness,
                                                    gfx::RoundedCornersF(20))));

  // When elevation is zero, unused shadow color maps are normalized out of the
  // cache key so identical borders share a single cached image.
  ShadowWithHighlightBorder border_only_default(
      std::make_unique<Shadow>(/*elevation=*/0),
      std::make_unique<HighlightBorder>(kHighlightColor, kBorderColor,
                                        kThickness));
  ShadowWithHighlightBorder border_only_custom_colors(
      std::make_unique<Shadow>(
          /*elevation=*/0, Shadow::Style::kMaterialDesign,
          Shadow::ElevationToColorsMap{{0, {SK_ColorRED, SK_ColorBLUE}}}),
      std::make_unique<HighlightBorder>(kHighlightColor, kBorderColor,
                                        kThickness));
  const gfx::ImageSkia border_only_image =
      border_only_default.GetDetails(content_bounds, corners)
          ->appearance.nine_patch_image;
  EXPECT_TRUE(border_only_image.BackedBySameObjectAs(
      border_only_custom_colors.GetDetails(content_bounds, corners)
          ->appearance.nine_patch_image));
}

}  // namespace
}  // namespace ui::decoration
