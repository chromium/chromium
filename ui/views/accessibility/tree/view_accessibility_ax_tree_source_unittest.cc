// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/tree/view_accessibility_ax_tree_source.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/accessibility/tree/view_accessibility_ax_tree_source_test_api.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace views::test {

class ViewAccessibilityAXTreeSourceTest : public testing::Test {
 protected:
  ViewAccessibilityAXTreeSourceTest() = default;
  ~ViewAccessibilityAXTreeSourceTest() override = default;

  void SetUp() override {
    root_view_ = std::make_unique<View>();
    cache_ = std::make_unique<WidgetViewAXCache>();
    cache_->Insert(&root_view_->GetViewAccessibility());
    source_ = std::make_unique<ViewAccessibilityAXTreeSource>(
        root_id(), ui::AXTreeID::CreateNewAXTreeID(), cache_.get());
    test_api_ =
        std::make_unique<ViewAccessibilityAXTreeSourceTestApi>(source_.get());
  }

  ui::AXNodeID root_id() const {
    return root_view_->GetViewAccessibility().GetUniqueId();
  }
  ViewAccessibilityAXTreeSource* source() const { return source_.get(); }
  ViewAccessibilityAXTreeSourceTestApi& test_api() const { return *test_api_; }

 private:
  std::unique_ptr<View> root_view_;
  std::unique_ptr<WidgetViewAXCache> cache_;
  std::unique_ptr<ViewAccessibilityAXTreeSource> source_;
  std::unique_ptr<ViewAccessibilityAXTreeSourceTestApi> test_api_;

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAccessibilityTreeForViews};
};

TEST_F(ViewAccessibilityAXTreeSourceTest, CacheInsertGetRemove) {
  auto v = std::make_unique<View>();

  EXPECT_EQ(test_api().cache().Get(v->GetViewAccessibility().GetUniqueId()),
            nullptr);

  test_api().cache().Insert(&v->GetViewAccessibility());
  EXPECT_EQ(test_api().cache().Get(v->GetViewAccessibility().GetUniqueId()),
            &v->GetViewAccessibility());

  test_api().cache().Remove(v->GetViewAccessibility().GetUniqueId());
  EXPECT_EQ(test_api().cache().Get(v->GetViewAccessibility().GetUniqueId()),
            nullptr);
  EXPECT_FALSE(
      test_api().cache().HasCachedChildren(&v->GetViewAccessibility()));
}

// This test validates that CacheChildrenIfNeeded properly caches the children
// and not the grandchildren, and that it accurately track the "cached-children"
// state.
TEST_F(ViewAccessibilityAXTreeSourceTest, CacheChildrenIfNeeded) {
  auto v = std::make_unique<View>();
  auto* child = v->AddChildView(std::make_unique<View>());
  auto* grandchild = child->AddChildView(std::make_unique<View>());

  EXPECT_FALSE(
      test_api().cache().HasCachedChildren(&v->GetViewAccessibility()));
  EXPECT_FALSE(
      test_api().cache().HasCachedChildren(&child->GetViewAccessibility()));
  EXPECT_EQ(test_api().cache().Get(v->GetViewAccessibility().GetUniqueId()),
            nullptr);
  EXPECT_EQ(test_api().cache().Get(child->GetViewAccessibility().GetUniqueId()),
            nullptr);
  EXPECT_EQ(
      test_api().cache().Get(grandchild->GetViewAccessibility().GetUniqueId()),
      nullptr);

  test_api().cache().CacheChildrenIfNeeded(&v->GetViewAccessibility());
  EXPECT_TRUE(test_api().cache().HasCachedChildren(&v->GetViewAccessibility()));
  EXPECT_FALSE(
      test_api().cache().HasCachedChildren(&child->GetViewAccessibility()));
  EXPECT_EQ(test_api().cache().Get(child->GetViewAccessibility().GetUniqueId()),
            &child->GetViewAccessibility());
  EXPECT_EQ(
      test_api().cache().Get(grandchild->GetViewAccessibility().GetUniqueId()),
      nullptr);

  test_api().cache().RemoveFromChildCache(&v->GetViewAccessibility());
  EXPECT_FALSE(
      test_api().cache().HasCachedChildren(&v->GetViewAccessibility()));

  // While we want to remove the "cached-children" mark, we still want to keep
  // the nodes in the cache until they're explicitly removed.
  EXPECT_EQ(test_api().cache().Get(child->GetViewAccessibility().GetUniqueId()),
            &child->GetViewAccessibility());
}

TEST_F(ViewAccessibilityAXTreeSourceTest, GetRoot) {
  ASSERT_EQ(test_api().root_id(), root_id());

  EXPECT_EQ(source()->GetRoot()->GetUniqueId(), root_id());
}

TEST_F(ViewAccessibilityAXTreeSourceTest, GetFromId) {
  ASSERT_EQ(test_api().root_id(), root_id());

  EXPECT_EQ(source()->GetFromId(root_id())->GetUniqueId(), root_id());
}

TEST_F(ViewAccessibilityAXTreeSourceTest, GetTreeData_PopulatesTreeData) {
  ui::AXTreeData data;
  EXPECT_TRUE(source()->GetTreeData(&data));
  EXPECT_EQ(data.tree_id, test_api().tree_id());
  EXPECT_TRUE(data.loaded);
  EXPECT_DOUBLE_EQ(data.loading_progress, 1.0);
}

TEST_F(ViewAccessibilityAXTreeSourceTest, GetChildCount_RealChildren) {
  auto v = std::make_unique<View>();
  v->AddChildView(std::make_unique<View>());
  v->AddChildView(std::make_unique<View>());
  EXPECT_EQ(source()->GetChildCount(&v->GetViewAccessibility()), 2u);
}

TEST_F(ViewAccessibilityAXTreeSourceTest, GetChildCount_VirtualChildren) {
  auto v = std::make_unique<View>();
  auto& v_ax = v->GetViewAccessibility();
  v_ax.AddVirtualChildView(std::make_unique<AXVirtualView>());
  v_ax.AddVirtualChildView(std::make_unique<AXVirtualView>());
  EXPECT_EQ(source()->GetChildCount(&v_ax), 2u);
}

TEST_F(ViewAccessibilityAXTreeSourceTest, GetChildCount_MixedChildren) {
  auto v = std::make_unique<View>();
  v->AddChildView(std::make_unique<View>());
  auto& v_ax = v->GetViewAccessibility();
  v_ax.AddVirtualChildView(std::make_unique<AXVirtualView>());
  EXPECT_EQ(source()->GetChildCount(&v_ax), 1u);
}

TEST_F(ViewAccessibilityAXTreeSourceTest, GetChildAt_ValidAndInvalidIndices) {
  auto v = std::make_unique<View>();
  v->AddChildView(std::make_unique<View>());
  auto& v_ax = v->GetViewAccessibility();
  auto children = v_ax.GetChildren();
  ASSERT_EQ(children.size(), 1u);
  EXPECT_EQ(source()->ChildAt(&v_ax, 0), children[0]);
  EXPECT_EQ(source()->ChildAt(&v_ax, children.size()), nullptr);
}

TEST_F(ViewAccessibilityAXTreeSourceTest, GetParent_RootReturnsNull) {
  auto* root_ax = source()->GetRoot();
  EXPECT_EQ(source()->GetParent(root_ax), nullptr);
}

TEST_F(ViewAccessibilityAXTreeSourceTest, GetParent_ImmediateUnignoredParent) {
  auto parent = std::make_unique<View>();
  auto* child = parent->AddChildView(std::make_unique<View>());
  EXPECT_EQ(source()->GetParent(&child->GetViewAccessibility()),
            &parent->GetViewAccessibility());
}

TEST_F(ViewAccessibilityAXTreeSourceTest, GetParent_SkipIgnoredParent) {
  auto grand = std::make_unique<View>();
  auto* parent = grand->AddChildView(std::make_unique<View>());
  auto* child = parent->AddChildView(std::make_unique<View>());
  parent->GetViewAccessibility().SetIsIgnored(true);
  EXPECT_EQ(source()->GetParent(&child->GetViewAccessibility()),
            &grand->GetViewAccessibility());
}

}  // namespace views::test
