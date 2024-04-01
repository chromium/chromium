// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_shadow.h"

#include "ui/compositor/layer.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/shadow_util.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace views {

using ViewShadowTest = ViewsTestBase;

TEST_F(ViewShadowTest, UseShadow) {
  View root;
  root.SetPaintToLayer();

  View* v1 = root.AddChildView(std::make_unique<View>());
  v1->SetPaintToLayer();
  View* v2 = root.AddChildView(std::make_unique<View>());
  v2->SetPaintToLayer();
  View* v3 = root.AddChildView(std::make_unique<View>());
  v3->SetPaintToLayer();

  auto shadow = std::make_unique<ViewShadow>(v2, 1);

  ASSERT_EQ(4u, root.layer()->children().size());
  EXPECT_EQ(v1->layer(), root.layer()->children()[0]);
  EXPECT_EQ(shadow->shadow()->layer(), root.layer()->children()[1]);
  EXPECT_EQ(v2->layer(), root.layer()->children()[2]);
  EXPECT_EQ(v3->layer(), root.layer()->children()[3]);

  shadow.reset();
  EXPECT_EQ(3u, root.layer()->children().size());
  EXPECT_EQ(v1->layer(), root.layer()->children()[0]);
  EXPECT_EQ(v2->layer(), root.layer()->children()[1]);
  EXPECT_EQ(v3->layer(), root.layer()->children()[2]);
}

TEST_F(ViewShadowTest, ShadowBoundsFollowView) {
  View view;
  view.SetBoundsRect(gfx::Rect(10, 20, 30, 40));

  ViewShadow shadow(&view, 1);

  EXPECT_EQ(gfx::Rect(10, 20, 30, 40), shadow.shadow()->content_bounds());

  view.SetBoundsRect(gfx::Rect(100, 110, 120, 130));
  EXPECT_EQ(gfx::Rect(100, 110, 120, 130), shadow.shadow()->content_bounds());
}

TEST_F(ViewShadowTest, ShadowBoundsFollowIndirectViewBoundsChange) {
  View root;
  root.SetPaintToLayer();
  root.SetBoundsRect(gfx::Rect(100, 100, 200, 200));

  View* parent = root.AddChildView(std::make_unique<View>());
  parent->SetBoundsRect(gfx::Rect(10, 20, 70, 80));
  View* view = parent->AddChildView(std::make_unique<View>());
  view->SetBoundsRect(gfx::Rect(5, 10, 20, 30));

  ViewShadow shadow(view, 1);
  EXPECT_EQ(gfx::Rect(15, 30, 20, 30), shadow.shadow()->content_bounds());

  parent->SetBoundsRect(gfx::Rect(5, 15, 60, 70));
  EXPECT_EQ(gfx::Rect(10, 25, 20, 30), shadow.shadow()->content_bounds());
}

TEST_F(ViewShadowTest, ViewDestruction) {
  View root;
  root.SetPaintToLayer();
  root.SetBoundsRect(gfx::Rect(10, 20, 30, 40));

  View* v1 = root.AddChildView(std::make_unique<View>());
  ViewShadow shadow(v1, 1);
  EXPECT_EQ(2u, root.layer()->children().size());

  delete v1;
  EXPECT_TRUE(root.layer()->children().empty());
}

TEST_F(ViewShadowTest, ShadowKeepsLayerType) {
  View view;
  view.SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  view.SetBoundsRect(gfx::Rect(10, 20, 30, 40));
  ViewShadow shadow(&view, 1);
  EXPECT_TRUE(view.layer());
  EXPECT_EQ(ui::LAYER_SOLID_COLOR, view.layer()->type());
}

// Tests the shadow layer will not shift when the view's layer is reparented to
// another layer.
TEST_F(ViewShadowTest, NoShiftWhenReparentViewLayer) {
  View root1;
  root1.SetPaintToLayer();

  View* view = root1.AddChildView(std::make_unique<View>());
  view->SetPaintToLayer();
  view->SetBoundsRect(gfx::Rect(10, 20, 30, 40));

  ViewShadow shadow(view, 1);

  // Cache current shadow position.
  const gfx::Point pos = shadow.shadow()->layer()->bounds().origin();

  // Reparent the view's layer to another layer.
  View root2;
  root2.SetPaintToLayer();
  root2.AddChildView(view);
  // Check if the shadow layer shifted.
  EXPECT_EQ(pos, shadow.shadow()->layer()->bounds().origin());
}

}  // namespace views
