// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

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

// Creates a control widget with the passed in parameters.
// The caller takes ownership of the returned widget.
Widget* CreateControlWidget(aura::Window* parent, const gfx::Rect& bounds) {
  Widget::InitParams params(Widget::InitParams::TYPE_CONTROL);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = parent;
  params.bounds = bounds;
  Widget* widget = new Widget();
  widget->Init(std::move(params));
  return widget;
}

// Sets the name of |window| and |window|'s layer to |name|.
void SetWindowAndLayerName(aura::Window* window, const std::string& name) {
  window->SetName(name);
  window->layer()->set_name(name);
}

// Returns a string containing the name of each of the child windows (bottommost
// first) of |parent|. The format of the string is "name1 name2 name3 ...".
std::string ChildWindowNamesAsString(const aura::Window& parent) {
  std::string names;
  for (const auto* child : parent.children()) {
    if (!names.empty())
      names += " ";
    names += child->GetName();
  }
  return names;
}

using WindowReordererTest = ViewsTestBase;

// Test that views with layers and views with associated windows are reordered
// according to the view hierarchy.
TEST_F(WindowReordererTest, Basic) {
  std::unique_ptr<Widget> parent(
      CreateControlWidget(root_window(), gfx::Rect(0, 0, 100, 100)));
  parent->Show();
  aura::Window* parent_window = parent->GetNativeWindow();

  View* contents_view = new View();
  parent->SetContentsView(contents_view);

  // 1) Test that layers for views and layers for windows associated to a host
  // view are stacked below the layers for any windows not associated to a host
  // view.
  View* v = new View();
  v->SetPaintToLayer();
  v->layer()->set_name("v");
  contents_view->AddChildView(v);

  std::unique_ptr<Widget> w1(
      CreateControlWidget(parent_window, gfx::Rect(0, 1, 100, 101)));
  SetWindowAndLayerName(w1->GetNativeView(), "w1");
  w1->Show();
  std::unique_ptr<Widget> w2(
      CreateControlWidget(parent_window, gfx::Rect(0, 2, 100, 102)));
  SetWindowAndLayerName(w2->GetNativeView(), "w2");
  w2->Show();

  EXPECT_EQ("w1 w2", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("v w1 w2",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  View* host_view2 = new View();
  contents_view->AddChildView(host_view2);
  w2->GetNativeView()->SetProperty(kHostViewKey, host_view2);
  EXPECT_EQ("w2 w1", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("v w2 w1",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  View* host_view1 = new View();
  w1->GetNativeView()->SetProperty(kHostViewKey, host_view1);
  contents_view->AddChildViewAt(host_view1, 0);
  EXPECT_EQ("w1 w2", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("w1 v w2",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  // 2) Test the z-order of the windows and layers as a result of reordering the
  // views.
  contents_view->ReorderChildView(host_view1, -1);
  EXPECT_EQ("w2 w1", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("v w2 w1",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  contents_view->ReorderChildView(host_view2, -1);
  EXPECT_EQ("w1 w2", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("v w1 w2",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  // 3) Test the z-order of the windows and layers as a result of reordering the
  // views in situations where the window order remains unchanged.
  contents_view->ReorderChildView(v, -1);
  EXPECT_EQ("w1 w2", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("w1 w2 v",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  contents_view->ReorderChildView(host_view2, -1);
  EXPECT_EQ("w1 w2", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("w1 v w2",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  // Work around for bug in NativeWidgetAura.
  // TODO: fix bug and remove this.
  parent->Close();
}

// Test that different orderings of:
// - adding a window to a parent widget
// - adding a "host" view to a parent widget
// - associating the "host" view and window
// all correctly reorder the child windows and layers.
TEST_F(WindowReordererTest, Association) {
  std::unique_ptr<Widget> parent(
      CreateControlWidget(root_window(), gfx::Rect(0, 0, 100, 100)));
  parent->Show();
  aura::Window* parent_window = parent->GetNativeWindow();

  View* contents_view = new View();
  parent->SetContentsView(contents_view);

  aura::Window* w1 = aura::test::CreateTestWindowWithId(0,
      parent->GetNativeWindow());
  SetWindowAndLayerName(w1, "w1");

  aura::Window* w2 = aura::test::CreateTestWindowWithId(0, nullptr);
  SetWindowAndLayerName(w2, "w2");

  View* host_view2 = new View();

  // 1) Test that parenting the window to the parent widget last results in a
  //    correct ordering of child windows and layers.
  contents_view->AddChildView(host_view2);
  w2->SetProperty(views::kHostViewKey, host_view2);
  EXPECT_EQ("w1", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("w1",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  parent_window->AddChild(w2);
  EXPECT_EQ("w2 w1", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("w2 w1",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  // 2) Test that associating the window and "host" view last results in a
  // correct ordering of child windows and layers.
  View* host_view1 = new View();
  contents_view->AddChildViewAt(host_view1, 0);
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

  // Work around for bug in NativeWidgetAura.
  // TODO: fix bug and remove this.
  parent->Close();
}

// It is possible to associate a window to a view which has a parent layer
// (other than the widget layer). In this case, the parent layer of the host
// view and the parent layer of the associated window are different. Test that
// the layers and windows are properly reordered in this case.
TEST_F(WindowReordererTest, HostViewParentHasLayer) {
  std::unique_ptr<Widget> parent(
      CreateControlWidget(root_window(), gfx::Rect(0, 0, 100, 100)));
  parent->Show();
  aura::Window* parent_window = parent->GetNativeWindow();

  View* contents_view = new View();
  parent->SetContentsView(contents_view);

  // Create the following view hierarchy. (*) denotes views which paint to a
  // layer.
  //
  // contents_view
  // +-- v1
  //     +-- v11*
  //     +-- v12 (attached window)
  //     +-- v13*
  // +--v2*

  View* v1 = new View();
  contents_view->AddChildView(v1);

  View* v11 = new View();
  v11->SetPaintToLayer();
  v11->layer()->set_name("v11");
  v1->AddChildView(v11);

  std::unique_ptr<Widget> w(
      CreateControlWidget(parent_window, gfx::Rect(0, 1, 100, 101)));
  SetWindowAndLayerName(w->GetNativeView(), "w");
  w->Show();

  View* v12 = new View();
  v1->AddChildView(v12);
  w->GetNativeView()->SetProperty(kHostViewKey, v12);

  View* v13 = new View();
  v13->SetPaintToLayer();
  v13->layer()->set_name("v13");
  v1->AddChildView(v13);

  View* v2 = new View();
  v2->SetPaintToLayer();
  v2->layer()->set_name("v2");
  contents_view->AddChildView(v2);

  // Test intial state.
  EXPECT_EQ("w", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("v11 w v13 v2",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  // |w|'s layer should be stacked above |v1|'s layer.
  v1->SetPaintToLayer();
  v1->layer()->set_name("v1");
  EXPECT_EQ("w", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("v1 w v2",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  // Test moving the host view from one view with a layer to another.
  v1->RemoveChildView(v12);
  v2->AddChildView(v12);
  EXPECT_EQ("w", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("v1 v2 w",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  // Work around for bug in NativeWidgetAura.
  // TODO: fix bug and remove this.
  parent->Close();
}

// Test that a layer added beneath a view is restacked correctly.
TEST_F(WindowReordererTest, ViewWithLayerBeneath) {
  std::unique_ptr<Widget> parent(
      CreateControlWidget(root_window(), gfx::Rect(0, 0, 100, 100)));
  parent->Show();

  aura::Window* parent_window = parent->GetNativeWindow();

  View* contents_view = new View;
  parent->SetContentsView(contents_view);

  View* view_with_layer_beneath =
      contents_view->AddChildView(std::make_unique<View>());
  ui::Layer layer_beneath;
  view_with_layer_beneath->AddLayerBeneathView(&layer_beneath);

  ASSERT_NE(nullptr, view_with_layer_beneath->layer());
  view_with_layer_beneath->layer()->set_name("view");
  layer_beneath.set_name("beneath");

  // Verify that the initial ordering is correct.
  EXPECT_EQ("beneath view",
            ui::test::ChildLayerNamesAsString(*parent_window->layer()));

  // Add a hosted window to make WindowReorderer::ReorderChildWindows() restack
  // layers.
  std::unique_ptr<Widget> child_widget(
      CreateControlWidget(parent_window, gfx::Rect(gfx::Rect(0, 0, 50, 50))));
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
