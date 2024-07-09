// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_tree_source_views.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

// TestAXTreeSourceViews provides a root with a default tree ID.
class TestAXTreeSourceViews : public AXTreeSourceViews {
 public:
  TestAXTreeSourceViews(ui::AXNodeID root_id, AXAuraObjCache* cache)
      : AXTreeSourceViews(root_id, ui::AXTreeID::CreateNewAXTreeID(), cache) {}
  TestAXTreeSourceViews(const TestAXTreeSourceViews&) = delete;
  TestAXTreeSourceViews& operator=(const TestAXTreeSourceViews&) = delete;
  ~TestAXTreeSourceViews() override = default;
};

class AXTreeSourceViewsTest : public ViewsTestBase {
 public:
  AXTreeSourceViewsTest() = default;
  AXTreeSourceViewsTest(const AXTreeSourceViewsTest&) = delete;
  AXTreeSourceViewsTest& operator=(const AXTreeSourceViewsTest&) = delete;
  ~AXTreeSourceViewsTest() override = default;

  // testing::Test:
  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                              Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(11, 22, 333, 444);
    params.context = GetContext();
    widget_->Init(std::move(params));
    widget_->SetContentsView(std::make_unique<View>());

    label1_ = new Label(u"Label 1");
    label1_->SetBounds(1, 1, 111, 111);
    widget_->GetContentsView()->AddChildView(label1_.get());

    label2_ = new Label(u"Label 2");
    label2_->SetBounds(2, 2, 222, 222);
    widget_->GetContentsView()->AddChildView(label2_.get());

    textfield_ = new Textfield();
    textfield_->SetBounds(222, 2, 20, 200);
    widget_->GetContentsView()->AddChildView(textfield_.get());
  }

  void TearDown() override {
    label1_ = nullptr;
    label2_ = nullptr;
    textfield_ = nullptr;
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  std::unique_ptr<Widget> widget_;
  raw_ptr<Label, AcrossTasksDanglingUntriaged> label1_ = nullptr;
  raw_ptr<Label, AcrossTasksDanglingUntriaged> label2_ = nullptr;
  raw_ptr<Textfield, AcrossTasksDanglingUntriaged> textfield_ = nullptr;
};

TEST_F(AXTreeSourceViewsTest, Basics) {
  AXAuraObjCache cache;

  // Start the tree at the Widget's contents view.
  AXAuraObjWrapper* root = cache.GetOrCreate(widget_->GetContentsView());
  TestAXTreeSourceViews tree(root->GetUniqueId(), &cache);
  EXPECT_EQ(root, tree.GetRoot());

  // The root has no parent.
  EXPECT_FALSE(tree.GetParent(root));

  // The root has the right children.
  tree.CacheChildrenIfNeeded(root);
  ASSERT_EQ(3u, tree.GetChildCount(root));

  // The labels are the children.
  AXAuraObjWrapper* label1 = tree.ChildAt(root, 0);
  AXAuraObjWrapper* label2 = tree.ChildAt(root, 1);
  AXAuraObjWrapper* textfield = tree.ChildAt(root, 2);
  EXPECT_EQ(label1, cache.GetOrCreate(label1_));
  EXPECT_EQ(label2, cache.GetOrCreate(label2_));
  EXPECT_EQ(textfield, cache.GetOrCreate(textfield_));

  // The parents is correct.
  EXPECT_EQ(root, tree.GetParent(label1));
  EXPECT_EQ(root, tree.GetParent(label2));
  EXPECT_EQ(root, tree.GetParent(textfield));

  // IDs match the ones in the cache.
  EXPECT_EQ(root->GetUniqueId(), tree.GetId(root));
  EXPECT_EQ(label1->GetUniqueId(), tree.GetId(label1));
  EXPECT_EQ(label2->GetUniqueId(), tree.GetId(label2));
  EXPECT_EQ(textfield->GetUniqueId(), tree.GetId(textfield));

  // Reverse ID lookups work.
  EXPECT_EQ(root, tree.GetFromId(root->GetUniqueId()));
  EXPECT_EQ(label1, tree.GetFromId(label1->GetUniqueId()));
  EXPECT_EQ(label2, tree.GetFromId(label2->GetUniqueId()));
  EXPECT_EQ(textfield, tree.GetFromId(textfield->GetUniqueId()));

  // Validity.
  EXPECT_TRUE(root != nullptr);

  // Comparisons.
  EXPECT_TRUE(tree.IsEqual(label1, label1));
  EXPECT_FALSE(tree.IsEqual(label1, label2));
  EXPECT_FALSE(tree.IsEqual(label1, nullptr));
  EXPECT_FALSE(tree.IsEqual(nullptr, label1));

  // Null pointers is the null value.
  EXPECT_EQ(nullptr, tree.GetNull());
}

TEST_F(AXTreeSourceViewsTest, GetTreeDataWithFocus) {
  AXAuraObjCache cache;
  TestAXTreeSourceViews tree(cache.GetOrCreate(widget_.get())->GetUniqueId(),
                             &cache);
  textfield_->RequestFocus();

  ui::AXTreeData tree_data;
  tree.GetTreeData(&tree_data);
  EXPECT_TRUE(tree_data.loaded);
  EXPECT_EQ(cache.GetID(textfield_), tree_data.focus_id);
}

TEST_F(AXTreeSourceViewsTest, IgnoredView) {
  View* ignored_view = new View();
  ignored_view->GetViewAccessibility().SetIsIgnored(true);
  widget_->GetContentsView()->AddChildView(ignored_view);

  AXAuraObjCache cache;
  TestAXTreeSourceViews tree(cache.GetOrCreate(widget_.get())->GetUniqueId(),
                             &cache);
  EXPECT_TRUE(cache.GetOrCreate(ignored_view) != nullptr);
}

TEST_F(AXTreeSourceViewsTest, ViewWithChildTreeHasNoChildren) {
  View* contents_view = widget_->GetContentsView();
  contents_view->GetViewAccessibility().SetChildTreeID(
      ui::AXTreeID::CreateNewAXTreeID());

  AXAuraObjCache cache;
  TestAXTreeSourceViews tree(cache.GetOrCreate(widget_.get())->GetUniqueId(),
                             &cache);
  auto* ax_obj = cache.GetOrCreate(contents_view);
  EXPECT_TRUE(ax_obj != nullptr);
  tree.CacheChildrenIfNeeded(ax_obj);
  EXPECT_EQ(0u, tree.GetChildCount(ax_obj));
  EXPECT_EQ(nullptr, cache.GetOrCreate(textfield_)->GetParent());
}

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
class AXTreeSourceViewsDesktopWidgetTest : public AXTreeSourceViewsTest {
 public:
  AXTreeSourceViewsDesktopWidgetTest() {
    set_native_widget_type(ViewsTestBase::NativeWidgetType::kDesktop);
  }
};

// Tests that no use-after-free when a focused child window is destroyed in
// desktop aura widget.
TEST_F(AXTreeSourceViewsDesktopWidgetTest, FocusedChildWindowDestroyed) {
  AXAuraObjCache cache;
  AXAuraObjWrapper* root_wrapper =
      cache.GetOrCreate(widget_->GetNativeWindow()->GetRootWindow());
  EXPECT_NE(nullptr, root_wrapper);

  aura::test::TestWindowDelegate child_delegate;
  aura::Window* child = new aura::Window(&child_delegate);
  child->Init(ui::LAYER_NOT_DRAWN);
  widget_->GetNativeView()->AddChild(child);
  aura::client::GetFocusClient(widget_->GetNativeView())->FocusWindow(child);

  AXAuraObjWrapper* child_wrapper = cache.GetOrCreate(child);
  EXPECT_NE(nullptr, child_wrapper);

  // GetFocus() reflects the focused child window.
  EXPECT_NE(nullptr, cache.GetFocus());

  test::WidgetDestroyedWaiter waiter(widget_.get());

  // Close the widget to destroy the child.
  widget_->CloseNow();

  // Wait for the async widget close.
  waiter.Wait();

  // GetFocus() should return null and no use-after-free to call it.
  EXPECT_EQ(nullptr, cache.GetFocus());
}
#endif  // defined(USE_AURA)

}  // namespace
}  // namespace views
