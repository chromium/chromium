// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/test_layers.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

// Sets the name of |window| and |window|'s layer to |name|.
void SetWindowAndLayerName(aura::Window* window, const std::string& name) {
  window->SetName(name);
  window->layer()->SetName(name);
}

// Returns a string containing the name of each of the child windows (bottommost
// first) of |parent|. The format of the string is "name1 name2 name3 ...".
std::string ChildWindowNamesAsString(const aura::Window& parent) {
  std::string names;
  for (const aura::Window* child : parent.children()) {
    if (!names.empty())
      names += " ";
    names += child->GetName();
  }
  return names;
}

class WindowReordererTest : public ViewsTestBase {
 protected:
  std::unique_ptr<Widget> CreateControlWidget(aura::Window* parent) {
    Widget::InitParams params =
        CreateParamsForTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                  Widget::InitParams::TYPE_CONTROL);
    params.parent = parent;
    return CreateTestWidget(std::move(params));
  }
};

// Test that views with layers and views with associated windows are reordered
// according to the view hierarchy.
TEST_F(WindowReordererTest, Basic) {
  std::unique_ptr<Widget> parent = CreateControlWidget(root_window());
  parent->Show();
  aura::Window* parent_window = parent->GetNativeWindow();

  View* contents_view = parent->SetContentsView(std::make_unique<View>());

  // 1) Test that layers for views and layers for windows associated to a host
  // view are stacked below the layers for any windows not associated to a host
  // view.
  auto* v = contents_view->AddChildView(std::make_unique<View>());
  v->SetPaintToLayer();
  v->layer()->SetName("v");

  std::unique_ptr<Widget> w1 = CreateControlWidget(parent_window);
  SetWindowAndLayerName(w1->GetNativeView(), "w1");
  w1->Show();
  std::unique_ptr<Widget> w2 = CreateControlWidget(parent_window);
  SetWindowAndLayerName(w2->GetNativeView(), "w2");
  w2->Show();

  EXPECT_EQ("w1 w2", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("v w1 w2",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  auto* host_view2 = contents_view->AddChildView(std::make_unique<View>());
  w2->GetNativeView()->SetProperty(kHostViewKey, host_view2);
  EXPECT_EQ("w2 w1", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("v w2 w1",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  auto* host_view1 = contents_view->AddChildViewAt(std::make_unique<View>(), 0);
  w1->GetNativeView()->SetProperty(kHostViewKey, host_view1);
  EXPECT_EQ("w1 w2", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("w1 v w2",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  // 2) Test the z-order of the windows and layers as a result of reordering the
  // views.
  contents_view->ReorderChildView(host_view1, contents_view->children().size());
  EXPECT_EQ("w2 w1", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("v w2 w1",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  contents_view->ReorderChildView(host_view2, contents_view->children().size());
  EXPECT_EQ("w1 w2", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("v w1 w2",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  // 3) Test the z-order of the windows and layers as a result of reordering the
  // views in situations where the window order remains unchanged.
  contents_view->ReorderChildView(v, contents_view->children().size());
  EXPECT_EQ("w1 w2", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("w1 w2 v",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  contents_view->ReorderChildView(host_view2, contents_view->children().size());
  EXPECT_EQ("w1 w2", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("w1 v w2",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));
}

// Test that different orderings of:
// - adding a window to a parent widget
// - adding a "host" view to a parent widget
// - associating the "host" view and window
// all correctly reorder the child windows and layers.
TEST_F(WindowReordererTest, Association) {
  std::unique_ptr<Widget> parent = CreateControlWidget(root_window());
  parent->Show();
  aura::Window* parent_window = parent->GetNativeWindow();

  View* contents_view = parent->SetContentsView(std::make_unique<View>());

  aura::Window* w1 =
      aura::test::CreateTestWindowWithId(0, parent->GetNativeWindow());
  SetWindowAndLayerName(w1, "w1");

  aura::Window* w2 = aura::test::CreateTestWindowWithId(0, nullptr);
  SetWindowAndLayerName(w2, "w2");

  // 1) Test that parenting the window to the parent widget last results in a
  //    correct ordering of child windows and layers.
  auto* host_view2 = contents_view->AddChildView(std::make_unique<View>());
  w2->SetProperty(views::kHostViewKey, host_view2);
  EXPECT_EQ("w1", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("w1", ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  parent_window->AddChild(w2);
  EXPECT_EQ("w2 w1", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("w2 w1",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  // 2) Test that associating the window and "host" view last results in a
  // correct ordering of child windows and layers.
  auto* host_view1 = contents_view->AddChildViewAt(std::make_unique<View>(), 0);
  EXPECT_EQ("w2 w1", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("w2 w1",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  w1->SetProperty(views::kHostViewKey, host_view1);
  EXPECT_EQ("w1 w2", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("w1 w2",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  // 3) Test that parenting the "host" view to the parent widget last results
  // in a correct ordering of child windows and layers.
  contents_view->RemoveChildView(host_view2);
  contents_view->AddChildViewAt(host_view2, 0);
  EXPECT_EQ("w2 w1", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("w2 w1",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));
}

// It is possible to associate a window to a view which has a parent layer
// (other than the widget layer). In this case, the parent layer of the host
// view and the parent layer of the associated window are different. Test that
// the layers and windows are properly reordered in this case.
TEST_F(WindowReordererTest, HostViewParentHasLayer) {
  std::unique_ptr<Widget> parent = CreateControlWidget(root_window());
  parent->Show();
  aura::Window* parent_window = parent->GetNativeWindow();

  View* contents_view = parent->SetContentsView(std::make_unique<View>());

  // Create the following view hierarchy. (*) denotes views which paint to a
  // layer.
  //
  // contents_view
  // +-- v1
  //     +-- v11*
  //     +-- v12 (attached window)
  //     +-- v13*
  // +--v2*

  View* v1 = contents_view->AddChildView(std::make_unique<View>());

  View* v11 = v1->AddChildView(std::make_unique<View>());
  v11->SetPaintToLayer();
  v11->layer()->SetName("v11");

  std::unique_ptr<Widget> w = CreateControlWidget(parent_window);
  SetWindowAndLayerName(w->GetNativeView(), "w");
  w->Show();

  View* v12 = v1->AddChildView(std::make_unique<View>());
  w->GetNativeView()->SetProperty(kHostViewKey, v12);

  View* v13 = v1->AddChildView(std::make_unique<View>());
  v13->SetPaintToLayer();
  v13->layer()->SetName("v13");

  View* v2 = contents_view->AddChildView(std::make_unique<View>());
  v2->SetPaintToLayer();
  v2->layer()->SetName("v2");

  // Test intial state.
  EXPECT_EQ("w", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("v11 w v13 v2",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  // |w|'s layer should be stacked above |v1|'s layer.
  v1->SetPaintToLayer();
  v1->layer()->SetName("v1");
  EXPECT_EQ("w", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("v1 w v2",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  // Test moving the host view from one view with a layer to another.
  v2->AddChildView(v1->RemoveChildViewT(v12));
  EXPECT_EQ("w", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("v1 v2 w",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));
}

// Test that a layer added beneath a view is restacked correctly.
TEST_F(WindowReordererTest, ViewWithLayerBeneath) {
  std::unique_ptr<Widget> parent = CreateControlWidget(root_window());
  parent->Show();

  aura::Window* parent_window = parent->GetNativeWindow();

  View* contents_view = parent->SetContentsView(std::make_unique<View>());

  View* view_with_layer_beneath =
      contents_view->AddChildView(std::make_unique<View>());
  ui::Layer layer_beneath;
  view_with_layer_beneath->AddLayerToRegion(&layer_beneath,
                                            LayerRegion::kBelow);

  ASSERT_NE(nullptr, view_with_layer_beneath->layer());
  view_with_layer_beneath->layer()->SetName("view");
  layer_beneath.SetName("beneath");

  // Verify that the initial ordering is correct.
  EXPECT_EQ("beneath view",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  // Add a hosted window to make WindowReorderer::ReorderChildWindows() restack
  // layers.
  std::unique_ptr<Widget> child_widget = CreateControlWidget(parent_window);
  SetWindowAndLayerName(child_widget->GetNativeView(), "child_widget");
  child_widget->Show();
  View* host_view = contents_view->AddChildView(std::make_unique<View>());
  child_widget->GetNativeView()->SetProperty(kHostViewKey, host_view);

  // Verify the new order is correct.
  EXPECT_EQ("beneath view child_widget",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));
}

}  // namespace
}  // namespace views
