// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/decoration/highlight_border.h"

#include <memory>
#include <optional>

#include "base/memory/discardable_memory_allocator.h"
#include "base/test/bind.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_nine_patch.h"
#include "ui/decoration/decoration.h"
#include "ui/decoration/decoration_source.h"
#include "ui/decoration/decoration_util.h"
#include "ui/decoration/highlight_border_value.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace ui::decoration {
namespace {

constexpr SkColor kHighlight = SK_ColorWHITE;
constexpr SkColor kBorder = SK_ColorBLACK;
constexpr HighlightBorderValue kValue(kHighlight, kBorder);

TEST(HighlightBorderTest, GetDetails) {
  HighlightBorder border(kHighlight, kBorder);
  const gfx::Rect content(50, 50, 200, 200);
  const gfx::RoundedCornersF radii(8);

  const std::optional<DecorationSource::Details> details =
      border.GetDetails(content, radii);
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(gfx::Outsets(), details->appearance.margins);
  EXPECT_EQ(gfx::Insets(10), details->appearance.aperture_insets);
  EXPECT_EQ(gfx::Size(21, 21), details->appearance.nine_patch_image.size());
  EXPECT_TRUE(details->appearance.nine_patch_image.BackedBySameObjectAs(
      HighlightBorderDetails::Get(radii, kValue).nine_patch_image));
  // Everything inside both rings and the rounded corners is covered, in content
  // coordinates.
  EXPECT_EQ(gfx::Rect(10, 10, 180, 180), details->occlusion_rect);
}

// The border is not drawn when its corner patches cannot fit in the content.
TEST(HighlightBorderTest, NoDetailsWhenContentTooSmall) {
  HighlightBorder border(kHighlight, kBorder);

  const gfx::RoundedCornersF square;
  EXPECT_TRUE(border.GetDetails(gfx::Rect(4, 4), square).has_value());
  EXPECT_FALSE(border.GetDetails(gfx::Rect(3, 4), square).has_value());
  EXPECT_FALSE(border.GetDetails(gfx::Rect(4, 3), square).has_value());

  const gfx::RoundedCornersF rounded(4);
  EXPECT_TRUE(border.GetDetails(gfx::Rect(12, 12), rounded).has_value());
  EXPECT_FALSE(border.GetDetails(gfx::Rect(11, 12), rounded).has_value());
  EXPECT_FALSE(border.GetDetails(gfx::Rect(12, 11), rounded).has_value());
}

// Rings thicker than the content can hold are thinned rather than dropped, the
// way Shadow lowers its elevation for small content.
TEST(HighlightBorderTest, ThicknessAdjustedToFitContent) {
  HighlightBorder border(kHighlight, kBorder, /*thickness=*/3);
  const gfx::RoundedCornersF radii(2);

  // 16 DIP hold the full three-pixel rings.
  std::optional<DecorationSource::Details> details =
      border.GetDetails(gfx::Rect(16, 20), radii);
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(gfx::Outsets(), details->appearance.margins);
  EXPECT_EQ(gfx::Insets(8), details->appearance.aperture_insets);

  // 15 DIP only hold two-pixel rings; the image and aperture follow suit.
  details = border.GetDetails(gfx::Rect(15, 20), radii);
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(gfx::Outsets(), details->appearance.margins);
  EXPECT_EQ(gfx::Insets(6), details->appearance.aperture_insets);
  EXPECT_TRUE(details->appearance.nine_patch_image.BackedBySameObjectAs(
      HighlightBorderDetails::Get(
          radii, HighlightBorderValue(kHighlight, kBorder, /*thickness=*/2))
          .nine_patch_image));
  EXPECT_EQ(gfx::Rect(6, 6, 3, 8), details->occlusion_rect);

  // 8 DIP are down to hairlines, and 7 DIP cannot even hold those.
  details = border.GetDetails(gfx::Rect(8, 20), radii);
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(gfx::Outsets(), details->appearance.margins);
  EXPECT_EQ(gfx::Insets(4), details->appearance.aperture_insets);
  EXPECT_FALSE(border.GetDetails(gfx::Rect(7, 20), radii).has_value());

  // The requested thickness is kept for when the content grows again.
  EXPECT_EQ(3, border.thickness());
}

TEST(HighlightBorderTest, PillShapedContent) {
  HighlightBorder border(kHighlight, kBorder);
  const gfx::RoundedCornersF pill_radii(18);

  // Horizontal pill (200x36, radius 18) and circular pill (36x36, radius 18).
  for (const gfx::Rect& bounds : {gfx::Rect(200, 36), gfx::Rect(36, 36)}) {
    const std::optional<DecorationSource::Details> details =
        border.GetDetails(bounds, pill_radii);
    ASSERT_TRUE(details.has_value());
    EXPECT_EQ(gfx::Outsets(), details->appearance.margins);
    EXPECT_EQ(gfx::Insets(18), details->appearance.aperture_insets);
    EXPECT_EQ(gfx::Size(37, 37), details->appearance.nine_patch_image.size());
    EXPECT_TRUE(details->occlusion_rect.IsEmpty());
  }

  // Half-pill (left cap semicircular, right edge square).
  const std::optional<DecorationSource::Details> half_pill =
      border.GetDetails(gfx::Rect(200, 36), gfx::RoundedCornersF(18, 0, 0, 18));
  ASSERT_TRUE(half_pill.has_value());
  EXPECT_EQ(gfx::Insets::TLBR(18, 18, 18, 2),
            half_pill->appearance.aperture_insets);
  EXPECT_TRUE(half_pill->occlusion_rect.IsEmpty());

  // Variable rounded corners where a corner reaches half the smaller dimension
  // also use pill-shaped clamping (including diagonal leaf shapes), matching
  // Shadow::GetSpec.
  const std::optional<DecorationSource::Details> top_pill =
      border.GetDetails(gfx::Rect(200, 50), gfx::RoundedCornersF(25, 25, 0, 0));
  ASSERT_TRUE(top_pill.has_value());
  EXPECT_EQ(gfx::Insets::TLBR(25, 25, 2, 25),
            top_pill->appearance.aperture_insets);
  EXPECT_EQ(gfx::Rect(25, 25, 150, 23), top_pill->occlusion_rect);

  const std::optional<DecorationSource::Details> leaf =
      border.GetDetails(gfx::Rect(50, 50), gfx::RoundedCornersF(25, 0, 25, 0));
  ASSERT_TRUE(leaf.has_value());
  EXPECT_EQ(gfx::Insets(25), leaf->appearance.aperture_insets);
  EXPECT_TRUE(leaf->occlusion_rect.IsEmpty());

  // Radii strictly below half the smaller dimension are not pill-shaped and
  // keep the full two-thickness inset past the corners.
  const std::optional<DecorationSource::Details> non_pill =
      border.GetDetails(gfx::Rect(200, 50), gfx::RoundedCornersF(20, 20, 0, 0));
  ASSERT_TRUE(non_pill.has_value());
  EXPECT_EQ(gfx::Insets::TLBR(22, 22, 2, 22),
            non_pill->appearance.aperture_insets);
  EXPECT_EQ(gfx::Rect(22, 22, 156, 26), non_pill->occlusion_rect);

  // Thick borders on small pills clamp down to fit and drop below 4 DIP.
  HighlightBorder thick_border(kHighlight, kBorder, /*thickness=*/3);
  std::optional<HighlightBorderValue> spec =
      thick_border.GetSpec(gfx::Rect(8, 20), gfx::RoundedCornersF(4));
  ASSERT_TRUE(spec.has_value());
  EXPECT_EQ(2, spec->thickness());
  EXPECT_TRUE(spec->is_pill_shaped());

  spec = thick_border.GetSpec(gfx::Rect(4, 20), gfx::RoundedCornersF(2));
  ASSERT_TRUE(spec.has_value());
  EXPECT_EQ(1, spec->thickness());
  EXPECT_TRUE(spec->is_pill_shaped());

  EXPECT_FALSE(thick_border.GetSpec(gfx::Rect(3, 20), gfx::RoundedCornersF(1))
                   .has_value());
}

TEST(HighlightBorderTest, SettersNotifyOnlyOnChange) {
  HighlightBorder border(kHighlight, kBorder);
  int notifications = 0;
  std::optional<base::TimeDelta> last_duration;
  border.set_details_changed_callback(base::BindLambdaForTesting(
      [&](std::optional<base::TimeDelta> cross_fade_duration) {
        ++notifications;
        last_duration = cross_fade_duration;
      }));

  border.SetColors(kHighlight, kBorder);
  border.SetThickness(1);
  EXPECT_EQ(0, notifications);

  // Changing either color repaints once.
  border.SetColors(SK_ColorBLUE, kBorder);
  EXPECT_EQ(1, notifications);
  EXPECT_EQ(SK_ColorBLUE, border.highlight_color());
  EXPECT_EQ(kBorder, border.border_color());
  // Color changes swap the image in place rather than cross-fading.
  EXPECT_FALSE(last_duration.has_value());

  border.SetColors(SK_ColorBLUE, SK_ColorRED);
  EXPECT_EQ(2, notifications);
  EXPECT_EQ(SK_ColorBLUE, border.highlight_color());
  EXPECT_EQ(SK_ColorRED, border.border_color());

  border.SetThickness(2);
  EXPECT_EQ(3, notifications);
  EXPECT_EQ(2, border.thickness());
  EXPECT_FALSE(last_duration.has_value());
}

class HighlightBorderDecorationTest : public testing::Test {
 public:
  void SetUp() override {
    base::DiscardableMemoryAllocator::SetInstance(
        &discardable_memory_allocator_);
    decoration_ = Decoration::CreateHighlightBorder(kHighlight, kBorder);
  }

  void TearDown() override {
    decoration_.reset();
    base::DiscardableMemoryAllocator::SetInstance(nullptr);
  }

 protected:
  Decoration& decoration() { return *decoration_; }
  LayerNinePatch* decoration_layer() {
    return decoration_->decoration_layer_for_testing();
  }

 private:
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;
  std::unique_ptr<Decoration> decoration_;
};

TEST_F(HighlightBorderDecorationTest, CreateHighlightBorder) {
  HighlightBorder* source = decoration().GetSourceAs<HighlightBorder>();
  ASSERT_TRUE(source);
  EXPECT_EQ(kHighlight, source->highlight_color());
  EXPECT_EQ(kBorder, source->border_color());
  EXPECT_EQ(1, source->thickness());
  EXPECT_EQ("Decoration-HighlightBorder", decoration().name());
}

TEST_F(HighlightBorderDecorationTest, LayerGeometry) {
  decoration().SetRoundedCorners(gfx::RoundedCornersF(8));
  decoration().SetContentBounds(gfx::Rect(50, 50, 200, 200));

  // Both rings sit inside the content, so the decoration layer matches the
  // content bounds.
  EXPECT_EQ(gfx::Rect(50, 50, 200, 200), decoration().layer()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 200, 200), decoration_layer()->bounds());
  // The 21x21 image stretches its center pixel; the patches around it span two
  // ring thicknesses plus the corner radius.
  EXPECT_EQ(gfx::Rect(10, 10, 1, 1), decoration_layer()->aperture());
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20), decoration_layer()->border());
  // The occlusion is in the decoration layer's coordinates and matches the
  // stretched center exactly.
  EXPECT_EQ(gfx::Rect(10, 10, 180, 180), decoration_layer()->occlusion());
}

TEST_F(HighlightBorderDecorationTest, SetColorsSwapsImageOnly) {
  decoration().SetRoundedCorners(gfx::RoundedCornersF(8));
  decoration().SetContentBounds(gfx::Rect(50, 50, 200, 200));
  const gfx::Rect layer_bounds = decoration().layer()->bounds();
  const gfx::Rect aperture = decoration_layer()->aperture();

  decoration().GetSourceAs<HighlightBorder>()->SetColors(SK_ColorBLUE,
                                                         SK_ColorRED);

  // The new colors are picked up without a cross-fade or any change to the
  // geometry.
  EXPECT_FALSE(decoration().fading_layer_for_testing());
  EXPECT_EQ(layer_bounds, decoration().layer()->bounds());
  EXPECT_EQ(aperture, decoration_layer()->aperture());
  const std::optional<DecorationSource::Details> details =
      decoration().source()->GetDetails(decoration().content_bounds(),
                                        decoration().rounded_corners());
  ASSERT_TRUE(details.has_value());
  EXPECT_TRUE(details->appearance.nine_patch_image.BackedBySameObjectAs(
      HighlightBorderDetails::Get(
          gfx::RoundedCornersF(8),
          HighlightBorderValue(SK_ColorBLUE, SK_ColorRED))
          .nine_patch_image));
  EXPECT_FALSE(details->appearance.nine_patch_image.BackedBySameObjectAs(
      HighlightBorderDetails::Get(gfx::RoundedCornersF(8), kValue)
          .nine_patch_image));
}

TEST_F(HighlightBorderDecorationTest, SetThicknessUpdatesGeometry) {
  decoration().SetRoundedCorners(gfx::RoundedCornersF(8));
  decoration().SetContentBounds(gfx::Rect(50, 50, 200, 200));

  decoration().GetSourceAs<HighlightBorder>()->SetThickness(2);

  // Two-pixel rings need twice the straight run before the corner arcs.
  EXPECT_EQ(gfx::Rect(50, 50, 200, 200), decoration().layer()->bounds());
  EXPECT_EQ(gfx::Rect(12, 12, 1, 1), decoration_layer()->aperture());
  EXPECT_EQ(gfx::Rect(12, 12, 24, 24), decoration_layer()->border());
  EXPECT_EQ(gfx::Rect(12, 12, 176, 176), decoration_layer()->occlusion());
}

TEST_F(HighlightBorderDecorationTest, PillShapedLayerGeometry) {
  // Odd height with half-height float radius exercises ClampRadiiToBounds
  // together with pill-shaped ninebox geometry.
  decoration().SetRoundedCorners(gfx::RoundedCornersF(18.5f));
  decoration().SetContentBounds(gfx::Rect(10, 20, 200, 37));

  EXPECT_TRUE(decoration_layer()->visible());
  EXPECT_EQ(gfx::Rect(10, 20, 200, 37), decoration().layer()->bounds());
  EXPECT_EQ(gfx::Rect(18, 18, 1, 1), decoration_layer()->aperture());
  EXPECT_EQ(gfx::Rect(18, 18, 36, 36), decoration_layer()->border());
  EXPECT_EQ(gfx::Rect(18, 18, 164, 1), decoration_layer()->occlusion());
}

}  // namespace
}  // namespace ui::decoration
