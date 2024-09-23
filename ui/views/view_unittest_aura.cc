// Copyright 2014 The Chromium Authors
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
std::unique_ptr<Widget> CreateControlWidget(aura::Window* parent,
                                            const gfx::Rect& bounds) {
  Widget::InitParams params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                            Widget::InitParams::TYPE_CONTROL);
  params.parent = parent;
  params.bounds = bounds;
  auto widget = std::make_unique<Widget>();
  widget->Init(std::move(params));
  return widget;
}

// Returns a view with a layer with the passed in |bounds| and |layer_name|.
// The caller takes ownership of the returned view.
std::unique_ptr<View> CreateViewWithLayer(const gfx::Rect& bounds,
                                          const char* layer_name) {
  auto view = std::make_unique<View>();
  view->SetBoundsRect(bounds);
  view->SetPaintToLayer();
  view->layer()->SetName(layer_name);
  return view;
}

}  // namespace

class ViewAuraTest : public ViewsTestBase {
 public:
  ViewAuraTest() = default;

  ViewAuraTest(const ViewAuraTest&) = delete;
  ViewAuraTest& operator=(const ViewAuraTest&) = delete;

  ~ViewAuraTest() override = default;

  const View::Views& GetViewsWithLayers(Widget* widget) {
    return widget->GetViewsWithLayers();
  }
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
  std::unique_ptr<Widget> w1 =
      CreateControlWidget(GetContext(), gfx::Rect(0, 0, 100, 100));
  w1->GetNativeView()->layer()->SetName("w1");

  auto* v1 = w1->GetRootView()->AddChildView(
      CreateViewWithLayer(gfx::Rect(0, 3, 100, 103), "v1"));
  ui::Layer* v1_layer = v1->layer();
  auto* v2 = w1->GetRootView()->AddChildView(std::make_unique<View>());
  v2->SetBounds(0, 1, 100, 101);
  v2->AddChildView(std::make_unique<View>())->SetBounds(0, 2, 100, 102);
  auto* v4 =
      v2->AddChildView(CreateViewWithLayer(gfx::Rect(0, 4, 100, 104), "v4"));
  ui::Layer* v4_layer = v4->layer();

  auto* w2_host_view =
      w1->GetRootView()->AddChildView(std::make_unique<View>());
  auto* v7 = w1->GetRootView()->AddChildView(
      CreateViewWithLayer(gfx::Rect(0, 4, 100, 104), "v7"));
  ui::Layer* v7_layer = v7->layer();

  auto* v8 =
      v7->AddChildView(CreateViewWithLayer(gfx::Rect(0, 4, 100, 104), "v8"));
  ui::Layer* v8_layer = v8->layer();

  auto* v9 =
      v7->AddChildView(CreateViewWithLayer(gfx::Rect(0, 4, 100, 104), "v9"));
  ui::Layer* v9_layer = v9->layer();

  std::unique_ptr<Widget> w2 =
      CreateControlWidget(w1->GetNativeView(), gfx::Rect(0, 5, 100, 105));
  w2->GetNativeView()->layer()->SetName("w2");
  w2->GetNativeView()->SetProperty(kHostViewKey, w2_host_view);

  auto* v5 = w2->GetRootView()->AddChildView(
      CreateViewWithLayer(gfx::Rect(0, 6, 100, 106), "v5"));
  auto* v6 =
      v5->AddChildView(CreateViewWithLayer(gfx::Rect(0, 7, 100, 107), "v6"));
  ui::Layer* v6_layer = v6->layer();

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
  View::Views old_w1_views_with_layers = GetViewsWithLayers(w1.get());
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
    ASSERT_EQ("v1 v4 w2 v7", ui::test::ChildLayerNamesAsString(*w1_layer));
    ASSERT_EQ("v5", ui::test::ChildLayerNamesAsString(*w2_layer));
    ASSERT_EQ("v6", ui::test::ChildLayerNamesAsString(*v5_layer));
    EXPECT_EQ("v8 v9", ui::test::ChildLayerNamesAsString(*v7_layer));

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
    View::Views new_w1_views_with_layers = GetViewsWithLayers(w1.get());
    ASSERT_EQ(3u, new_w1_views_with_layers.size());
    EXPECT_EQ(v1, new_w1_views_with_layers[0]);
    EXPECT_EQ(v4, new_w1_views_with_layers[1]);
    EXPECT_EQ(v7, new_w1_views_with_layers[2]);
  }
  w1->CloseNow();
}

}  // namespace views
