// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/window_reorderer.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

// Returns a string containing the name of each of the child windows (bottommost
// first) of |parent|. The format of the string is "name1 name2 name3 ...".
std::string ChildWindowNamesAsString(const aura::Window& parent) {
  std::string names;
  for (const aura::Window* child : parent.children()) {
    if (!names.empty()) {
      names += " ";
    }
    names += child->GetName();
  }
  return names;
}

void AccumulateLayerNames(ui::Layer* layer, std::vector<std::string>* names) {
  if (layer->name() == "NativeViewHost") {
    if (!layer->children().empty()) {
      names->push_back(layer->children()[0]->name());
    } else {
      names->push_back("NativeViewHost(empty)");
    }
  } else {
    names->push_back(layer->name());
  }
  for (ui::Layer* child : layer->children()) {
    if (layer->name() != "NativeViewHost") {
      AccumulateLayerNames(child, names);
    }
  }
}

std::string FlattenedChildLayerNames(ui::Layer* parent_layer) {
  std::vector<std::string> names;
  for (ui::Layer* child : parent_layer->children()) {
    AccumulateLayerNames(child, &names);
  }
  return base::JoinString(names, " ");
}

void StackWindows(aura::Window* parent,
                  std::initializer_list<aura::Window*> windows) {
  aura::Window* prev = nullptr;
  for (aura::Window* window : windows) {
    if (prev) {
      parent->StackChildAbove(window, prev);
    } else {
      parent->StackChildAtBottom(window);
    }
    prev = window;
  }
}

void StackViews(View* parent, std::initializer_list<View*> views) {
  size_t index = 0;
  for (View* view : views) {
    parent->ReorderChildView(view, index++);
  }
}

class WindowReordererTest : public ViewsTestBase {
 public:
  std::unique_ptr<Widget> CreateControlWidget(aura::Window* parent,
                                              const std::string& name = "") {
    Widget::InitParams params =
        CreateParamsForTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                  Widget::InitParams::TYPE_CONTROL);
    params.parent = parent;
    params.name = name;
    return CreateTestWidget(std::move(params));
  }

 protected:
  void SetUp() override {
    // Enable NativeViewHostManagesLayers to force WindowReorderer.
    feature_list_.InitAndEnableFeature(features::kNativeViewHostManagesLayers);
    ViewsTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Helper class to build a view and window hierarchy for testing.
// It simplifies creating and hosting windows, and ensures that windows are
// stacked in the order they are added unless overridden.
class TreeBuilder {
 public:
  TreeBuilder(WindowReordererTest* test, Widget* parent_widget)
      : test_(test),
        parent_widget_(parent_widget),
        parent_window_(parent_widget->GetNativeWindow()) {}
  TreeBuilder(const TreeBuilder&) = delete;
  TreeBuilder& operator=(const TreeBuilder&) = delete;
  ~TreeBuilder() = default;

  aura::Window* AddUnassociatedWindow(const std::string& name) {
    aura::Window* window =
        aura::test::CreateTestWindow(
            {.parent = parent_window_, .bounds = {100, 100}, .window_id = 99})
            .release();
    window->SetName(name);
    window->Show();
    StackWindow(window);
    return window;
  }

  NativeViewHost* AddNativeViewHost(const std::string& name) {
    std::unique_ptr<Widget> w_assoc =
        test_->CreateControlWidget(parent_window_, name);
    w_assoc->Show();

    auto* contents_view = parent_widget_->GetContentsView();
    auto* host =
        contents_view->AddChildView(std::make_unique<NativeViewHost>());
    host->Attach(w_assoc->GetNativeView());

    StackWindow(w_assoc->GetNativeView());

    widgets_.push_back(std::move(w_assoc));

    return host;
  }

  // Hosts an existing native view at the specified index (or appends if
  // nullopt). The lifetime of the widget associated with the native_view is
  // managed by the caller.
  // Note: This method does not call StackWindow, relying on WindowReorderer
  // to perform the correct stacking when the NativeViewHost is attached.
  // This avoids overriding the correct order when adding views out of order.
  NativeViewHost* AddNativeViewHost(
      gfx::NativeView native_view,
      std::optional<size_t> index = std::nullopt) {
    auto* contents_view = parent_widget_->GetContentsView();
    std::unique_ptr<NativeViewHost> host = std::make_unique<NativeViewHost>();
    NativeViewHost* host_ptr = host.get();
    if (index.has_value()) {
      contents_view->AddChildViewAt(std::move(host), index.value());
    } else {
      contents_view->AddChildView(std::move(host));
    }
    host_ptr->Attach(native_view);
    return host_ptr;
  }

  // Creates a View with a layer and appends it to the contents view.
  View* AddViewWithLayer(const std::string& name) {
    auto* contents_view = parent_widget_->GetContentsView();
    auto* view = contents_view->AddChildView(std::make_unique<View>());
    view->SetPaintToLayer();
    view->layer()->SetName(name);
    return view;
  }

 private:
  void StackWindow(aura::Window* window) {
    if (last_stacked_window_) {
      parent_window_->StackChildAbove(window, last_stacked_window_);
    } else {
      parent_window_->StackChildAtBottom(window);
    }
    last_stacked_window_ = window;
  }

  raw_ptr<WindowReordererTest> test_;
  raw_ptr<Widget> parent_widget_;
  raw_ptr<aura::Window> parent_window_;
  raw_ptr<aura::Window> last_stacked_window_ = nullptr;
  std::vector<std::unique_ptr<Widget>> widgets_;
};

}  // namespace

// Test that views with layers and views with hosted native views
// (NativeViewHost) are reordered according to the view hierarchy.
//
// View hierarchy:
// contents_view
// ├── host_view1 (hosts w1)
// ├── v (paint to layer)
// └── host_view2 (hosts w2)
//
// Initial Window stack (bottom to top):
//  w1, w2
// Initial Layer stack:
//  w1, v, w2
//
// Reorder 1: Move host_view1 to top
// View hierarchy: contents_view -> [v, host_view2, host_view1]
// Window stack: w2, w1
// Layer stack: v, w2, w1
//
// Reorder 2: Move host_view2 to top
// View hierarchy: contents_view -> [v, host_view1, host_view2]
// Window stack: w1, w2
// Layer stack: v, w1, w2
TEST_F(WindowReordererTest, Basic) {
  std::unique_ptr<Widget> parent = CreateControlWidget(root_window());
  parent->Show();
  aura::Window* parent_window = parent->GetNativeWindow();
  View* contents_view = parent->SetContentsView(std::make_unique<View>());

  TreeBuilder builder(this, parent.get());

  // Create a view with a layer.
  builder.AddViewWithLayer("v");

  // 1) Create child widgets to be hosted. Pass name to CreateControlWidget.
  std::unique_ptr<Widget> w1 = CreateControlWidget(parent_window, "w1");
  w1->Show();
  std::unique_ptr<Widget> w2 = CreateControlWidget(parent_window, "w2");
  w2->Show();

  // Initially they are just child windows, not hosted yet.
  EXPECT_EQ("w1 w2", ChildWindowNamesAsString(*parent_window));

  // Host w2 in host_view2 (added after v, so append).
  auto* host_view2 = builder.AddNativeViewHost(w2->GetNativeView());

  // Host w1 in host_view1 (added before v, so index 0).
  auto* host_view1 = builder.AddNativeViewHost(w1->GetNativeView(), 0);

  // Verify initial hosted order.
  EXPECT_EQ("w1 w2", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("w1 v w2", FlattenedChildLayerNames(parent_window->layer()));

  // 2) Test the z-order as a result of reordering.
  contents_view->ReorderChildView(host_view1, contents_view->children().size());
  EXPECT_EQ("w2 w1", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("v w2 w1", FlattenedChildLayerNames(parent_window->layer()));

  contents_view->ReorderChildView(host_view2, contents_view->children().size());
  EXPECT_EQ("w1 w2", ChildWindowNamesAsString(*parent_window));
  EXPECT_EQ("v w1 w2", FlattenedChildLayerNames(parent_window->layer()));

  // 3) Test adding a new NativeViewHost + native view.
  // Case 3a: Add below two (at the bottom).
  {
    std::unique_ptr<Widget> w3 = CreateControlWidget(parent_window, "w3");
    w3->Show();
    auto* host_view3 = builder.AddNativeViewHost(w3->GetNativeView(), 0);

    EXPECT_EQ("w3 w1 w2", ChildWindowNamesAsString(*parent_window));
    EXPECT_EQ("w3 v w1 w2", FlattenedChildLayerNames(parent_window->layer()));

    contents_view->RemoveChildViewT(host_view3);
  }
  EXPECT_EQ("w1 w2", ChildWindowNamesAsString(*parent_window));

  // Case 3b: Add in the middle.
  {
    std::unique_ptr<Widget> w3 = CreateControlWidget(parent_window, "w3");
    w3->Show();
    auto* host_view3 = builder.AddNativeViewHost(w3->GetNativeView(), 2);

    EXPECT_EQ("w1 w3 w2", ChildWindowNamesAsString(*parent_window));
    EXPECT_EQ("v w1 w3 w2", FlattenedChildLayerNames(parent_window->layer()));

    contents_view->RemoveChildViewT(host_view3);
  }
  EXPECT_EQ("w1 w2", ChildWindowNamesAsString(*parent_window));

  // Case 3c: Add above two (at the top).
  {
    std::unique_ptr<Widget> w3 = CreateControlWidget(parent_window, "w3");
    w3->Show();
    auto* host_view3 = builder.AddNativeViewHost(w3->GetNativeView());

    EXPECT_EQ("w1 w2 w3", ChildWindowNamesAsString(*parent_window));
    EXPECT_EQ("v w1 w2 w3", FlattenedChildLayerNames(parent_window->layer()));

    contents_view->RemoveChildViewT(host_view3);
  }
  EXPECT_EQ("w1 w2", ChildWindowNamesAsString(*parent_window));
}

// Test that unassociated windows (windows not hosted by NativeViewHost)
// preserve their relative positions during reordering.
//
// Initial Window stack (bottom to top):
//  [top]
//   u3         <-- Unassociated
//   w_assoc2   <-- Associated
//   u2         <-- Unassociated
//   u1         <-- Unassociated
//   w_assoc1   <-- Associated
//  [bottom]
//
// Reorder: Swap host1 and host2 (visual order: host2, host1)
// Expected Window stack:
//  [top]
//   u3
//   w_assoc1   <-- Swapped
//   u2
//   u1
//   w_assoc2   <-- Swapped
//  [bottom]
TEST_F(WindowReordererTest, UnassociatedWindows) {
  std::unique_ptr<Widget> parent = CreateControlWidget(root_window());
  parent->Show();
  aura::Window* parent_window = parent->GetNativeWindow();

  View* contents_view = parent->SetContentsView(std::make_unique<View>());

  TreeBuilder builder(this, parent.get());
  auto* host1 = builder.AddNativeViewHost("w_assoc1");
  builder.AddUnassociatedWindow("u1");
  builder.AddUnassociatedWindow("u2");
  auto* host2 = builder.AddNativeViewHost("w_assoc2");
  builder.AddUnassociatedWindow("u3");

  ASSERT_EQ("w_assoc1 u1 u2 w_assoc2 u3",
            ChildWindowNamesAsString(*parent_window));

  // Reorder: swap host1 and host2. Visual: host2, host1.
  // Expected: w_assoc2, u1, u2, w_assoc1, u3
  contents_view->ReorderChildView(host2, 0);
  EXPECT_EQ("w_assoc2 u1 u2 w_assoc1 u3",
            ChildWindowNamesAsString(*parent_window));

  // Swap back.
  contents_view->ReorderChildView(host1, 0);
  EXPECT_EQ("w_assoc1 u1 u2 w_assoc2 u3",
            ChildWindowNamesAsString(*parent_window));
}

// Test that three associated windows keep their correct order and update
// correctly when reordered, while unassociated windows preserve their relative
// positions.
//
// Initial Window stack (bottom to top) for all permutations:
//  [top]
//   u3         <-- Unassociated (above range)
//   w_assoc3   <-- Associated
//   u2         <-- Unassociated
//   w_assoc2   <-- Associated
//   u1         <-- Unassociated
//   w_assoc1   <-- Associated
//  [bottom]
//
// Permutations and expected window stacks:
//
// 1) w_assoc1, w_assoc2, w_assoc3 (No change)
//  [top] u3, w_assoc3, u2, w_assoc2, u1, w_assoc1 [bottom]
//
// 2) w_assoc1, w_assoc3, w_assoc2
//  [top] u3, w_assoc2, w_assoc3, u2, u1, w_assoc1 [bottom]
//
// 3) w_assoc2, w_assoc1, w_assoc3
//  [top] u3, w_assoc3, u2, u1, w_assoc1, w_assoc2 [bottom]
//
// 4) w_assoc2, w_assoc3, w_assoc1
//  [top] u3, w_assoc1, w_assoc3, u2, u1, w_assoc2 [bottom]
//
// 5) w_assoc3, w_assoc1, w_assoc2
//  [top] u3, w_assoc2, u2, u1, w_assoc1, w_assoc3 [bottom]
//
// 6) w_assoc3, w_assoc2, w_assoc1
//  [top] u3, w_assoc1, u2, u1, w_assoc2, w_assoc3 [bottom]
TEST_F(WindowReordererTest, ThreeAssociatedWithUnassociated) {
  std::unique_ptr<Widget> parent = CreateControlWidget(root_window());
  parent->Show();
  aura::Window* parent_window = parent->GetNativeWindow();

  View* contents_view = parent->SetContentsView(std::make_unique<View>());

  TreeBuilder builder(this, parent.get());
  auto* host1 = builder.AddNativeViewHost("w_assoc1");
  aura::Window* u1 = builder.AddUnassociatedWindow("u1");
  auto* host2 = builder.AddNativeViewHost("w_assoc2");
  aura::Window* u2 = builder.AddUnassociatedWindow("u2");
  auto* host3 = builder.AddNativeViewHost("w_assoc3");
  aura::Window* u3 = builder.AddUnassociatedWindow("u3");

  // Helper to reset tree to: w_assoc1, u1, w_assoc2, u2, w_assoc3, u3
  auto reset_tree = [&]() {
    StackViews(contents_view, {host1, host2, host3});
    StackWindows(parent_window, {host1->native_view(), u1, host2->native_view(),
                                 u2, host3->native_view(), u3});
    ASSERT_EQ("w_assoc1 u1 w_assoc2 u2 w_assoc3 u3",
              ChildWindowNamesAsString(*parent_window));
  };

  // Permutation 1: w_assoc1, w_assoc2, w_assoc3 (No change)
  reset_tree();
  // Visual order: host1, host2, host3. Already in this order.
  StackViews(contents_view, {host1, host2, host3});
  EXPECT_EQ("w_assoc1 u1 w_assoc2 u2 w_assoc3 u3",
            ChildWindowNamesAsString(*parent_window));

  // Permutation 2: w_assoc1, w_assoc3, w_assoc2
  reset_tree();
  // Visual: host1, host3, host2
  StackViews(contents_view, {host1, host3, host2});
  EXPECT_EQ("w_assoc1 u1 u2 w_assoc3 w_assoc2 u3",
            ChildWindowNamesAsString(*parent_window));

  // Permutation 3: w_assoc2, w_assoc1, w_assoc3
  reset_tree();
  // Visual: host2, host1, host3
  StackViews(contents_view, {host2, host1, host3});
  EXPECT_EQ("w_assoc2 w_assoc1 u1 u2 w_assoc3 u3",
            ChildWindowNamesAsString(*parent_window));

  // Permutation 4: w_assoc2, w_assoc3, w_assoc1
  reset_tree();
  // Visual: host2, host3, host1
  StackViews(contents_view, {host2, host3, host1});
  EXPECT_EQ("w_assoc2 u1 u2 w_assoc3 w_assoc1 u3",
            ChildWindowNamesAsString(*parent_window));

  // Permutation 5: w_assoc3, w_assoc1, w_assoc2
  reset_tree();
  // Visual: host3, host1, host2
  StackViews(contents_view, {host3, host1, host2});
  EXPECT_EQ("w_assoc3 w_assoc1 u1 u2 w_assoc2 u3",
            ChildWindowNamesAsString(*parent_window));

  // Permutation 6: w_assoc3, w_assoc2, w_assoc1
  reset_tree();
  // Visual: host3, host2, host1
  StackViews(contents_view, {host3, host2, host1});
  EXPECT_EQ("w_assoc3 u1 u2 w_assoc2 w_assoc1 u3",
            ChildWindowNamesAsString(*parent_window));
}

// Test that reordering a subset of windows works correctly in a larger tree
// with multiple associated and unassociated windows.
//
// Initial Window stack (bottom to top):
//  [top]
//   u4         <-- Unassociated
//   w4         <-- Associated
//   u3         <-- Unassociated
//   w3         <-- Associated
//   u2         <-- Unassociated
//   w2         <-- Associated
//   u1         <-- Unassociated
//   w1         <-- Associated
//  [bottom]
//
// View order: host1, host2, host3, host4
// (associated order in window stack matches: w1, w2, w3, w4)
TEST_F(WindowReordererTest, FourAssociatedWithUnassociated) {
  std::unique_ptr<Widget> parent = CreateControlWidget(root_window());
  parent->Show();
  aura::Window* parent_window = parent->GetNativeWindow();

  View* contents_view = parent->SetContentsView(std::make_unique<View>());

  TreeBuilder builder(this, parent.get());
  auto* host1 = builder.AddNativeViewHost("w1");
  aura::Window* u1 = builder.AddUnassociatedWindow("u1");
  auto* host2 = builder.AddNativeViewHost("w2");
  aura::Window* u2 = builder.AddUnassociatedWindow("u2");
  auto* host3 = builder.AddNativeViewHost("w3");
  aura::Window* u3 = builder.AddUnassociatedWindow("u3");
  auto* host4 = builder.AddNativeViewHost("w4");
  aura::Window* u4 = builder.AddUnassociatedWindow("u4");

  auto reset_tree = [&]() {
    // Start with view order: w1, w2, w3, w4
    StackViews(contents_view, {host1, host2, host3, host4});

    // Stack windows to match view order: w1, u1, w2, u2, w3, u3, w4, u4
    StackWindows(parent_window,
                 {host1->native_view(), u1, host2->native_view(), u2,
                  host3->native_view(), u3, host4->native_view(), u4});
    ASSERT_EQ("w1 u1 w2 u2 w3 u3 w4 u4",
              ChildWindowNamesAsString(*parent_window));
  };

  // Reorder to: w1, w3, w2, w4
  reset_tree();

  // Move w3 to index 1. This should trigger only one reorder.
  // View order becomes: w1, w3, w2, w4.
  contents_view->ReorderChildView(host3, 1);

  EXPECT_EQ("w1 u1 w3 w2 u2 u3 w4 u4",
            ChildWindowNamesAsString(*parent_window));

  // Swap w1 and w4
  reset_tree();
  // Visual: host4, host2, host3, host1
  StackViews(contents_view, {host4, host2, host3, host1});
  EXPECT_EQ("w4 w2 u1 u2 u3 w3 w1 u4",
            ChildWindowNamesAsString(*parent_window));
  // Remove w3
  reset_tree();

  // host3 is deleted after this call.
  contents_view->RemoveChildViewT(host3);
  EXPECT_EQ("w1 u1 w2 u2 u3 w4 u4", ChildWindowNamesAsString(*parent_window));

  // then remove w1 (without reset)
  // Remove w1
  // host1 is deleted after this call.
  contents_view->RemoveChildViewT(host1);
  EXPECT_EQ("u1 w2 u2 u3 w4 u4", ChildWindowNamesAsString(*parent_window));

  // Delete u2 (which also removes it from parent)
  delete u2;
  EXPECT_EQ("u1 w2 u3 w4 u4", ChildWindowNamesAsString(*parent_window));

  // Delete u1
  delete u1;
  EXPECT_EQ("w2 u3 w4 u4", ChildWindowNamesAsString(*parent_window));
}

// Test that detaching all native views and re-attaching them restores the
// correct order when there are no unassociated windows between them.
TEST_F(WindowReordererTest, DetachReattach) {
  std::unique_ptr<Widget> parent = CreateControlWidget(root_window());
  parent->Show();
  aura::Window* parent_window = parent->GetNativeWindow();

  parent->SetContentsView(std::make_unique<View>());

  TreeBuilder builder(this, parent.get());
  auto* host1 = builder.AddNativeViewHost("w_assoc1");
  auto* host2 = builder.AddNativeViewHost("w_assoc2");
  builder.AddUnassociatedWindow("u1");

  ASSERT_EQ("w_assoc1 w_assoc2 u1", ChildWindowNamesAsString(*parent_window));

  aura::Window* w1 = host1->native_view();
  aura::Window* w2 = host2->native_view();

  // Detach both.
  host1->Detach();
  host2->Detach();
  EXPECT_EQ("u1", ChildWindowNamesAsString(*parent_window));

  // Re-attach w1 first.
  host1->Attach(w1);
  EXPECT_EQ("w_assoc1 u1", ChildWindowNamesAsString(*parent_window));

  // Re-attach w2.
  host2->Attach(w2);
  EXPECT_EQ("w_assoc1 w_assoc2 u1", ChildWindowNamesAsString(*parent_window));

  // Detach both again.
  host1->Detach();
  host2->Detach();
  EXPECT_EQ("u1", ChildWindowNamesAsString(*parent_window));

  // Re-attach w2 first (reverse order).
  host2->Attach(w2);
  EXPECT_EQ("w_assoc2 u1", ChildWindowNamesAsString(*parent_window));

  // Re-attach w1.
  host1->Attach(w1);
  EXPECT_EQ("w_assoc1 w_assoc2 u1", ChildWindowNamesAsString(*parent_window));
}

}  // namespace views
