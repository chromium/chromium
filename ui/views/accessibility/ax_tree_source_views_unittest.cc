// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_tree_source_views.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

// TestAXTreeSourceViews provides a root with a default tree ID.
class TestAXTreeSourceViews : public AXTreeSourceViews {
 public:
  TestAXTreeSourceViews(AXAuraObjWrapper* root, AXAuraObjCache* cache)
      : AXTreeSourceViews(root, ui::AXTreeID::CreateNewAXTreeID(), cache) {}
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
    Widget::InitParams params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(11, 22, 333, 444);
    params.context = GetContext();
    widget_->Init(std::move(params));
    widget_->SetContentsView(new View());

    label1_ = new Label(base::ASCIIToUTF16("Label 1"));
    label1_->SetBounds(1, 1, 111, 111);
    widget_->GetContentsView()->AddChildView(label1_);

    label2_ = new Label(base::ASCIIToUTF16("Label 2"));
    label2_->SetBounds(2, 2, 222, 222);
    widget_->GetContentsView()->AddChildView(label2_);

    textfield_ = new Textfield();
    textfield_->SetBounds(222, 2, 20, 200);
    widget_->GetContentsView()->AddChildView(textfield_);
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  std::unique_ptr<Widget> widget_;
  Label* label1_ = nullptr;         // Owned by views hierarchy.
  Label* label2_ = nullptr;         // Owned by views hierarchy.
  Textfield* textfield_ = nullptr;  // Owned by views hierarchy.
};

TEST_F(AXTreeSourceViewsTest, Basics) {
  AXAuraObjCache cache;

  // Start the tree at the Widget's contents view.
  AXAuraObjWrapper* root = cache.GetOrCreate(widget_->GetContentsView());
  TestAXTreeSourceViews tree(root, &cache);
  EXPECT_EQ(root, tree.GetRoot());

  // The root has no parent.
  EXPECT_FALSE(tree.GetParent(root));

  // The root has the right children.
  std::vector<AXAuraObjWrapper*> children;
  tree.GetChildren(root, &children);
  ASSERT_EQ(3u, children.size());

  // The labels are the children.
  AXAuraObjWrapper* label1 = children[0];
  AXAuraObjWrapper* label2 = children[1];
  AXAuraObjWrapper* textfield = children[2];
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
  EXPECT_TRUE(tree.IsValid(root));
  EXPECT_FALSE(tree.IsValid(nullptr));

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
  TestAXTreeSourceViews tree(cache.GetOrCreate(widget_.get()), &cache);
  textfield_->RequestFocus();

  ui::AXTreeData tree_data;
  tree.GetTreeData(&tree_data);
  EXPECT_TRUE(tree_data.loaded);
  EXPECT_EQ(cache.GetID(textfield_), tree_data.focus_id);
}

TEST_F(AXTreeSourceViewsTest, IgnoredView) {
  View* ignored_view = new View();
  ignored_view->GetViewAccessibility().OverrideIsIgnored(true);
  widget_->GetContentsView()->AddChildView(ignored_view);

  AXAuraObjCache cache;
  TestAXTreeSourceViews tree(cache.GetOrCreate(widget_.get()), &cache);
  EXPECT_TRUE(tree.IsValid(cache.GetOrCreate(ignored_view)));
}

}  // namespace
}  // namespace views
