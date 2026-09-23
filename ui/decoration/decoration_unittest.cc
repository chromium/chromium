// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/decoration/decoration.h"

#include <memory>
#include <optional>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_nine_patch.h"
#include "ui/decoration/decoration_source.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/image/image_skia.h"

namespace ui::decoration {
namespace {

DecorationSource::Details MakeDetails(int margin,
                                      const gfx::Rect& occlusion_rect = {}) {
  return DecorationSource::Details{
      .appearance =
          {
              .nine_patch_image = gfx::ImageSkia(),
              .aperture_insets = gfx::Insets(margin),
              .margins = gfx::Insets(-margin),
          },
      .occlusion_rect = occlusion_rect,
  };
}

// The content geometry formed by `rect` and `corners`.
gfx::RRectF ContentBounds(
    const gfx::Rect& rect,
    const gfx::RoundedCornersF& corners = gfx::RoundedCornersF()) {
  return gfx::RRectF(gfx::RectF(rect), corners);
}

class TestDecorationSource : public DecorationSource {
 public:
  DECLARE_SAFE_CAST_TARGET()

  explicit TestDecorationSource(
      std::optional<Details> details = MakeDetails(10))
      : details_(std::move(details)) {}
  ~TestDecorationSource() override = default;

  std::optional<Details> GetDetails(
      const gfx::RRectF& content_bounds) override {
    last_content_bounds_ = content_bounds;
    return details_;
  }

  void set_details(std::optional<Details> details) {
    details_ = std::move(details);
  }
  const gfx::RRectF& last_content_bounds() const {
    return last_content_bounds_;
  }

  void TriggerChanged(
      std::optional<base::TimeDelta> cross_fade_duration = std::nullopt) {
    NotifyDecorationChanged(cross_fade_duration);
  }

 private:
  std::optional<Details> details_;
  gfx::RRectF last_content_bounds_;
};

DEFINE_SAFE_CAST_TARGET(TestDecorationSource)

class DecorationTest : public testing::Test {
 public:
  using Details = DecorationSource::Details;

  DecorationTest()
      : decoration_(
            Decoration::Create(std::make_unique<TestDecorationSource>())) {}
  ~DecorationTest() override = default;

 protected:
  Decoration& decoration() { return *decoration_; }
  TestDecorationSource& source() {
    return *decoration_->source()->AsA<TestDecorationSource>();
  }
  const TestDecorationSource& source() const {
    return *decoration_->source()->AsA<TestDecorationSource>();
  }

  void set_details(std::optional<Details> details) {
    source().set_details(std::move(details));
  }
  const gfx::RRectF& last_content_bounds() const {
    return source().last_content_bounds();
  }

 private:
  std::unique_ptr<Decoration> decoration_;
};

TEST_F(DecorationTest, BasicInitialization) {
  EXPECT_TRUE(decoration().layer());
  EXPECT_TRUE(decoration().decoration_layer_for_testing());
  EXPECT_FALSE(decoration().fading_layer_for_testing());
  EXPECT_EQ(gfx::RRectF(), decoration().content_bounds());
}

TEST_F(DecorationTest, SetContentBoundsAndAppearance) {
  const gfx::Rect content_bounds(50, 50, 200, 200);
  decoration().SetContentBounds(ContentBounds(content_bounds));

  EXPECT_EQ(ContentBounds(content_bounds), decoration().content_bounds());
  EXPECT_EQ(ContentBounds(content_bounds), last_content_bounds());

  // Layer bounds are outset by margins (-10 -> +10 outwards).
  gfx::Rect expected_layer_bounds = content_bounds;
  expected_layer_bounds.Inset(gfx::Insets(-10));
  EXPECT_EQ(expected_layer_bounds, decoration().layer()->bounds());
}

// Test that layer bounds are empty when content bounds are empty, and update
// properly when transitioning between empty and non-empty bounds.
TEST_F(DecorationTest, EmptyContentBounds) {
  // Initially, content bounds are empty and layer bounds should be empty.
  EXPECT_TRUE(decoration().content_bounds().IsEmpty());
  EXPECT_TRUE(decoration().layer()->bounds().IsEmpty());
  EXPECT_TRUE(decoration().decoration_layer_for_testing()->bounds().IsEmpty());

  // Set non-empty content bounds.
  const gfx::Rect content_bounds(100, 100, 300, 300);
  decoration().SetContentBounds(ContentBounds(content_bounds));
  gfx::Rect expected_layer_bounds = content_bounds;
  expected_layer_bounds.Inset(gfx::Insets(-10));
  EXPECT_EQ(expected_layer_bounds, decoration().layer()->bounds());
  EXPECT_EQ(expected_layer_bounds.size(),
            decoration().decoration_layer_for_testing()->bounds().size());

  // Reset to empty content bounds. Layer bounds should collapse to empty.
  decoration().SetContentBounds(ContentBounds(gfx::Rect()));
  EXPECT_TRUE(decoration().content_bounds().IsEmpty());
  EXPECT_TRUE(decoration().layer()->bounds().IsEmpty());
  EXPECT_TRUE(decoration().decoration_layer_for_testing()->bounds().IsEmpty());

  // Restore non-empty content bounds.
  decoration().SetContentBounds(ContentBounds(content_bounds));
  EXPECT_EQ(expected_layer_bounds, decoration().layer()->bounds());
  EXPECT_EQ(expected_layer_bounds.size(),
            decoration().decoration_layer_for_testing()->bounds().size());
}

// Test if the decoration's layer bounds are modified, setting the same content
// bounds can reset the layer bounds.
TEST_F(DecorationTest, ResetLayerBoundsBySettingSameContentBounds) {
  const gfx::Rect content_bounds(100, 100, 300, 300);
  decoration().SetContentBounds(ContentBounds(content_bounds));
  EXPECT_EQ(ContentBounds(content_bounds), decoration().content_bounds());

  const gfx::Rect layer_bounds = decoration().layer()->bounds();

  // Change decoration's layer bounds.
  const gfx::Rect modified_bounds(200, 200, 150, 400);
  decoration().layer()->SetBounds(modified_bounds);
  EXPECT_EQ(decoration().layer()->bounds(), modified_bounds);

  // Reset layer bounds by setting the same content bounds.
  decoration().SetContentBounds(ContentBounds(content_bounds));
  EXPECT_EQ(layer_bounds, decoration().layer()->bounds());
}

TEST_F(DecorationTest, RecreateLayer) {
  decoration().SetContentBounds(ContentBounds(gfx::Rect(0, 0, 100, 100)));

  ui::Layer* original_root_layer = decoration().layer();
  ASSERT_TRUE(original_root_layer);

  // Recreate the root decoration layer.
  std::unique_ptr<ui::Layer> old_layer = decoration().RecreateLayer();
  EXPECT_NE(original_root_layer, decoration().layer());
  EXPECT_EQ(gfx::Rect(-10, -10, 120, 120), decoration().layer()->bounds());
}

TEST_F(DecorationTest, RoundedContentBounds) {
  const gfx::RRectF content_bounds = ContentBounds(
      gfx::Rect(0, 0, 100, 100), gfx::RoundedCornersF(5, 10, 15, 20));
  decoration().SetContentBounds(content_bounds);

  EXPECT_EQ(content_bounds, decoration().content_bounds());
  EXPECT_EQ(content_bounds, last_content_bounds());
}

TEST_F(DecorationTest, OcclusionRectTranslatedToLayerSpace) {
  // Content-relative occlusion: the content inset by 5 on every side.
  set_details(MakeDetails(10, gfx::Rect(5, 5, 90, 90)));
  decoration().SetContentBounds(ContentBounds(gfx::Rect(50, 50, 100, 100)));

  // Margins are -10, so the content sits at (10, 10) within the layer.
  EXPECT_EQ(gfx::Rect(15, 15, 90, 90),
            decoration().decoration_layer_for_testing()->occlusion());
}

// The nine-patch image does not need re-uploading when only occlusion moves.
TEST_F(DecorationTest, OcclusionChangesWithoutAppearanceChange) {
  set_details(MakeDetails(10, gfx::Rect(5, 5, 90, 90)));
  decoration().SetContentBounds(ContentBounds(gfx::Rect(0, 0, 100, 100)));

  const gfx::Rect aperture =
      decoration().decoration_layer_for_testing()->aperture();

  // Same appearance, different occlusion rect.
  set_details(MakeDetails(10, gfx::Rect(20, 20, 60, 60)));
  source().TriggerChanged();

  EXPECT_EQ(gfx::Rect(30, 30, 60, 60),
            decoration().decoration_layer_for_testing()->occlusion());
  EXPECT_EQ(aperture, decoration().decoration_layer_for_testing()->aperture());
}

TEST(AppearanceTest, EqualityIgnoresOcclusionRect) {
  const DecorationSource::Details details =
      MakeDetails(10, gfx::Rect(5, 5, 90, 90));
  const DecorationSource::Details same_appearance_other_occlusion =
      MakeDetails(10, gfx::Rect(20, 20, 60, 60));

  EXPECT_EQ(details.appearance, same_appearance_other_occlusion.appearance);
  EXPECT_NE(details, same_appearance_other_occlusion);

  EXPECT_NE(details.appearance,
            MakeDetails(20, gfx::Rect(5, 5, 90, 90)).appearance);
}

TEST_F(DecorationTest, SizeAdjustedRoundedCornersClamping) {
  // Content dimension is 40x40, max radius is 20.
  const gfx::Rect content_bounds(0, 0, 40, 40);
  decoration().SetContentBounds(
      ContentBounds(content_bounds, gfx::RoundedCornersF(30, 10, 25, 5)));

  // Radii too large for the content are clamped to fit it.
  const gfx::RRectF clamped_bounds =
      ContentBounds(content_bounds, gfx::RoundedCornersF(20, 10, 20, 5));
  EXPECT_EQ(clamped_bounds, decoration().content_bounds());
  EXPECT_EQ(clamped_bounds, last_content_bounds());
}

TEST_F(DecorationTest, CrossFade) {
  decoration().SetContentBounds(ContentBounds(gfx::Rect(0, 0, 100, 100)));

  EXPECT_FALSE(decoration().fading_layer_for_testing());
  source().TriggerChanged(base::Milliseconds(100));

  EXPECT_TRUE(decoration().decoration_layer_for_testing());
  EXPECT_TRUE(decoration().fading_layer_for_testing());

  // Completing animations cleans up the fading layer.
  decoration().OnImplicitAnimationsCompleted();
  EXPECT_FALSE(decoration().fading_layer_for_testing());
}

TEST_F(DecorationTest, NulloptDetailsResetsDecorationLayer) {
  const gfx::Rect content_bounds(0, 0, 100, 100);
  decoration().SetContentBounds(ContentBounds(content_bounds));

  // Layer bounds initially outset by margins (-10 -> +10 outwards).
  gfx::Rect expected_layer_bounds = content_bounds;
  expected_layer_bounds.Inset(gfx::Insets(-10));
  EXPECT_EQ(expected_layer_bounds, decoration().layer()->bounds());

  // Source returns nullopt -> decoration layer image and bounds are reset.
  set_details(std::nullopt);
  source().TriggerChanged();
  EXPECT_EQ(content_bounds, decoration().layer()->bounds());
  EXPECT_EQ(gfx::Rect(content_bounds.size()),
            decoration().decoration_layer_for_testing()->bounds());

  // Source returns details again -> decoration layer is reconfigured.
  set_details(MakeDetails(10));
  source().TriggerChanged();
  EXPECT_EQ(expected_layer_bounds, decoration().layer()->bounds());
}

TEST_F(DecorationTest, NulloptDetailsWithFadingLayer) {
  decoration().SetContentBounds(ContentBounds(gfx::Rect(0, 0, 100, 100)));

  source().TriggerChanged(base::Milliseconds(100));
  ASSERT_TRUE(decoration().fading_layer_for_testing());

  // Source returns nullopt while cross-fading.
  set_details(std::nullopt);
  source().TriggerChanged();

  // Layer bounds should encompass the fading layer bounds.
  EXPECT_FALSE(decoration().layer()->bounds().IsEmpty());
  EXPECT_TRUE(decoration().fading_layer_for_testing());

  decoration().OnImplicitAnimationsCompleted();
  EXPECT_FALSE(decoration().fading_layer_for_testing());
}

TEST_F(DecorationTest, NotifyDecorationChanged) {
  const gfx::Rect content_bounds(0, 0, 100, 100);
  decoration().SetContentBounds(ContentBounds(content_bounds));

  set_details(MakeDetails(20));
  source().TriggerChanged();

  gfx::Rect expected_layer_bounds = content_bounds;
  expected_layer_bounds.Inset(gfx::Insets(-20));
  EXPECT_EQ(expected_layer_bounds, decoration().layer()->bounds());
}

}  // namespace
}  // namespace ui::decoration
