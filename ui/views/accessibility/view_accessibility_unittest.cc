// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_accessibility.h"

#include "base/test/gtest_util.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/ax_platform_for_test.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/test/views_test_base.h"

namespace views::test {

namespace {
class TestView : public View {
  METADATA_HEADER(TestView, View)

 public:
  TestView() = default;
  TestView(const TestView&) = delete;
  TestView& operator=(const TestView&) = delete;
  ~TestView() override = default;

  // View:
  void OnAccessibilityInitializing(ui::AXNodeData* data) override {
    views::ViewAccessibilityUtils::Merge(/*source*/ lazy_loading_data_,
                                         /*destination*/ *data);
  }

  void OnRoleChanged(ax::mojom::Role role) {
    GetViewAccessibility().SetRole(role);
  }

  void OnNameChanged(ax::mojom::StringAttribute attribute,
                     const std::optional<std::string>& name) {
    // If we don't want any attribute related events to be fired twice (once in
    // the originating View and once in the subscriber View), we can use a
    // scoped blocker on the callback function so that the event is only fired
    // on the originating View.
    ScopedAccessibilityEventBlocker blocker(GetViewAccessibility());
    if (name.has_value()) {
      GetViewAccessibility().SetName(name.value());
    } else {
      GetViewAccessibility().RemoveName();
    }
  }

  void OnClassNameChanged(ax::mojom::StringAttribute attribute,
                          const std::optional<std::string>& class_name) {
    ScopedAccessibilityEventBlocker blocker(GetViewAccessibility());
    if (class_name.has_value()) {
      GetViewAccessibility().SetClassName(class_name.value());
    }
  }

  void OnSelectedChanged(ax::mojom::BoolAttribute attribute,
                         std::optional<bool> selected) {
    if (selected.has_value()) {
      GetViewAccessibility().SetIsSelected(selected.value());
    }
  }

  void OnEditableChanged(ax::mojom::State state, bool editable) {
    GetViewAccessibility().SetIsEditable(editable);
  }

  void OnPosInSetChanged(ax::mojom::IntAttribute attribute,
                         std::optional<int> pos_in_set) {
    if (pos_in_set.has_value()) {
      GetViewAccessibility().SetPosInSet(pos_in_set.value());
    } else {
      GetViewAccessibility().ClearPosInSet();
    }
  }

  void OnCharacterOffsetsChanged(
      ax::mojom::IntListAttribute attribute,
      const std::optional<std::vector<int32_t>>& offsets) {
    ScopedAccessibilityEventBlocker blocker(GetViewAccessibility());
    if (offsets.has_value()) {
      GetViewAccessibility().SetCharacterOffsets(offsets.value());
    } else {
      GetViewAccessibility().ClearTextOffsets();
    }
  }

  ui::AXNodeData lazy_loading_data_;
};

BEGIN_METADATA(TestView)
END_METADATA

}  // namespace

class ViewAccessibilityTest : public ViewsTestBase {
 public:
  ViewAccessibilityTest() : ax_mode_setter_(ui::kAXModeComplete) {
    view_ = std::make_unique<TestView>();
    child_view_ = view_->AddChildView(std::make_unique<View>());
  }
  ViewAccessibilityTest(const ViewAccessibilityTest&) = delete;
  ViewAccessibilityTest& operator=(const ViewAccessibilityTest&) = delete;
  ~ViewAccessibilityTest() override = default;

  void TearDown() override { ViewsTestBase::TearDown(); }

  TestView* view() { return view_.get(); }
  View* child_view() { return child_view_; }

 protected:
  // std::unique_ptr<Widget> widget_;
  ::ui::ScopedAXModeSetter ax_mode_setter_;
  std::unique_ptr<TestView> view_ = nullptr;
  raw_ptr<View> child_view_ = nullptr;
};

TEST_F(ViewAccessibilityTest, ViewUsesChildViewRole) {
  base::CallbackListSubscription role_changed_subscription_ =
      child_view()->GetViewAccessibility().AddRoleChangedCallback(
          base::BindRepeating(&TestView::OnRoleChanged,
                              base::Unretained(view())));
  view()->GetViewAccessibility().SetRole(ax::mojom::Role::kTextField);
  ui::AXNodeData data;
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kTextField);
  child_view()->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  data = ui::AXNodeData();
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, child_view()->GetViewAccessibility().GetCachedRole());
}

TEST_F(ViewAccessibilityTest, ViewUsesChildViewName) {
  view()->GetViewAccessibility().SetRole(ax::mojom::Role::kTextField);
  child_view()->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  base::CallbackListSubscription name_changed_subscription_ =
      child_view()->GetViewAccessibility().AddStringAttributeChangedCallback(
          ax::mojom::StringAttribute::kName,
          base::BindRepeating(&TestView::OnNameChanged,
                              base::Unretained(view())));
  base::CallbackListSubscription class_name_changed_subscription_ =
      child_view()->GetViewAccessibility().AddStringAttributeChangedCallback(
          ax::mojom::StringAttribute::kClassName,
          base::BindRepeating(&TestView::OnClassNameChanged,
                              base::Unretained(view())));
  child_view()->GetViewAccessibility().SetName("My button");
  ui::AXNodeData data;
  ui::AXNodeData data_2;
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  child_view()->GetViewAccessibility().GetAccessibleNodeData(&data_2);
  EXPECT_EQ(data_2.GetStringAttribute(ax::mojom::StringAttribute::kName),
            "My button");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            data_2.GetString16Attribute(ax::mojom::StringAttribute::kName));

  // Make sure adding another string attribute callback doesnt override the
  // name.
  child_view()->GetViewAccessibility().SetClassName("My class");
  data = ui::AXNodeData();
  data_2 = ui::AXNodeData();
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  child_view()->GetViewAccessibility().GetAccessibleNodeData(&data_2);
  EXPECT_EQ(data_2.GetStringAttribute(ax::mojom::StringAttribute::kName),
            "My button");
  EXPECT_EQ(data_2.GetStringAttribute(ax::mojom::StringAttribute::kClassName),
            "My class");
  EXPECT_EQ(data.GetStringAttribute(ax::mojom::StringAttribute::kClassName),
            data_2.GetStringAttribute(ax::mojom::StringAttribute::kClassName));
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            data_2.GetString16Attribute(ax::mojom::StringAttribute::kName));

  child_view()->GetViewAccessibility().RemoveName();
  data = ui::AXNodeData();
  data_2 = ui::AXNodeData();
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  child_view()->GetViewAccessibility().GetAccessibleNodeData(&data_2);
  EXPECT_FALSE(data_2.HasStringAttribute(ax::mojom::StringAttribute::kName));
  EXPECT_FALSE(data.HasStringAttribute(ax::mojom::StringAttribute::kName));
}

TEST_F(ViewAccessibilityTest, ViewUsesChildViewSelected) {
  base::CallbackListSubscription selected_changed_subscription_ =
      child_view()->GetViewAccessibility().AddBoolAttributeChangedCallback(
          ax::mojom::BoolAttribute::kSelected,
          base::BindRepeating(&TestView::OnSelectedChanged,
                              base::Unretained(view())));
  child_view()->GetViewAccessibility().SetIsSelected(true);
  ui::AXNodeData data;
  ui::AXNodeData data_2;
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  child_view()->GetViewAccessibility().GetAccessibleNodeData(&data_2);
  EXPECT_EQ(data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected),
            data_2.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}

TEST_F(ViewAccessibilityTest, ViewUsesChildViewEditable) {
  base::CallbackListSubscription editable_changed_subscription_ =
      child_view()->GetViewAccessibility().AddStateChangedCallback(
          ax::mojom::State::kEditable,
          base::BindRepeating(&TestView::OnEditableChanged,
                              base::Unretained(view())));
  child_view()->GetViewAccessibility().SetIsEditable(true);
  ui::AXNodeData data;
  ui::AXNodeData data_2;
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  child_view()->GetViewAccessibility().GetAccessibleNodeData(&data_2);
  EXPECT_EQ(data.HasState(ax::mojom::State::kEditable),
            data_2.HasState(ax::mojom::State::kEditable));
}

TEST_F(ViewAccessibilityTest, ViewUsesChildViewPosInSet) {
  base::CallbackListSubscription pos_in_set_changed_subscription_ =
      child_view()->GetViewAccessibility().AddIntAttributeChangedCallback(
          ax::mojom::IntAttribute::kPosInSet,
          base::BindRepeating(&TestView::OnPosInSetChanged,
                              base::Unretained(view())));
  child_view()->GetViewAccessibility().SetPosInSet(1);
  ui::AXNodeData data;
  ui::AXNodeData data_2;
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  child_view()->GetViewAccessibility().GetAccessibleNodeData(&data_2);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
            data_2.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
}

TEST_F(ViewAccessibilityTest, ViewUsesChildViewCharacterOffsets) {
  base::CallbackListSubscription character_offsets_changed_subscription_ =
      child_view()->GetViewAccessibility().AddIntListAttributeChangedCallback(
          ax::mojom::IntListAttribute::kCharacterOffsets,
          base::BindRepeating(&TestView::OnCharacterOffsetsChanged,
                              base::Unretained(view())));
  std::vector<int32_t> offsets = {1, 2, 3};
  child_view()->GetViewAccessibility().SetCharacterOffsets(offsets);
  ui::AXNodeData data;
  ui::AXNodeData data_2;
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  child_view()->GetViewAccessibility().GetAccessibleNodeData(&data_2);
  EXPECT_EQ(
      data.GetIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets),
      data_2.GetIntListAttribute(
          ax::mojom::IntListAttribute::kCharacterOffsets));
}

TEST_F(ViewAccessibilityTest, AccessibleURL) {
  const std::string& test_url("https://example.com");
  view()->GetViewAccessibility().SetRootViewURL(test_url);
  ui::AXNodeData node_data;
  view()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetStringAttribute(ax::mojom::StringAttribute::kUrl),
            test_url);

  // Setting the root view URL is only supported on the root view.
  EXPECT_CHECK_DEATH(
      child_view()->GetViewAccessibility().SetRootViewURL(test_url));
}

TEST_F(ViewAccessibilityTest, LazyLoadingNoOverlap) {
  auto lazy_loading_view = std::make_unique<TestView>();
  lazy_loading_view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  EXPECT_FALSE(lazy_loading_view->GetViewAccessibility().is_initialized());

  lazy_loading_view->lazy_loading_data_.AddStringAttribute(
      ax::mojom::StringAttribute::kName, "My button");
  lazy_loading_view->GetViewAccessibility().CompleteCacheInitialization();

  ui::AXNodeData data;
  lazy_loading_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetStringAttribute(ax::mojom::StringAttribute::kName),
            "My button");
}

TEST_F(ViewAccessibilityTest, LazyLoadingOverlapString) {
  auto lazy_loading_view = std::make_unique<TestView>();
  lazy_loading_view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  lazy_loading_view->lazy_loading_data_.AddStringAttribute(
      ax::mojom::StringAttribute::kName, "Lazy Name");
  lazy_loading_view->GetViewAccessibility().SetName("My button");
  EXPECT_DCHECK_DEATH(
      lazy_loading_view->GetViewAccessibility().CompleteCacheInitialization());
}

TEST_F(ViewAccessibilityTest, LazyLoadingOverlapBool) {
  auto lazy_loading_view = std::make_unique<TestView>();
  lazy_loading_view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  lazy_loading_view->lazy_loading_data_.AddBoolAttribute(
      ax::mojom::BoolAttribute::kSelected, true);
  lazy_loading_view->GetViewAccessibility().SetIsSelected(true);
  EXPECT_DCHECK_DEATH(
      lazy_loading_view->GetViewAccessibility().CompleteCacheInitialization());
}

TEST_F(ViewAccessibilityTest, LazyLoadingOverlapInt) {
  auto lazy_loading_view = std::make_unique<TestView>();
  lazy_loading_view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  lazy_loading_view->lazy_loading_data_.AddIntAttribute(
      ax::mojom::IntAttribute::kPosInSet, 1);
  lazy_loading_view->GetViewAccessibility().SetPosInSet(2);
  EXPECT_DCHECK_DEATH(
      lazy_loading_view->GetViewAccessibility().CompleteCacheInitialization());
}

// Need to rebase to use this function.
TEST_F(ViewAccessibilityTest, LazyLoadingOverlapFloat) {
  auto lazy_loading_view = std::make_unique<TestView>();
  lazy_loading_view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  lazy_loading_view->lazy_loading_data_.AddFloatAttribute(
      ax::mojom::FloatAttribute::kChildTreeScale, 1.0f);
  lazy_loading_view->GetViewAccessibility().SetChildTreeID(
      ui::AXTreeID::CreateNewAXTreeID());
  lazy_loading_view->GetViewAccessibility().SetChildTreeScaleFactor(2.0f);
  EXPECT_DCHECK_DEATH(
      lazy_loading_view->GetViewAccessibility().CompleteCacheInitialization());
}

TEST_F(ViewAccessibilityTest, LazyLoadingOverlapIntList) {
  auto lazy_loading_view = std::make_unique<TestView>();
  lazy_loading_view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  std::vector<int32_t> list_1 = {1, 2, 3};
  lazy_loading_view->lazy_loading_data_.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, list_1);
  std::vector<int32_t> list_2 = {4, 5, 6};
  lazy_loading_view->GetViewAccessibility().SetCharacterOffsets(list_2);
  EXPECT_DCHECK_DEATH(
      lazy_loading_view->GetViewAccessibility().CompleteCacheInitialization());
}

TEST_F(ViewAccessibilityTest, CantSetStateInLazyLoading) {
  auto lazy_loading_view = std::make_unique<TestView>();
  lazy_loading_view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  lazy_loading_view->lazy_loading_data_.AddState(ax::mojom::State::kCollapsed);
  EXPECT_DCHECK_DEATH(
      lazy_loading_view->GetViewAccessibility().CompleteCacheInitialization());
}

TEST_F(ViewAccessibilityTest, CantSetActionsInLazyLoading) {
  auto lazy_loading_view = std::make_unique<TestView>();
  lazy_loading_view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  lazy_loading_view->lazy_loading_data_.AddAction(
      ax::mojom::Action::kDoDefault);
  EXPECT_DCHECK_DEATH(
      lazy_loading_view->GetViewAccessibility().CompleteCacheInitialization());
}

TEST_F(ViewAccessibilityTest, CantSetRelativeBoundsInLazyLoading) {
  auto lazy_loading_view = std::make_unique<TestView>();
  lazy_loading_view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  gfx::RectF relative_bounds(0, 0, 10, 10);
  lazy_loading_view->lazy_loading_data_.relative_bounds.bounds =
      relative_bounds;
  EXPECT_DCHECK_DEATH(
      lazy_loading_view->GetViewAccessibility().CompleteCacheInitialization());
}

TEST_F(ViewAccessibilityTest, EmptyWhenNoChildren) {
  auto view = std::make_unique<TestView>();
  auto children = view->GetViewAccessibility().GetChildren();
  EXPECT_TRUE(children.empty());
}

TEST_F(ViewAccessibilityTest, ReturnsVirtualChildrenOnly) {
  auto view = std::make_unique<TestView>();
  auto virtual_view_1 = std::make_unique<AXVirtualView>();
  auto virtual_view_2 = std::make_unique<AXVirtualView>();

  auto virtual_view_1_id = virtual_view_1->ViewAccessibility::GetUniqueId();
  auto virtual_view_2_id = virtual_view_2->ViewAccessibility::GetUniqueId();

  view->GetViewAccessibility().AddVirtualChildView(std::move(virtual_view_1));
  view->GetViewAccessibility().AddVirtualChildView(std::move(virtual_view_2));

  auto children = view->GetViewAccessibility().GetChildren();
  ASSERT_EQ(children.size(), 2u);
  EXPECT_EQ(children[0]->GetUniqueId(), virtual_view_1_id);
  EXPECT_EQ(children[1]->GetUniqueId(), virtual_view_2_id);
}

TEST_F(ViewAccessibilityTest, ReturnsRealViewChildren) {
  auto parent_view = std::make_unique<TestView>();
  auto child_view_1 = std::make_unique<TestView>();
  auto child_view_2 = std::make_unique<TestView>();

  auto child_view_1_id = child_view_1->GetViewAccessibility().GetUniqueId();
  auto child_view_2_id = child_view_2->GetViewAccessibility().GetUniqueId();

  parent_view->AddChildView(std::move(child_view_1));
  parent_view->AddChildView(std::move(child_view_2));

  auto children = parent_view->GetViewAccessibility().GetChildren();
  ASSERT_EQ(children.size(), 2u);
  EXPECT_EQ(children[0]->GetUniqueId(), child_view_1_id);
  EXPECT_EQ(children[1]->GetUniqueId(), child_view_2_id);
}

TEST_F(ViewAccessibilityTest, VirtualChildrenOverrideRealOnes) {
  auto view = std::make_unique<TestView>();
  auto virtual_view = std::make_unique<AXVirtualView>();
  auto child_view = std::make_unique<TestView>();

  auto virtual_view_id = virtual_view->ViewAccessibility::GetUniqueId();

  view->GetViewAccessibility().AddVirtualChildView(std::move(virtual_view));
  view->AddChildView(std::move(child_view));

  auto children = view->GetViewAccessibility().GetChildren();
  ASSERT_EQ(children.size(), 1u);
  EXPECT_EQ(children[0]->GetUniqueId(), virtual_view_id);
}

TEST_F(ViewAccessibilityTest, EmptyWhenIsLeaf) {
  auto view = std::make_unique<TestView>();
  auto virtual_view = std::make_unique<AXVirtualView>();
  auto child_view = std::make_unique<TestView>();
  view->AddChildView(std::move(child_view));
  view->GetViewAccessibility().AddVirtualChildView(std::move(virtual_view));

  view->GetViewAccessibility().SetIsLeaf(true);

  auto children = view->GetViewAccessibility().GetChildren();
  ASSERT_EQ(children.size(), 0u);
}

TEST_F(ViewAccessibilityTest, GetParent_ReturnsImmediateParent) {
  EXPECT_EQ(child_view()->GetViewAccessibility().GetViewAccessibilityParent(),
            &view()->GetViewAccessibility());
}

TEST_F(ViewAccessibilityTest, GetParent_NoParent) {
  auto lone = std::make_unique<TestView>();
  EXPECT_EQ(lone->GetViewAccessibility().GetViewAccessibilityParent(), nullptr);
}

TEST_F(ViewAccessibilityTest, GetUnignoredParent_NoParent) {
  auto lone = std::make_unique<TestView>();
  EXPECT_EQ(lone->GetViewAccessibility().GetUnignoredParent(), nullptr);
}

TEST_F(ViewAccessibilityTest, GetUnignoredParent_ImmediateNotIgnored) {
  auto parent = std::make_unique<TestView>();
  auto* child = parent->AddChildView(std::make_unique<TestView>());
  EXPECT_EQ(child->GetViewAccessibility().GetUnignoredParent(),
            &parent->GetViewAccessibility());
}

TEST_F(ViewAccessibilityTest, GetUnignoredParent_SkipIgnoredToGrandparent) {
  auto grand = std::make_unique<TestView>();
  auto* parent = grand->AddChildView(std::make_unique<TestView>());
  auto* child = parent->AddChildView(std::make_unique<TestView>());
  parent->GetViewAccessibility().SetIsIgnored(true);
  EXPECT_EQ(child->GetViewAccessibility().GetUnignoredParent(),
            &grand->GetViewAccessibility());
}

TEST_F(ViewAccessibilityTest, AXVirtualView_GetParent_NoParent) {
  AXVirtualView v;
  EXPECT_EQ(v.GetViewAccessibilityParent(), nullptr);
}

TEST_F(ViewAccessibilityTest,
       AXVirtualView_GetParent_ImmediateVirtualViewNotIgnored) {
  AXVirtualView parent;
  auto child = std::make_unique<AXVirtualView>();
  auto* child_ptr = child.get();
  parent.AddChildView(std::move(child));

  EXPECT_EQ(child_ptr->GetViewAccessibilityParent(), &parent);
}

TEST_F(ViewAccessibilityTest,
       AXVirtualView_GetUnignoredParent_SkipIgnoredToGrandparentVirtualView) {
  AXVirtualView grand;
  auto parent = std::make_unique<AXVirtualView>();
  parent->SetIsIgnored(true);
  auto child = std::make_unique<AXVirtualView>();
  auto* child_ptr = child.get();

  parent->AddChildView(std::move(child));
  grand.AddChildView(std::move(parent));

  EXPECT_EQ(child_ptr->GetUnignoredParent(), &grand);
}

TEST_F(ViewAccessibilityTest,
       AXVirtualView_GetUnignoredParent_VirtualIfNoReal) {
  auto view = std::make_unique<TestView>();
  auto parent = std::make_unique<AXVirtualView>();
  parent->SetIsIgnored(true);
  auto child = std::make_unique<AXVirtualView>();
  auto* child_ptr = child.get();
  parent->AddChildView(std::move(child));
  view->GetViewAccessibility().AddVirtualChildView(std::move(parent));

  EXPECT_EQ(child_ptr->GetUnignoredParent(), &view->GetViewAccessibility());
}

}  // namespace views::test
