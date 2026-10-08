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
  EXPECT_EQ(gfx::Insets(-1), details->appearance.margins);
  EXPECT_EQ(gfx::Insets(10), details->appearance.aperture_insets);
  EXPECT_EQ(gfx::Size(21, 21), details->appearance.nine_patch_image.size());
  EXPECT_TRUE(details->appearance.nine_patch_image.BackedBySameObjectAs(
      HighlightBorderDetails::Get(radii, kValue).nine_patch_image));
  // Everything inside the inner ring and the rounded corners is covered, in
  // content coordinates.
  EXPECT_EQ(gfx::Rect(9, 9, 182, 182), details->occlusion_rect);
}

// The border is not drawn when its corner patches cannot fit in the content.
TEST(HighlightBorderTest, NoDetailsWhenContentTooSmall) {
  HighlightBorder border(kHighlight, kBorder);

  const gfx::RoundedCornersF square;
  EXPECT_TRUE(border.GetDetails(gfx::Rect(2, 2), square).has_value());
  EXPECT_FALSE(border.GetDetails(gfx::Rect(1, 2), square).has_value());
  EXPECT_FALSE(border.GetDetails(gfx::Rect(2, 1), square).has_value());

  const gfx::RoundedCornersF rounded(4);
  EXPECT_TRUE(border.GetDetails(gfx::Rect(10, 10), rounded).has_value());
  EXPECT_FALSE(border.GetDetails(gfx::Rect(9, 10), rounded).has_value());
  EXPECT_FALSE(border.GetDetails(gfx::Rect(10, 9), rounded).has_value());
}

// Rings thicker than the content can hold are thinned rather than dropped, the
// way Shadow lowers its elevation for small content.
TEST(HighlightBorderTest, ThicknessAdjustedToFitContent) {
  HighlightBorder border(kHighlight, kBorder, /*thickness=*/3);
  const gfx::RoundedCornersF radii(2);

  // Each side needs two thicknesses plus the corner radius while the layer is
  // only one thickness larger than the content, so 10 DIP hold the full rings.
  std::optional<DecorationSource::Details> details =
      border.GetDetails(gfx::Rect(10, 20), radii);
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(gfx::Insets(-3), details->appearance.margins);
  EXPECT_EQ(gfx::Insets(8), details->appearance.aperture_insets);

  // 9 DIP only hold two-pixel rings; the image and occlusion follow suit.
  details = border.GetDetails(gfx::Rect(9, 20), radii);
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(gfx::Insets(-2), details->appearance.margins);
  EXPECT_EQ(gfx::Insets(6), details->appearance.aperture_insets);
  EXPECT_TRUE(details->appearance.nine_patch_image.BackedBySameObjectAs(
      HighlightBorderDetails::Get(
          radii, HighlightBorderValue(kHighlight, kBorder, /*thickness=*/2))
          .nine_patch_image));
  EXPECT_EQ(gfx::Rect(4, 4, 1, 12), details->occlusion_rect);

  // 6 DIP are down to hairlines, and 5 DIP cannot even hold those.
  details = border.GetDetails(gfx::Rect(6, 20), radii);
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(gfx::Insets(-1), details->appearance.margins);
  EXPECT_FALSE(border.GetDetails(gfx::Rect(5, 20), radii).has_value());

  // The requested thickness is kept for when the content grows again.
  EXPECT_EQ(3, border.thickness());
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

  // The decoration extends one ring thickness beyond the content.
  EXPECT_EQ(gfx::Rect(49, 49, 202, 202), decoration().layer()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 202, 202), decoration_layer()->bounds());
  // The 21x21 image stretches its center pixel; the patches around it span two
  // ring thicknesses plus the corner radius.
  EXPECT_EQ(gfx::Rect(10, 10, 1, 1), decoration_layer()->aperture());
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20), decoration_layer()->border());
  // The occlusion is in the decoration layer's coordinates and matches the
  // stretched center exactly.
  EXPECT_EQ(gfx::Rect(10, 10, 182, 182), decoration_layer()->occlusion());
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

  // Two-pixel rings need twice the margin and twice the straight run before
  // the corner arcs.
  EXPECT_EQ(gfx::Rect(48, 48, 204, 204), decoration().layer()->bounds());
  EXPECT_EQ(gfx::Rect(12, 12, 1, 1), decoration_layer()->aperture());
  EXPECT_EQ(gfx::Rect(12, 12, 24, 24), decoration_layer()->border());
  EXPECT_EQ(gfx::Rect(12, 12, 180, 180), decoration_layer()->occlusion());
}

}  // namespace
}  // namespace ui::decoration
