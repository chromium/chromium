// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view.h"

#include <memory>

#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/test/test_layers.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace views {

namespace {

// Creates a widget of TYPE_CONTROL.
// The caller takes ownership of the returned widget.
Widget* CreateControlWidget(aura::Window* parent, const gfx::Rect& bounds) {
  Widget::InitParams params(Widget::InitParams::TYPE_CONTROL);
  params.parent = parent;
  params.bounds = bounds;
  Widget* widget = new Widget();
  widget->Init(std::move(params));
  return widget;
}

// Returns a view with a layer with the passed in |bounds| and |layer_name|.
// The caller takes ownership of the returned view.
View* CreateViewWithLayer(const gfx::Rect& bounds, const char* layer_name) {
  View* view = new View();
  view->SetBoundsRect(bounds);
  view->SetPaintToLayer();
  view->layer()->set_name(layer_name);
  return view;
}

}  // namespace

class ViewAuraTest : public ViewsTestBase {
 public:
  ViewAuraTest() = default;
  ~ViewAuraTest() override = default;

  const View::Views& GetViewsWithLayers(Widget* widget) {
    return widget->GetViewsWithLayers();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewAuraTest);
};

// Test that wm::RecreateLayers() recreates the layers for all child windows and
// all child views and that the z-order of the recreated layers matches that of
// the original layers.
// Test hierarchy:
// w1
// +-- v1
// +-- v2 (no layer)
//     +-- v3 (no layer)
//     +-- v4
// +-- w2
//     +-- v5
//         +-- v6
// +-- v7
//     +-- v8
//     +-- v9
TEST_F(ViewAuraTest, RecreateLayersWithWindows) {
  Widget* w1 = CreateControlWidget(GetContext(), gfx::Rect(0, 0, 100, 100));
  w1->GetNativeView()->layer()->set_name("w1");

  View* v2 = new View();
  v2->SetBounds(0, 1, 100, 101);
  View* v3 = new View();
  v3->SetBounds(0, 2, 100, 102);
  View* w2_host_view = new View();

  View* v1 = CreateViewWithLayer(gfx::Rect(0, 3, 100, 103), "v1");
  ui::Layer* v1_layer = v1->layer();
  w1->GetRootView()->AddChildView(v1);
  w1->GetRootView()->AddChildView(v2);
  v2->AddChildView(v3);
  View* v4 = CreateViewWithLayer(gfx::Rect(0, 4, 100, 104), "v4");
  ui::Layer* v4_layer = v4->layer();
  v2->AddChildView(v4);

  w1->GetRootView()->AddChildView(w2_host_view);
  View* v7 = CreateViewWithLayer(gfx::Rect(0, 4, 100, 104), "v7");
  ui::Layer* v7_layer = v7->layer();
  w1->GetRootView()->AddChildView(v7);

  View* v8 = CreateViewWithLayer(gfx::Rect(0, 4, 100, 104), "v8");
  ui::Layer* v8_layer = v8->layer();
  v7->AddChildView(v8);

  View* v9 = CreateViewWithLayer(gfx::Rect(0, 4, 100, 104), "v9");
  ui::Layer* v9_layer = v9->layer();
  v7->AddChildView(v9);

  Widget* w2 =
      CreateControlWidget(w1->GetNativeView(), gfx::Rect(0, 5, 100, 105));
  w2->GetNativeView()->layer()->set_name("w2");
  w2->GetNativeView()->SetProperty(kHostViewKey, w2_host_view);

  View* v5 = CreateViewWithLayer(gfx::Rect(0, 6, 100, 106), "v5");
  w2->GetRootView()->AddChildView(v5);
  View* v6 = CreateViewWithLayer(gfx::Rect(0, 7, 100, 107), "v6");
  ui::Layer* v6_layer = v6->layer();
  v5->AddChildView(v6);

  // Test the initial order of the layers.
  ui::Layer* w1_layer = w1->GetNativeView()->layer();
  ASSERT_EQ("w1", w1_layer->name());
  ASSERT_EQ("v1 v4 w2 v7", ui::test::ChildLayerNamesAsString(*w1_layer));
  ui::Layer* w2_layer = w1_layer->children()[2];
  ASSERT_EQ("v5", ui::test::ChildLayerNamesAsString(*w2_layer));
  ui::Layer* v5_layer = w2_layer->children()[0];
  ASSERT_EQ("v6", ui::test::ChildLayerNamesAsString(*v5_layer));

  // Verify the value of Widget::GetRootLayers(). It should only include layers
  // from layer-backed Views descended from the Widget's root View.
  View::Views old_w1_views_with_layers = GetViewsWithLayers(w1);
  ASSERT_EQ(3u, old_w1_views_with_layers.size());
  EXPECT_EQ(v1, old_w1_views_with_layers[0]);
  EXPECT_EQ(v4, old_w1_views_with_layers[1]);
  EXPECT_EQ(v7, old_w1_views_with_layers[2]);

  {
    std::unique_ptr<ui::LayerTreeOwner> cloned_owner(
        wm::RecreateLayers(w1->GetNativeView()));
    EXPECT_EQ(w1_layer, cloned_owner->root());
    EXPECT_NE(w1_layer, w1->GetNativeView()->layer());

    // The old layers should still exist and have the same hierarchy.
    ASSERT_EQ("w1", w1_layer->name());
    ASSERT_EQ("v1 v4 w2 v7", ui::test::ChildLayerNamesAsString(*w1_layer));
    ASSERT_EQ("v5", ui::test::ChildLayerNamesAsString(*w2_layer));
    ASSERT_EQ("v6", ui::test::ChildLayerNamesAsString(*v5_layer));
    EXPECT_EQ("v8 v9", ui::test::ChildLayerNamesAsString(*v7_layer));

    ASSERT_EQ(4u, w1_layer->children().size());
    EXPECT_EQ(v1_layer, w1_layer->children()[0]);
    EXPECT_EQ(v4_layer, w1_layer->children()[1]);
    EXPECT_EQ(w2_layer, w1_layer->children()[2]);
    EXPECT_EQ(v7_layer, w1_layer->children()[3]);

    ASSERT_EQ(1u, w2_layer->children().size());
    EXPECT_EQ(v5_layer, w2_layer->children()[0]);

    ASSERT_EQ(1u, v5_layer->children().size());
    EXPECT_EQ(v6_layer, v5_layer->children()[0]);

    ASSERT_EQ(0u, v6_layer->children().size());

    EXPECT_EQ(2u, v7_layer->children().size());
    EXPECT_EQ(v8_layer, v7_layer->children()[0]);
    EXPECT_EQ(v9_layer, v7_layer->children()[1]);

    // The cloned layers should have the same hierarchy as old, but with new
    // ui::Layer instances.
    ui::Layer* w1_new_layer = w1->GetNativeView()->layer();
    EXPECT_EQ("w1", w1_new_layer->name());
    EXPECT_NE(w1_layer, w1_new_layer);
    ui::Layer* v1_new_layer = w1_new_layer->children()[0];
    ui::Layer* v4_new_layer = w1_new_layer->children()[1];
    ASSERT_EQ("v1 v4 w2 v7", ui::test::ChildLayerNamesAsString(*w1_new_layer));
    EXPECT_NE(v1_layer, v1_new_layer);
    EXPECT_NE(v4_layer, v4_new_layer);
    ui::Layer* w2_new_layer = w1_new_layer->children()[2];
    ASSERT_EQ("v5", ui::test::ChildLayerNamesAsString(*w2_new_layer));
    ui::Layer* v5_new_layer = w2_new_layer->children()[0];
    ASSERT_EQ("v6", ui::test::ChildLayerNamesAsString(*v5_new_layer));
    ui::Layer* v7_new_layer = w1_new_layer->children()[3];
    ASSERT_EQ("v8 v9", ui::test::ChildLayerNamesAsString(*v7_new_layer));
    EXPECT_NE(v7_layer, v7_new_layer);

    // Ensure Widget::GetViewsWithLayers() is correctly updated.
    View::Views new_w1_views_with_layers = GetViewsWithLayers(w1);
    ASSERT_EQ(3u, new_w1_views_with_layers.size());
    EXPECT_EQ(v1, new_w1_views_with_layers[0]);
    EXPECT_EQ(v4, new_w1_views_with_layers[1]);
    EXPECT_EQ(v7, new_w1_views_with_layers[2]);
  }
  w1->CloseNow();
}

}  // namespace views
