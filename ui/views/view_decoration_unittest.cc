// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_decoration.h"

#include <memory>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_nine_patch.h"
#include "ui/decoration/highlight_border.h"
#include "ui/decoration/shadow.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace views {

using ViewDecorationTest = ViewsTestBase;

TEST_F(ViewDecorationTest, UseShadow) {
  View root;
  root.SetPaintToLayer();

  View* v1 = root.AddChildView(std::make_unique<View>());
  v1->SetPaintToLayer();
  View* v2 = root.AddChildView(std::make_unique<View>());
  v2->SetPaintToLayer();
  View* v3 = root.AddChildView(std::make_unique<View>());
  v3->SetPaintToLayer();

  auto decoration = std::make_unique<ViewDecoration>(v2);
  decoration->SetShadow(1);

  ASSERT_EQ(4u, root.layer()->children().size());
  EXPECT_EQ(v1->layer(), root.layer()->children()[0]);
  EXPECT_EQ(decoration->decoration()->layer(), root.layer()->children()[1]);
  EXPECT_EQ(v2->layer(), root.layer()->children()[2]);
  EXPECT_EQ(v3->layer(), root.layer()->children()[3]);

  decoration.reset();
  EXPECT_EQ(3u, root.layer()->children().size());
  EXPECT_EQ(v1->layer(), root.layer()->children()[0]);
  EXPECT_EQ(v2->layer(), root.layer()->children()[1]);
  EXPECT_EQ(v3->layer(), root.layer()->children()[2]);
}

TEST_F(ViewDecorationTest, DecorationBoundsFollowView) {
  View view;
  view.SetBoundsRect(gfx::Rect(10, 20, 30, 40));

  ViewDecoration decoration(&view);
  decoration.SetShadow(1);

  EXPECT_EQ(gfx::Rect(10, 20, 30, 40),
            decoration.decoration()->content_bounds());

  view.SetBoundsRect(gfx::Rect(100, 110, 120, 130));
  EXPECT_EQ(gfx::Rect(100, 110, 120, 130),
            decoration.decoration()->content_bounds());
}

TEST_F(ViewDecorationTest, DecorationBoundsFollowIndirectViewBoundsChange) {
  View root;
  root.SetPaintToLayer();
  root.SetBoundsRect(gfx::Rect(100, 100, 200, 200));

  View* parent = root.AddChildView(std::make_unique<View>());
  parent->SetBoundsRect(gfx::Rect(10, 20, 70, 80));
  View* view = parent->AddChildView(std::make_unique<View>());
  view->SetBoundsRect(gfx::Rect(5, 10, 20, 30));

  ViewDecoration decoration(view);
  decoration.SetShadow(1);
  EXPECT_EQ(gfx::Rect(15, 30, 20, 30),
            decoration.decoration()->content_bounds());

  parent->SetBoundsRect(gfx::Rect(5, 15, 60, 70));
  EXPECT_EQ(gfx::Rect(10, 25, 20, 30),
            decoration.decoration()->content_bounds());
}

TEST_F(ViewDecorationTest, ViewDestruction) {
  View root;
  root.SetPaintToLayer();
  root.SetBoundsRect(gfx::Rect(10, 20, 30, 40));

  View* v1 = root.AddChildView(std::make_unique<View>());
  ViewDecoration decoration(v1);
  decoration.SetShadow(1);
  EXPECT_EQ(2u, root.layer()->children().size());

  delete v1;
  EXPECT_TRUE(root.layer()->children().empty());
  EXPECT_FALSE(decoration.decoration());
  EXPECT_FALSE(decoration.shadow());
  EXPECT_FALSE(decoration.highlight_border());
}

TEST_F(ViewDecorationTest, DecorationKeepsLayerType) {
  View view;
  view.SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  view.SetBoundsRect(gfx::Rect(10, 20, 30, 40));
  ViewDecoration decoration(&view);
  decoration.SetShadow(1);
  EXPECT_TRUE(view.layer());
  EXPECT_TRUE(view.layer()->AsSolidColor());
}

// Tests the decoration layer will not shift when the view's layer is
// reparented to another layer.
TEST_F(ViewDecorationTest, NoShiftWhenReparentViewLayer) {
  View root1;
  root1.SetPaintToLayer();

  View* view = root1.AddChildView(std::make_unique<View>());
  view->SetPaintToLayer();
  view->SetBoundsRect(gfx::Rect(10, 20, 30, 40));

  ViewDecoration decoration(view);
  decoration.SetShadow(1);

  // Cache current decoration position.
  const gfx::Point pos = decoration.decoration()->layer()->bounds().origin();

  // Reparent the view's layer to another layer.
  View root2;
  root2.SetPaintToLayer();
  root2.AddChildView(root1.RemoveChildViewT(view));
  // Check if the decoration layer shifted.
  EXPECT_EQ(pos, decoration.decoration()->layer()->bounds().origin());
}

TEST_F(ViewDecorationTest, SetRoundedCorners) {
  View view;
  ViewDecoration decoration(&view);
  decoration.SetShadow(1);

  const gfx::RoundedCornersF radii(10, 20, 30, 40);
  decoration.SetRoundedCorners(radii);
  EXPECT_EQ(radii, decoration.decoration()->rounded_corners());
}

TEST_F(ViewDecorationTest, SetShadowAndHighlightBorder) {
  View view;
  view.SetBoundsRect(gfx::Rect(10, 20, 100, 100));

  ViewDecoration decoration(&view);
  EXPECT_FALSE(decoration.shadow());
  EXPECT_FALSE(decoration.highlight_border());
  EXPECT_FALSE(
      decoration.decoration()->decoration_layer_for_testing()->visible());

  decoration.SetHighlightBorder(SK_ColorWHITE, SK_ColorBLACK, /*thickness=*/1);
  ASSERT_TRUE(decoration.highlight_border());
  EXPECT_EQ(SK_ColorWHITE, decoration.highlight_border()->highlight_color());
  EXPECT_EQ(SK_ColorBLACK, decoration.highlight_border()->border_color());
  EXPECT_EQ(1, decoration.highlight_border()->thickness());
  EXPECT_TRUE(
      decoration.decoration()->decoration_layer_for_testing()->visible());
  EXPECT_EQ(gfx::Rect(10, 20, 100, 100),
            decoration.decoration()->layer()->bounds());

  const ui::decoration::Shadow::ElevationToColorsMap color_map = {
      {6, {SK_ColorRED, SK_ColorBLUE}},
      {8, {SK_ColorGREEN, SK_ColorYELLOW}},
  };
  decoration.SetShadow(6, ui::decoration::Shadow::Style::kMaterialDesign,
                       color_map);
  ASSERT_TRUE(decoration.shadow());
  EXPECT_EQ(6, decoration.shadow()->elevation());
  EXPECT_LT(decoration.decoration()->layer()->bounds().x(), 10);

  // Updating only the elevation preserves the existing shadow instance and its
  // color map.
  const ui::decoration::Shadow* shadow_ptr = decoration.shadow();
  decoration.SetShadow(8);
  EXPECT_EQ(shadow_ptr, decoration.shadow());
  EXPECT_EQ(8, decoration.shadow()->elevation());
  const auto spec =
      decoration.shadow()->GetSpec(gfx::Rect(100, 100), gfx::RoundedCornersF());
  ASSERT_TRUE(spec.has_value());
  EXPECT_EQ(SK_ColorGREEN, (*spec)[0].color());

  decoration.ClearShadow();
  EXPECT_FALSE(decoration.shadow());
  EXPECT_TRUE(decoration.highlight_border());
  EXPECT_EQ(gfx::Rect(10, 20, 100, 100),
            decoration.decoration()->layer()->bounds());

  decoration.ClearHighlightBorder();
  EXPECT_FALSE(decoration.highlight_border());
  EXPECT_FALSE(
      decoration.decoration()->decoration_layer_for_testing()->visible());
}

}  // namespace views
