// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/tree/view_accessibility_ax_tree_source.h"

#include <memory>
#include <utility>

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"
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

class ActionTestView : public View {
 public:
  ActionTestView() = default;
  ~ActionTestView() override = default;

  bool HandleAccessibleAction(const ui::AXActionData& action) override {
    last_action_id_ = action.target_node_id;
    return true;
  }

  ui::AXNodeID last_action_id() const { return last_action_id_; }

 private:
  ui::AXNodeID last_action_id_ = ui::kInvalidAXNodeID;
};

class ActionTestVirtualView : public AXVirtualView {
 public:
  ActionTestVirtualView() = default;
  ~ActionTestVirtualView() override = default;

  bool HandleAccessibleAction(const ui::AXActionData& action) override {
    last_action_id_ = action.target_node_id;
    return true;
  }

  ui::AXNodeID last_action_id() const { return last_action_id_; }

 private:
  ui::AXNodeID last_action_id_ = ui::kInvalidAXNodeID;
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
  auto& v_ax = v->GetViewAccessibility();

  source()->CacheChildrenIfNeeded(&v_ax);
  EXPECT_EQ(source()->GetChildCount(&v_ax), 2u);
  source()->ClearChildCache(&v_ax);
}

TEST_F(ViewAccessibilityAXTreeSourceTest, GetChildCount_VirtualChildren) {
  auto v = std::make_unique<View>();
  auto& v_ax = v->GetViewAccessibility();
  v_ax.AddVirtualChildView(std::make_unique<AXVirtualView>());
  v_ax.AddVirtualChildView(std::make_unique<AXVirtualView>());

  source()->CacheChildrenIfNeeded(&v_ax);
  EXPECT_EQ(source()->GetChildCount(&v_ax), 2u);
  source()->ClearChildCache(&v_ax);
}

TEST_F(ViewAccessibilityAXTreeSourceTest, GetChildCount_MixedChildren) {
  auto v = std::make_unique<View>();
  v->AddChildView(std::make_unique<View>());  // real child
  auto& v_ax = v->GetViewAccessibility();
  v_ax.AddVirtualChildView(std::make_unique<AXVirtualView>());  // virtual child

  source()->CacheChildrenIfNeeded(&v_ax);
  // Mixed case: Views behavior prefers real children over virtual; expect 1.
  EXPECT_EQ(source()->GetChildCount(&v_ax), 1u);
  source()->ClearChildCache(&v_ax);
}

TEST_F(ViewAccessibilityAXTreeSourceTest, GetChildAt_ValidAndInvalidIndices) {
  auto v = std::make_unique<View>();
  v->AddChildView(std::make_unique<View>());
  auto& v_ax = v->GetViewAccessibility();

  source()->CacheChildrenIfNeeded(&v_ax);

  const auto children = v_ax.GetChildren();
  ASSERT_EQ(children.size(), 1u);

  EXPECT_EQ(source()->ChildAt(&v_ax, 0), children[0]);
  EXPECT_EQ(source()->ChildAt(&v_ax, children.size()), nullptr);

  source()->ClearChildCache(&v_ax);
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

TEST_F(ViewAccessibilityAXTreeSourceTest, HandleAccessibleAction_OnKnownView) {
  auto v = std::make_unique<ActionTestView>();
  test_api().cache().Insert(&v->GetViewAccessibility());

  ui::AXActionData action;
  action.action = ax::mojom::Action::kFocus;  // Unimportant for the test.
  action.target_node_id = v->GetViewAccessibility().GetUniqueId();

  EXPECT_EQ(v->last_action_id(), ui::kInvalidAXNodeID);
  source()->HandleAccessibleAction(action);
  EXPECT_EQ(v->last_action_id(), action.target_node_id);
}

TEST_F(ViewAccessibilityAXTreeSourceTest,
       HandleAccessibleAction_OnUnknownView) {
  auto v = std::make_unique<ActionTestView>();
  // Don't add the view to the cache so it can't find it.

  ui::AXActionData action;
  action.action = ax::mojom::Action::kFocus;  // Irrelevant for the test.
  action.target_node_id = v->GetViewAccessibility().GetUniqueId();

  EXPECT_EQ(v->last_action_id(), ui::kInvalidAXNodeID);
  source()->HandleAccessibleAction(action);
  EXPECT_EQ(v->last_action_id(), ui::kInvalidAXNodeID);
}

TEST_F(ViewAccessibilityAXTreeSourceTest,
       HandleAccessibleAction_OnKnownVirtualView) {
  auto v = std::make_unique<ActionTestVirtualView>();

  test_api().cache().Insert(v.get());

  ui::AXActionData action;
  action.action = ax::mojom::Action::kFocus;  // Irrelevant for the test.
  action.target_node_id = v->ViewAccessibility::GetUniqueId();

  EXPECT_EQ(v->last_action_id(), ui::kInvalidAXNodeID);
  source()->HandleAccessibleAction(action);
  EXPECT_EQ(v->last_action_id(), action.target_node_id);
}

TEST_F(ViewAccessibilityAXTreeSourceTest,
       HandleAccessibleAction_OnUnknownVirtualView) {
  auto v = std::make_unique<ActionTestVirtualView>();
  // Don't add the virtual view to the cache so it can't find it.

  ui::AXActionData action;
  action.action = ax::mojom::Action::kFocus;
  action.target_node_id = v->ViewAccessibility::GetUniqueId();

  EXPECT_EQ(v->last_action_id(), ui::kInvalidAXNodeID);
  source()->HandleAccessibleAction(action);
  EXPECT_EQ(v->last_action_id(), ui::kInvalidAXNodeID);
}

TEST_F(ViewAccessibilityAXTreeSourceTest,
       HandleAccessibleAction_SetSelection_MismatchedDeath) {
  ui::AXActionData action;
  action.action = ax::mojom::Action::kSetSelection;
  action.anchor_node_id = 1;
  action.focus_node_id = 2;

  // CHECK_EQ(anchor, focus) should fail because the IDs do not match.
  EXPECT_CHECK_DEATH(source()->HandleAccessibleAction(action));
}

TEST_F(ViewAccessibilityAXTreeSourceTest,
       HandleAccessibleAction_SetSelection_UsesAnchorNotTarget) {
  auto target = std::make_unique<ActionTestView>();
  auto anchor = std::make_unique<ActionTestView>();

  test_api().cache().Insert(&target->GetViewAccessibility());
  test_api().cache().Insert(&anchor->GetViewAccessibility());

  // Prepare a SetSelection action where target != anchor == focus.
  ui::AXActionData action;
  action.action = ax::mojom::Action::kSetSelection;
  action.target_node_id = target->GetViewAccessibility().GetUniqueId();
  action.anchor_node_id = anchor->GetViewAccessibility().GetUniqueId();
  action.focus_node_id = anchor->GetViewAccessibility().GetUniqueId();

  EXPECT_EQ(target->last_action_id(), ui::kInvalidAXNodeID);
  EXPECT_EQ(anchor->last_action_id(), ui::kInvalidAXNodeID);

  source()->HandleAccessibleAction(action);

  // The action should be performed on the anchor, not the target. However,
  // because of how the test view is structured, we still expect the last
  // action ID of the anchor to be the target's ID.
  EXPECT_EQ(target->last_action_id(), ui::kInvalidAXNodeID);
  EXPECT_EQ(anchor->last_action_id(), action.target_node_id);
}

TEST_F(ViewAccessibilityAXTreeSourceTest, IsIgnored) {
  EXPECT_FALSE(source()->IsIgnored(nullptr));

  auto v = std::make_unique<View>();
  auto& ax = v->GetViewAccessibility();

  EXPECT_FALSE(source()->IsIgnored(&ax));
  ax.SetIsIgnored(true);
  EXPECT_TRUE(source()->IsIgnored(&ax));
}

TEST_F(ViewAccessibilityAXTreeSourceTest, IsEqual) {
  auto v1 = std::make_unique<View>();
  auto v2 = std::make_unique<View>();
  auto& a1 = v1->GetViewAccessibility();
  auto& a2 = v2->GetViewAccessibility();

  EXPECT_FALSE(source()->IsEqual(nullptr, &a1));
  EXPECT_FALSE(source()->IsEqual(&a1, nullptr));
  EXPECT_FALSE(source()->IsEqual(nullptr, nullptr));

  EXPECT_TRUE(source()->IsEqual(&a1, &a1));

  EXPECT_FALSE(source()->IsEqual(&a1, &a2));
}

TEST_F(ViewAccessibilityAXTreeSourceTest, GetNull) {
  EXPECT_EQ(source()->GetNull(), nullptr);
}

TEST_F(ViewAccessibilityAXTreeSourceTest, ToString_SingleNode) {
  auto v = std::make_unique<View>();
  auto& ax = v->GetViewAccessibility();

  ui::AXNodeData data;
  source()->SerializeNode(&ax, &data);
  std::string prefix = "##";
  std::string expected = prefix + data.ToString() + "\n";

  EXPECT_EQ(source()->ToString(&ax, prefix), expected);
}

TEST_F(ViewAccessibilityAXTreeSourceTest, ToString_TwoLevelTree) {
  auto parent = std::make_unique<View>();
  auto* child = parent->AddChildView(std::make_unique<View>());
  auto& p_ax = parent->GetViewAccessibility();
  auto& c_ax = child->GetViewAccessibility();

  ui::AXNodeData p_data, c_data;
  source()->SerializeNode(&p_ax, &p_data);
  source()->SerializeNode(&c_ax, &c_data);

  std::string prefix = "*";
  std::string want = prefix + p_data.ToString() + "\n" +
                     std::string(2, prefix[0]) + c_data.ToString() + "\n";

  EXPECT_EQ(source()->ToString(&p_ax, prefix), want);
}

TEST_F(ViewAccessibilityAXTreeSourceTest, SerializeNode_NullInputs) {
  ui::AXNodeData data;
  // Null node should not crash.
  source()->SerializeNode(nullptr, &data);

  // Null out_data should not crash.
  auto v = std::make_unique<View>();
  EXPECT_NO_FATAL_FAILURE(
      source()->SerializeNode(&v->GetViewAccessibility(), nullptr));
}

TEST_F(ViewAccessibilityAXTreeSourceTest, SerializeNode_View) {
  auto v = std::make_unique<View>();
  std::u16string name = u"My button";
  v->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  v->GetViewAccessibility().SetName(name);

  ui::AXNodeData data;
  source()->SerializeNode(&v->GetViewAccessibility(), &data);
  EXPECT_EQ(data.role, ax::mojom::Role::kButton);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName), name);
}

TEST_F(ViewAccessibilityAXTreeSourceTest, SerializeNode_AXVirtualView) {
  auto v = std::make_unique<AXVirtualView>();
  std::u16string name = u"My button";
  v->SetRole(ax::mojom::Role::kButton);
  v->SetName(name);

  ui::AXNodeData data;
  source()->SerializeNode(v.get(), &data);
  EXPECT_EQ(data.role, ax::mojom::Role::kButton);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName), name);
}

TEST_F(ViewAccessibilityAXTreeSourceTest,
       GetChildCount_UsesSnapshotNotLiveList) {
  auto parent = std::make_unique<View>();
  auto& pax = parent->GetViewAccessibility();

  // Live: one child initially.
  auto* a = parent->AddChildView(std::make_unique<View>());

  // Make the parent discoverable by the source/cache.
  test_api().cache().Insert(&pax);

  // Take snapshot via the tree-source entry point.
  source()->CacheChildrenIfNeeded(&pax);
  EXPECT_EQ(source()->GetChildCount(&pax), 1u);
  EXPECT_EQ(source()->ChildAt(&pax, 0), &a->GetViewAccessibility());
  EXPECT_EQ(source()->ChildAt(&pax, 1), nullptr);

  // Mutate the live hierarchy after the snapshot.
  auto* b = parent->AddChildView(std::make_unique<View>());

  // Still reading from the cached snapshot (should remain 1).
  EXPECT_EQ(source()->GetChildCount(&pax), 1u);
  EXPECT_EQ(source()->ChildAt(&pax, 0), &a->GetViewAccessibility());
  EXPECT_EQ(source()->ChildAt(&pax, 1), nullptr);

  // Clear the snapshot; then re-cache and verify we see the new live state.
  source()->ClearChildCache(&pax);
  source()->CacheChildrenIfNeeded(&pax);
  EXPECT_EQ(source()->GetChildCount(&pax), 2u);
  // Order must correspond to the live children at re-snapshot time.
  EXPECT_EQ(source()->ChildAt(&pax, 0), &a->GetViewAccessibility());
  EXPECT_EQ(source()->ChildAt(&pax, 1), &b->GetViewAccessibility());
}

}  // namespace views::test
