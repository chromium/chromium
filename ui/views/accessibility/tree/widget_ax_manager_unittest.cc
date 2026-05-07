// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/tree/widget_ax_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/platform/ax_platform_for_test.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/accessibility/tree/widget_ax_manager_test_api.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace views::test {

class WidgetAXManagerTest : public test::WidgetTest {
 protected:
  WidgetAXManagerTest() = default;
  ~WidgetAXManagerTest() override = default;

  void SetUp() override {
    WidgetTest::SetUp();
    widget_.reset(CreateTopLevelPlatformWidget());
  }

  void TearDown() override {
    widget_.reset();
    WidgetTest::TearDown();
  }

  Widget* widget() { return widget_.get(); }
  WidgetAXManager* manager() { return widget_->ax_manager(); }

  View* AddFocusableView() { return AddFocusableView(widget()->GetRootView()); }

  View* AddFocusableView(View* parent) {
    auto* view = parent->AddChildView(std::make_unique<View>());
    view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
    view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
    view->GetViewAccessibility().SetName("Focusable test view");
    return view;
  }

 private:
  WidgetAutoclosePtr widget_;

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAccessibilityTreeForViews};
};

class WidgetAXManagerObserver : public views::WidgetAXManagerObserver {
 public:
  WidgetAXManagerObserver() = default;
  WidgetAXManagerObserver(const WidgetAXManagerObserver&) = delete;
  WidgetAXManagerObserver& operator=(const WidgetAXManagerObserver&) = delete;
  ~WidgetAXManagerObserver() override = default;

  void OnWidgetAXManagerEnabled() override { ++enabled_count_; }

  int enabled_count() const { return enabled_count_; }

 private:
  int enabled_count_ = 0;
};

TEST_F(WidgetAXManagerTest, InitiallyDisabled) {
  EXPECT_FALSE(manager()->is_enabled());
}

TEST_F(WidgetAXManagerTest, EnableSetsEnabled) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();
  EXPECT_TRUE(manager()->is_enabled());
}

TEST_F(WidgetAXManagerTest, IsEnabledAfterAXModeAdded) {
  // Initially, the manager should not be enabled.
  ASSERT_FALSE(manager()->is_enabled());

  // Simulate that AXMode with kNativeAPIs was added.
  ui::AXPlatform::GetInstance().NotifyModeAdded(ui::AXMode::kNativeAPIs);
  EXPECT_TRUE(manager()->is_enabled());
}

TEST_F(WidgetAXManagerTest, Enable_SerializesSubtree) {
  WidgetAXManagerTestApi api(manager());

  View* root = widget()->GetRootView();
  auto* c1 = root->AddChildView(std::make_unique<View>());
  auto* c2 = root->AddChildView(std::make_unique<View>());
  auto* g1 = c1->AddChildView(std::make_unique<View>());
  auto* g2 = c2->AddChildView(std::make_unique<View>());

  auto& rax = root->GetViewAccessibility();
  auto& c1ax = c1->GetViewAccessibility();
  auto& c2ax = c2->GetViewAccessibility();
  auto& g1ax = g1->GetViewAccessibility();
  auto& g2ax = g2->GetViewAccessibility();

  EXPECT_EQ(api.cache()->Get(rax.GetUniqueId()),
            &widget()->GetRootView()->GetViewAccessibility());
  EXPECT_EQ(api.cache()->Get(c1ax.GetUniqueId()), nullptr);
  EXPECT_EQ(api.cache()->Get(c2ax.GetUniqueId()), nullptr);
  EXPECT_EQ(api.cache()->Get(g1ax.GetUniqueId()), nullptr);
  EXPECT_EQ(api.cache()->Get(g2ax.GetUniqueId()), nullptr);

  api.Enable();

  EXPECT_EQ(api.cache()->Get(rax.GetUniqueId()), &rax);
  EXPECT_EQ(api.cache()->Get(c1ax.GetUniqueId()), &c1ax);
  EXPECT_EQ(api.cache()->Get(c2ax.GetUniqueId()), &c2ax);
  EXPECT_EQ(api.cache()->Get(g1ax.GetUniqueId()), &g1ax);
  EXPECT_EQ(api.cache()->Get(g2ax.GetUniqueId()), &g2ax);

  EXPECT_FALSE(api.cache()->HasCachedChildren(&rax));
  EXPECT_FALSE(api.cache()->HasCachedChildren(&c1ax));
  EXPECT_FALSE(api.cache()->HasCachedChildren(&c2ax));
  EXPECT_FALSE(api.cache()->HasCachedChildren(&g1ax));
  EXPECT_FALSE(api.cache()->HasCachedChildren(&g2ax));
}

TEST_F(WidgetAXManagerTest, InitInitializesBasicAXTreeManagerWhenAXOff) {
  WidgetAXManager manager(widget());
  WidgetAXManagerTestApi api(&manager);

  EXPECT_EQ(api.ax_tree_manager(), nullptr);
  manager.Init();
  EXPECT_NE(api.ax_tree_manager(), nullptr);

  auto& rax = widget()->GetRootView()->GetViewAccessibility();
  EXPECT_EQ(api.cache()->Get(rax.GetUniqueId()), &rax);
  EXPECT_EQ(api.ax_tree_manager()->ax_tree()->size(), 1);

  EXPECT_EQ(api.ax_tree_manager()->GetRoot()->id(), rax.GetUniqueId());
}

TEST_F(WidgetAXManagerTest, InitInitializesFullyAXTreeManagerWhenAXOn) {
  ui::ScopedAXModeSetter enable_accessibility(ui::AXMode::kNativeAPIs);
  WidgetAXManager manager(widget());
  WidgetAXManagerTestApi api(&manager);

  EXPECT_EQ(api.ax_tree_manager(), nullptr);
  manager.Init();
  EXPECT_TRUE(manager.is_enabled());
  EXPECT_NE(api.ax_tree_manager(), nullptr);

  EXPECT_GT(api.ax_tree_manager()->ax_tree()->size(), 1);
}

// Init() should enable the manager when kNativeAPIs is set
// alongside other AXMode flags.
TEST_F(WidgetAXManagerTest, InitEnablesWhenMultipleAXModeFlagsSet) {
  ui::ScopedAXModeSetter enable_accessibility(ui::kAXModeComplete);
  WidgetAXManager manager(widget());
  WidgetAXManagerTestApi api(&manager);

  EXPECT_EQ(api.ax_tree_manager(), nullptr);
  manager.Init();
  EXPECT_TRUE(manager.is_enabled());
  EXPECT_NE(api.ax_tree_manager(), nullptr);

  EXPECT_GT(api.ax_tree_manager()->ax_tree()->size(), 1);
}

TEST_F(WidgetAXManagerTest, InitWithAXModeOnTracksFocusAfterWidgetCreated) {
  ui::ScopedAXModeSetter enable_accessibility(ui::AXMode::kNativeAPIs);
  WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
  WidgetAXManager* manager = widget->ax_manager();

  ASSERT_TRUE(manager->is_enabled());

  View* view = widget->GetRootView()->AddChildView(std::make_unique<View>());
  view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  widget->Show();
  view->RequestFocus();

  EXPECT_EQ(manager->GetFocusedNodeId(),
            view->GetViewAccessibility().GetUniqueId());
}

TEST_F(WidgetAXManagerTest, Init_DoesNotInitAXTreeManagerForNonTopLevel) {
  std::unique_ptr<Widget> child_widget =
      base::WrapUnique(CreateChildNativeWidgetWithParent(
          widget(), Widget::InitParams::CLIENT_OWNS_WIDGET));

  auto* child_mgr = child_widget->ax_manager();
  WidgetAXManagerTestApi child_api(child_mgr);

  ASSERT_FALSE(child_widget->is_top_level());

  // Init() should not create an AXTreeManager on that child widget.
  EXPECT_EQ(child_api.ax_tree_manager(), nullptr);
  child_mgr->Init();
  EXPECT_EQ(child_api.ax_tree_manager(), nullptr);

  child_api.TearDown();
  child_widget->CloseNow();
  child_widget.reset();
}

TEST_F(WidgetAXManagerTest, ChildWidget_EnableSerializesFullTree) {
  std::unique_ptr<Widget> child_widget =
      base::WrapUnique(CreateChildNativeWidgetWithParent(
          widget(), Widget::InitParams::CLIENT_OWNS_WIDGET));
  auto* child_mgr = child_widget->ax_manager();
  child_mgr->Init();

  WidgetAXManagerTestApi child_api(child_mgr);

  View* child_root = child_widget->GetRootView();
  auto* c1 = child_root->AddChildView(std::make_unique<View>());
  auto* c2 = child_root->AddChildView(std::make_unique<View>());
  auto* g1 = c1->AddChildView(std::make_unique<View>());
  auto* g2 = c2->AddChildView(std::make_unique<View>());

  EXPECT_EQ(child_api.ax_tree_manager(), nullptr);

  ui::AXPlatform::GetInstance().NotifyModeAdded(ui::AXMode::kNativeAPIs);

  EXPECT_TRUE(child_mgr->is_enabled());
  EXPECT_NE(child_api.ax_tree_manager(), nullptr);

  // Expect >1 because the whole subtree (root + descendants) should be
  // serialized.
  EXPECT_GT(child_api.ax_tree_manager()->ax_tree()->size(), 1);

  // Spot-check that descendants made it into the tree.
  auto id = [&](View* v) {
    return static_cast<ui::AXNodeID>(v->GetViewAccessibility().GetUniqueId());
  };
  EXPECT_NE(child_api.ax_tree_manager()->ax_tree()->GetFromId(id(child_root)),
            nullptr);
  EXPECT_NE(child_api.ax_tree_manager()->ax_tree()->GetFromId(id(c1)), nullptr);
  EXPECT_NE(child_api.ax_tree_manager()->ax_tree()->GetFromId(id(c2)), nullptr);
  EXPECT_NE(child_api.ax_tree_manager()->ax_tree()->GetFromId(id(g1)), nullptr);
  EXPECT_NE(child_api.ax_tree_manager()->ax_tree()->GetFromId(id(g2)), nullptr);

  child_api.TearDown();
  child_widget->CloseNow();
  child_widget.reset();
}

TEST_F(WidgetAXManagerTest, InitParamsCreatesParentRelationship) {
  WidgetAXManagerTestApi parent_api(manager());

  std::unique_ptr<Widget> child_widget =
      base::WrapUnique(CreateChildNativeWidgetWithParent(
          widget(), Widget::InitParams::CLIENT_OWNS_WIDGET));
  auto* child_mgr = child_widget->ax_manager();
  WidgetAXManagerTestApi child_api(child_mgr);

  // The AX manager should have picked up the parent when Init() ran.
  EXPECT_EQ(child_api.parent_ax_tree_id(), parent_api.ax_tree_id());

  child_api.TearDown();

  child_widget->CloseNow();
  child_widget.reset();
}

TEST_F(WidgetAXManagerTest, ChildWidgetSerializedTreeDataIncludesParentTreeId) {
  ui::ScopedAXModeSetter enable_accessibility(ui::AXMode::kNativeAPIs);
  WidgetAutoclosePtr parent(CreateTopLevelPlatformWidget());
  WidgetAXManagerTestApi parent_api(parent->ax_manager());
  ASSERT_TRUE(parent->ax_manager()->is_enabled());

  std::unique_ptr<Widget> child_widget =
      base::WrapUnique(CreateChildNativeWidgetWithParent(
          parent.get(), Widget::InitParams::CLIENT_OWNS_WIDGET));
  WidgetAXManager* child_manager = child_widget->ax_manager();
  WidgetAXManagerTestApi child_api(child_manager);
  ASSERT_TRUE(child_manager->is_enabled());
  ASSERT_NE(child_api.ax_tree_manager(), nullptr);

  EXPECT_EQ(child_api.parent_ax_tree_id(), parent_api.ax_tree_id());
  EXPECT_EQ(child_api.ax_tree_manager()->GetTreeData().parent_tree_id,
            parent_api.ax_tree_id());

  child_api.TearDown();
  child_widget->CloseNow();
  child_widget.reset();
}

TEST_F(WidgetAXManagerTest, ReparentWidgetBetweenParents) {
  WidgetAXManagerTestApi parent1_api(manager());

  WidgetAutoclosePtr parent2(CreateTopLevelPlatformWidget());
  WidgetAXManagerTestApi parent2_api(parent2->ax_manager());

  std::unique_ptr<Widget> child_widget =
      base::WrapUnique(CreateChildNativeWidgetWithParent(
          widget(), Widget::InitParams::CLIENT_OWNS_WIDGET));

  WidgetAXManagerTestApi child_api(child_widget->ax_manager());
  EXPECT_EQ(child_api.parent_ax_tree_id(), parent1_api.ax_tree_id());

  // Reparent via Widget::Reparent() should update the parent AXTreeID.
  child_widget->Reparent(parent2.get());
  EXPECT_EQ(child_api.parent_ax_tree_id(), parent2_api.ax_tree_id());

  child_api.TearDown();
  child_widget->CloseNow();
  child_widget.reset();
}

TEST_F(WidgetAXManagerTest, RemovingChildResetsParent) {
  std::unique_ptr<Widget> child_widget =
      base::WrapUnique(CreateChildNativeWidgetWithParent(
          widget(), Widget::InitParams::CLIENT_OWNS_WIDGET));
  WidgetAXManagerTestApi child_api(child_widget->ax_manager());
  ASSERT_NE(child_api.parent_ax_tree_id(), ui::AXTreeID());

  // Detach the child widget from its parent should reset the parent AXTreeID.
  child_widget->Reparent(nullptr);
  EXPECT_EQ(child_api.parent_ax_tree_id(), ui::AXTreeID());

  child_api.TearDown();
  child_widget->CloseNow();
  child_widget.reset();
}

class WidgetAXManagerOffTest : public ViewsTestBase {
 protected:
  WidgetAXManagerOffTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kAccessibilityTreeForViews);
  }
  ~WidgetAXManagerOffTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This death test verifies that Create() crashes (via CHECK) when the flag is
// off.
TEST_F(WidgetAXManagerOffTest, CrashesWhenFlagOff) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 500, 500);
  widget->Init(std::move(params));

  auto create_manager = [&]() { WidgetAXManager manager(widget.get()); };
  EXPECT_DEATH(create_manager(), "");

  widget->CloseNow();
  widget.reset();
}

TEST_F(WidgetAXManagerTest, OnEvent_PostsSingleTaskAndQueuesCorrectly) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  EXPECT_TRUE(api.pending_events().empty());
  EXPECT_TRUE(api.pending_data_updates().empty());
  EXPECT_FALSE(api.processing_update_posted());

  auto* v1 = widget()->GetRootView()->AddChildView(std::make_unique<View>());
  auto* v2 = widget()->GetRootView()->AddChildView(std::make_unique<View>());

  // Wait for the serialization triggered by adding the child views to flush.
  api.WaitForNextSerialization();

  // Fire an event on v1, one on v2, before the first send.
  auto before = task_environment()->GetPendingMainThreadTaskCount();
  manager()->OnEvent(v1->GetViewAccessibility(),
                     ax::mojom::Event::kControlsChanged);
  manager()->OnEvent(v2->GetViewAccessibility(),
                     ax::mojom::Event::kControlsChanged);

  // Still just one task posted.
  EXPECT_EQ(task_environment()->GetPendingMainThreadTaskCount(), before + 1u);
  EXPECT_TRUE(api.processing_update_posted());

  // pending_events has three entries, pending_data_updates has two unique IDs.
  EXPECT_EQ(api.pending_events().size(), 2u);
  EXPECT_EQ(api.pending_data_updates().size(), 2u);

  // After run, everything clears.
  api.WaitForNextSerialization();
  EXPECT_EQ(api.pending_events().size(), 0u);
  EXPECT_EQ(api.pending_data_updates().size(), 0u);
  EXPECT_FALSE(api.processing_update_posted());

  ASSERT_EQ(api.last_serialization().events.size(), 2u);
  ASSERT_GE(api.last_serialization().updates.size(), 1u);
  EXPECT_EQ(api.last_serialization().events[0].event_type,
            ax::mojom::Event::kControlsChanged);
  EXPECT_EQ(api.last_serialization().events[1].event_type,
            ax::mojom::Event::kControlsChanged);
}

TEST_F(WidgetAXManagerTest, DiesOnUnhandledEventRouting) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  EXPECT_DEATH(
      {
        manager()->OnEvent(widget()->GetRootView()->GetViewAccessibility(),
                           ax::mojom::Event::kMouseMoved);
        api.WaitForNextSerialization();
      },
      "Unhandled event");
}

TEST_F(WidgetAXManagerTest, OnDataChanged_PostsSingleTaskAndQueuesCorrectly) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  EXPECT_TRUE(api.pending_events().empty());
  EXPECT_TRUE(api.pending_data_updates().empty());
  EXPECT_FALSE(api.processing_update_posted());

  auto before = task_environment()->GetPendingMainThreadTaskCount();

  auto* v1 = widget()->GetRootView()->AddChildView(std::make_unique<View>());
  widget()->GetRootView()->AddChildView(std::make_unique<View>());

  // We don't explicitly call OnDataChanged for v1 and v2 because adding those
  // views as children of the root view should automatically call it.

  // One task scheduled, two unique IDs in pending_data_updates.
  EXPECT_EQ(task_environment()->GetPendingMainThreadTaskCount(), before + 1u);
  EXPECT_TRUE(api.processing_update_posted());
  EXPECT_EQ(api.pending_events().size(), 0u);
  // Three unique IDs: v1, v2, and their parent view.
  EXPECT_EQ(api.pending_data_updates().size(), 3u);

  // Duplicate data-change for v1 should not grow the set.
  before = task_environment()->GetPendingMainThreadTaskCount();
  manager()->OnDataChanged(v1->GetViewAccessibility());
  EXPECT_EQ(api.pending_data_updates().size(), 3u);
  EXPECT_EQ(task_environment()->GetPendingMainThreadTaskCount(), before);

  // After run, clear everything.
  api.WaitForNextSerialization();

  EXPECT_EQ(api.pending_data_updates().size(), 0u);
  EXPECT_FALSE(api.processing_update_posted());

  ASSERT_EQ(api.last_serialization().updates.size(), 1u);
  EXPECT_FALSE(api.last_serialization().updates[0].nodes.empty());
}

TEST_F(WidgetAXManagerTest, OnEvent_CanScheduleAgainAfterSend) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  auto* v = widget()->GetRootView()->AddChildView(std::make_unique<View>());
  api.WaitForNextSerialization();

  // First batch.
  manager()->OnEvent(v->GetViewAccessibility(),
                     ax::mojom::Event::kControlsChanged);
  api.WaitForNextSerialization();

  EXPECT_FALSE(api.processing_update_posted());
  EXPECT_TRUE(api.pending_events().empty());
  EXPECT_TRUE(api.pending_data_updates().empty());
  ASSERT_EQ(api.last_serialization().events.size(), 1u);
  EXPECT_EQ(api.last_serialization().events[0].event_type,
            ax::mojom::Event::kControlsChanged);

  // Second batch.
  auto before = task_environment()->GetPendingMainThreadTaskCount();
  manager()->OnEvent(v->GetViewAccessibility(),
                     ax::mojom::Event::kControlsChanged);
  EXPECT_EQ(task_environment()->GetPendingMainThreadTaskCount(), before + 1u);
  EXPECT_TRUE(api.processing_update_posted());
  EXPECT_EQ(api.pending_events().size(), 1u);
  EXPECT_EQ(api.pending_data_updates().size(), 1u);

  api.WaitForNextSerialization();
  ASSERT_EQ(api.last_serialization().events.size(), 1u);
  EXPECT_EQ(api.last_serialization().events[0].event_type,
            ax::mojom::Event::kControlsChanged);
}

TEST_F(WidgetAXManagerTest, OnDataChanged_CanScheduleAgainAfterSend) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  auto* v = widget()->GetRootView()->AddChildView(std::make_unique<View>());
  api.WaitForNextSerialization();

  // First batch.
  manager()->OnDataChanged(v->GetViewAccessibility());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(api.processing_update_posted());
  EXPECT_TRUE(api.pending_events().empty());
  EXPECT_TRUE(api.pending_data_updates().empty());

  // Second batch.
  auto before = task_environment()->GetPendingMainThreadTaskCount();
  manager()->OnDataChanged(v->GetViewAccessibility());
  EXPECT_EQ(task_environment()->GetPendingMainThreadTaskCount(), before + 1u);
  EXPECT_TRUE(api.processing_update_posted());
  EXPECT_EQ(api.pending_events().size(), 0u);
  EXPECT_EQ(api.pending_data_updates().size(), 1u);
}

TEST_F(WidgetAXManagerTest, UpdatesIgnoredWhenDisabled) {
  WidgetAXManagerTestApi api(manager());

  // Manager is disabled by default.
  auto* v = widget()->GetRootView()->AddChildView(std::make_unique<View>());

  auto before = task_environment()->GetPendingMainThreadTaskCount();
  manager()->OnEvent(v->GetViewAccessibility(), ax::mojom::Event::kFocus);
  EXPECT_FALSE(api.processing_update_posted());
  EXPECT_TRUE(api.pending_events().empty());
  EXPECT_TRUE(api.pending_data_updates().empty());
  EXPECT_EQ(task_environment()->GetPendingMainThreadTaskCount(), before);

  manager()->OnDataChanged(v->GetViewAccessibility());
  EXPECT_FALSE(api.processing_update_posted());
  EXPECT_TRUE(api.pending_data_updates().empty());
  EXPECT_EQ(task_environment()->GetPendingMainThreadTaskCount(), before);
}

TEST_F(WidgetAXManagerTest,
       SendPendingUpdate_SerializationOnChildAddedAndRemoved) {
  WidgetAXManagerTestApi api(manager());

  api.Enable();

  EXPECT_EQ(api.ax_tree_manager()->ax_tree()->root()->id(),
            static_cast<int32_t>(
                widget()->GetRootView()->GetViewAccessibility().GetUniqueId()));
  EXPECT_GT(api.ax_tree_manager()->ax_tree()->size(), 1);

  // Adding a child view should automatically call OnDataChanged, which in turn
  // should schedule a pending serialization.
  auto* child = widget()->GetRootView()->AddChildView(std::make_unique<View>());
  ui::AXNodeID child_id =
      static_cast<ui::AXNodeID>(child->GetViewAccessibility().GetUniqueId());
  api.WaitForNextSerialization();

  EXPECT_NE(api.ax_tree_manager()->ax_tree()->GetFromId(child_id), nullptr);

  // Removing a child view should also schedule a pending serialization.
  widget()->GetRootView()->RemoveChildViewT(child);
  api.WaitForNextSerialization();

  EXPECT_EQ(api.ax_tree_manager()->ax_tree()->GetFromId(child_id), nullptr);
}

TEST_F(WidgetAXManagerTest, SendPendingUpdate_SendsSerializedUpdates) {
  WidgetAXManagerTestApi api(manager());

  api.Enable();

  manager()->OnEvent(widget()->GetRootView()->GetViewAccessibility(),
                     ax::mojom::Event::kControlsChanged);
  api.WaitForNextSerialization();

  EXPECT_EQ(api.last_serialization().events.size(), 1u);
  EXPECT_EQ(api.last_serialization().events[0].event_type,
            ax::mojom::Event::kControlsChanged);

  EXPECT_FALSE(api.last_serialization().updates.empty());

  EXPECT_NE(
      api.ax_tree_manager()->ax_tree()->GetFromId(static_cast<ui::AXNodeID>(
          widget()->GetRootView()->GetViewAccessibility().GetUniqueId())),
      nullptr);
}

TEST_F(WidgetAXManagerTest, SendPendingUpdate_NoSerializeWhenNodeNotInTree) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  // This view is not part of the widget.
  auto v = ViewAccessibility::Create(nullptr);

  manager()->OnDataChanged(*v.get());
  api.WaitForNextSerialization();

  EXPECT_EQ(api.has_last_serialization(), false);
  EXPECT_EQ(api.ax_tree_manager()->ax_tree()->GetFromId(
                static_cast<ui::AXNodeID>(v->GetUniqueId())),
            nullptr);
}

TEST_F(WidgetAXManagerTest,
       GetNativeViewAccessibleForIdReturnsBrowserAccessible) {
  ui::ScopedAXModeSetter enable_accessibility(ui::AXMode::kNativeAPIs);
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  auto* child = widget()->GetRootView()->AddChildView(std::make_unique<View>());
  api.WaitForNextSerialization();

  ui::BrowserAccessibilityManager* browser_manager = api.ax_tree_manager();
  ASSERT_NE(browser_manager, nullptr);

  const ui::AXNodeID child_id =
      static_cast<ui::AXNodeID>(child->GetViewAccessibility().GetUniqueId());
  ui::BrowserAccessibility* browser_node = browser_manager->GetFromID(child_id);
  ASSERT_NE(browser_node, nullptr);

  gfx::NativeViewAccessible expected = browser_node->GetNativeViewAccessible();
  EXPECT_NE(expected, gfx::NativeViewAccessible());
  EXPECT_EQ(expected, manager()->GetNativeViewAccessibleForId(child_id));
}

TEST_F(WidgetAXManagerTest,
       GetNativeViewAccessibleForIdWithoutAXTreeManagerReturnsNull) {
  std::unique_ptr<Widget> child_widget =
      base::WrapUnique(CreateChildNativeWidgetWithParent(
          widget(), Widget::InitParams::CLIENT_OWNS_WIDGET));
  auto* child_manager = child_widget->ax_manager();
  ASSERT_NE(child_manager, nullptr);

  WidgetAXManagerTestApi child_api(child_manager);
  EXPECT_EQ(child_api.ax_tree_manager(), nullptr);

  ui::AXNodeID child_root_id = static_cast<ui::AXNodeID>(
      child_widget->GetRootView()->GetViewAccessibility().GetUniqueId());
  EXPECT_EQ(child_manager->GetNativeViewAccessibleForId(child_root_id),
            gfx::NativeViewAccessible());

  child_api.TearDown();
  child_widget->CloseNow();
  child_widget.reset();
}

TEST_F(WidgetAXManagerTest,
       ViewAccessibilityGetNativeObjectMatchesBrowserAccessible) {
  ui::ScopedAXModeSetter enable_accessibility(ui::AXMode::kNativeAPIs);
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  auto* child = widget()->GetRootView()->AddChildView(std::make_unique<View>());
  api.WaitForNextSerialization();

  ui::BrowserAccessibilityManager* browser_manager = api.ax_tree_manager();
  ASSERT_NE(browser_manager, nullptr);

  const ui::AXNodeID child_id =
      static_cast<ui::AXNodeID>(child->GetViewAccessibility().GetUniqueId());
  ui::BrowserAccessibility* browser_node = browser_manager->GetFromID(child_id);
  ASSERT_NE(browser_node, nullptr);

  gfx::NativeViewAccessible expected = browser_node->GetNativeViewAccessible();
  EXPECT_NE(expected, gfx::NativeViewAccessible());
  EXPECT_EQ(expected, child->GetViewAccessibility().GetNativeObject());
}

TEST_F(WidgetAXManagerTest, AccessibilityViewHasFocusAndSetFocus) {
  EXPECT_FALSE(widget()->IsActive());
  EXPECT_FALSE(manager()->AccessibilityViewHasFocus());

  manager()->AccessibilityViewSetFocus();
  EXPECT_TRUE(widget()->IsActive());
  EXPECT_TRUE(manager()->AccessibilityViewHasFocus());

  // Calling SetFocus again doesn't change the active state.
  manager()->AccessibilityViewSetFocus();
  EXPECT_TRUE(widget()->IsActive());
  EXPECT_TRUE(manager()->AccessibilityViewHasFocus());
}

TEST_F(WidgetAXManagerTest, AccessibilityGetViewBounds_ReturnsWidgetBounds) {
  gfx::Rect test_bounds(10, 20, 300, 400);
  widget()->SetBounds(test_bounds);

  EXPECT_EQ(manager()->AccessibilityGetViewBounds(), test_bounds);
}

TEST_F(WidgetAXManagerTest, AccessibilityGetAcceleratedWidget) {
  gfx::AcceleratedWidget aw = manager()->AccessibilityGetAcceleratedWidget();
#if BUILDFLAG(IS_WIN)
  // On Windows we should get a real HWND.
  EXPECT_NE(aw, gfx::kNullAcceleratedWidget);
#else
  // Everywhere else it always returns the null widget.
  EXPECT_EQ(aw, gfx::kNullAcceleratedWidget);
#endif
}

TEST_F(WidgetAXManagerTest, AccessibilityGetNativeViewAccessible) {
#if BUILDFLAG(IS_MAC)
  // On macOS we get the NSView’s accessibility object.
  auto view_acc = manager()->AccessibilityGetNativeViewAccessible();
  EXPECT_NE(view_acc, gfx::NativeViewAccessible());
#elif BUILDFLAG(IS_WIN)
  // On Windows we should get a real IAccessible*.
  auto win_acc = manager()->AccessibilityGetNativeViewAccessible();
  EXPECT_NE(win_acc, nullptr);
#else
  // On other platforms it always falls back to empty.
  EXPECT_EQ(manager()->AccessibilityGetNativeViewAccessible(),
            gfx::NativeViewAccessible());
#endif
}

// AccessibilityGetNativeViewAccessibleForWindow

TEST_F(WidgetAXManagerTest, AccessibilityGetNativeViewAccessibleForWindow) {
#if BUILDFLAG(IS_MAC)
  // On macOS we get the NSWindow’s accessibility object.
  auto win_acc = manager()->AccessibilityGetNativeViewAccessibleForWindow();
  EXPECT_NE(win_acc, gfx::NativeViewAccessible());
#else
  // On other platforms it always returns empty.
  EXPECT_EQ(manager()->AccessibilityGetNativeViewAccessibleForWindow(),
            gfx::NativeViewAccessible());
#endif
}

TEST_F(WidgetAXManagerTest, GetTopLevelNativeWindow) {
  // Null widget should return nullptr.
  WidgetAXManager null_manager(nullptr);
  EXPECT_EQ(null_manager.GetTopLevelNativeWindow(), gfx::NativeWindow());

  // Top-level widget should return its native window.
  gfx::NativeWindow top_native = widget()->GetNativeWindow();
  EXPECT_EQ(manager()->GetTopLevelNativeWindow(), top_native);

  // Child widget should still return the top-level native window.
  std::unique_ptr<Widget> child_widget =
      base::WrapUnique(CreateChildNativeWidgetWithParent(
          widget(), Widget::InitParams::CLIENT_OWNS_WIDGET));
  auto* child_mgr = child_widget->ax_manager();
  EXPECT_EQ(child_mgr->GetTopLevelNativeWindow(), top_native);

  child_widget->CloseNow();
}

TEST_F(WidgetAXManagerTest, CanFireAccessibilityEvents) {
  // Null widget should always return false.
  WidgetAXManager null_mgr(nullptr);
  EXPECT_FALSE(null_mgr.CanFireAccessibilityEvents());

  // Newly created widget is not visible by default.
  EXPECT_TRUE(widget()->IsNativeWidgetInitialized());
  EXPECT_FALSE(widget()->IsVisible());
  EXPECT_FALSE(manager()->CanFireAccessibilityEvents());

  // Once shown (visible), it should return true.
  widget()->Show();
  EXPECT_TRUE(widget()->IsVisible());
  EXPECT_TRUE(manager()->CanFireAccessibilityEvents());
}

TEST_F(WidgetAXManagerTest, GetOrCreateAXNodeUniqueId) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  auto v = std::make_unique<View>();
  EXPECT_EQ(manager()->GetOrCreateAXNodeUniqueId(
                v->GetViewAccessibility().GetUniqueId()),
            ui::AXUniqueId::CreateInvalid());

  View* v_raw = widget()->GetRootView()->AddChildView(std::move(v));

  WidgetAXManagerTestApi test_api(manager());
  test_api.WaitForNextSerialization();

  EXPECT_EQ(manager()->GetOrCreateAXNodeUniqueId(
                v_raw->GetViewAccessibility().GetUniqueId()),
            v_raw->GetViewAccessibility().GetUniqueId());
}

TEST_F(WidgetAXManagerTest, OnChildAddedAndRemoved_ReserializeOnParent) {
  WidgetAXManagerTestApi api(manager());

  api.Enable();

  auto child = ViewAccessibility::Create(nullptr);
  auto parent = ViewAccessibility::Create(nullptr);

  // Adding a child should schedule a pending serialization on the parent.
  manager()->OnChildAdded(*child, *parent);
  EXPECT_EQ(api.pending_data_updates().size(), 1u);
  EXPECT_TRUE(api.pending_data_updates().contains(parent->GetUniqueId()));

  // Process the pending update to clear it.
  api.WaitForNextSerialization();

  // Removing a child should also schedule a pending serialization on the
  // parent.
  EXPECT_TRUE(api.pending_data_updates().empty());
  manager()->OnChildRemoved(*child, *parent);
  EXPECT_EQ(api.pending_data_updates().size(), 1u);
  EXPECT_TRUE(api.pending_data_updates().contains(parent->GetUniqueId()));
}

TEST_F(WidgetAXManagerTest, CacheTracksChildAddRemoveAfterEnable) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  View* root = widget()->GetRootView();

  auto* child = root->AddChildView(std::make_unique<View>());
  const auto child_id = child->GetViewAccessibility().GetUniqueId();
  EXPECT_EQ(api.cache()->Get(child_id), &child->GetViewAccessibility());

  root->RemoveChildViewT(child);
  EXPECT_EQ(api.cache()->Get(child_id), nullptr);
}

TEST_F(WidgetAXManagerTest, ObserverReceivesNotificationWhenEnabled) {
  WidgetAXManagerObserver observer;
  manager()->AddObserver(&observer);

  WidgetAXManagerTestApi api(manager());
  EXPECT_EQ(observer.enabled_count(), 0);

  api.Enable();
  EXPECT_EQ(observer.enabled_count(), 1);

  manager()->RemoveObserver(&observer);
}

TEST_F(WidgetAXManagerTest, ObserverNotifiedOnlyOnceForRepeatedEnable) {
  WidgetAXManagerObserver observer;
  manager()->AddObserver(&observer);

  WidgetAXManagerTestApi api(manager());
  api.Enable();
  EXPECT_EQ(observer.enabled_count(), 1);

  manager()->OnAXModeAdded(ui::AXMode::kNativeAPIs);
  EXPECT_EQ(observer.enabled_count(), 1);

  manager()->RemoveObserver(&observer);
}

TEST_F(WidgetAXManagerTest, RemovedObserverDoesNotReceiveNotifications) {
  WidgetAXManagerObserver observer;
  manager()->AddObserver(&observer);
  manager()->RemoveObserver(&observer);

  WidgetAXManagerTestApi api(manager());
  api.Enable();

  EXPECT_EQ(observer.enabled_count(), 0);
}

TEST_F(WidgetAXManagerTest,
       VirtualChildrenAddedBeforeWidgetAreRegisteredInCache) {
  // Create a view with nested virtual children *before* attaching to widget.
  auto container = std::make_unique<View>();
  auto virtual_child = std::make_unique<AXVirtualView>();
  auto nested_virtual = std::make_unique<AXVirtualView>();

  auto virtual_child_id = virtual_child->ViewAccessibility::GetUniqueId();
  auto nested_virtual_id = nested_virtual->ViewAccessibility::GetUniqueId();

  virtual_child->AddChildView(std::move(nested_virtual));
  container->GetViewAccessibility().AddVirtualChildView(
      std::move(virtual_child));

  WidgetAXManagerTestApi api(manager());
  api.Enable();

  // Now attach the view to the widget's tree. OnViewAddedToWidget should
  // recursively register the pre-existing virtual children.
  View* root = widget()->GetRootView();
  auto* container_ptr = root->AddChildView(std::move(container));

  EXPECT_EQ(
      api.cache()->Get(container_ptr->GetViewAccessibility().GetUniqueId()),
      &container_ptr->GetViewAccessibility());
  EXPECT_NE(api.cache()->Get(virtual_child_id), nullptr);
  EXPECT_NE(api.cache()->Get(nested_virtual_id), nullptr);
}

TEST_F(WidgetAXManagerTest, VirtualChildrenRemovedFromCacheWhenViewDetached) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  View* root = widget()->GetRootView();
  auto container = std::make_unique<View>();
  auto virtual_child = std::make_unique<AXVirtualView>();
  auto nested_virtual = std::make_unique<AXVirtualView>();

  auto virtual_child_id = virtual_child->ViewAccessibility::GetUniqueId();
  auto nested_virtual_id = nested_virtual->ViewAccessibility::GetUniqueId();

  virtual_child->AddChildView(std::move(nested_virtual));
  container->GetViewAccessibility().AddVirtualChildView(
      std::move(virtual_child));

  auto* container_ptr = root->AddChildView(std::move(container));
  auto container_id = container_ptr->GetViewAccessibility().GetUniqueId();

  // Verify they are all cached.
  ASSERT_NE(api.cache()->Get(container_id), nullptr);
  ASSERT_NE(api.cache()->Get(virtual_child_id), nullptr);
  ASSERT_NE(api.cache()->Get(nested_virtual_id), nullptr);

  // Detach the view from the widget.
  root->RemoveChildViewT(container_ptr);

  // All should be removed from the cache.
  EXPECT_EQ(api.cache()->Get(container_id), nullptr);
  EXPECT_EQ(api.cache()->Get(virtual_child_id), nullptr);
  EXPECT_EQ(api.cache()->Get(nested_virtual_id), nullptr);
}

TEST_F(WidgetAXManagerTest, FocusTracking_InitiallyNoFocus) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  EXPECT_EQ(manager()->GetFocusedNodeId(), ui::kInvalidAXNodeID);
}

TEST_F(WidgetAXManagerTest, FocusTracking_TracksFocusedView) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  auto* v1 = AddFocusableView();
  auto* v2 = AddFocusableView();

  api.WaitForNextSerialization();

  widget()->Show();
  v1->RequestFocus();

  EXPECT_EQ(manager()->GetFocusedNodeId(),
            static_cast<ui::AXNodeID>(
                v1->GetViewAccessibility().GetUniqueId()));

  v2->RequestFocus();
  EXPECT_EQ(manager()->GetFocusedNodeId(),
            static_cast<ui::AXNodeID>(
                v2->GetViewAccessibility().GetUniqueId()));
}

TEST_F(WidgetAXManagerTest, FocusTracking_ClearsFocusOnBlur) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  auto* v = AddFocusableView();

  api.WaitForNextSerialization();

  widget()->Show();
  v->RequestFocus();
  ASSERT_NE(manager()->GetFocusedNodeId(), ui::kInvalidAXNodeID);

  widget()->GetFocusManager()->ClearFocus();
  EXPECT_EQ(manager()->GetFocusedNodeId(), ui::kInvalidAXNodeID);
}

TEST_F(WidgetAXManagerTest, FocusTracking_SchedulesPendingUpdate) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  auto* v = AddFocusableView();

  api.WaitForNextSerialization();
  ASSERT_FALSE(api.processing_update_posted());

  widget()->Show();
  v->RequestFocus();
  EXPECT_TRUE(api.processing_update_posted());
}

TEST_F(WidgetAXManagerTest, FocusTracking_DoesNotResolveActiveDescendant) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  auto* parent = AddFocusableView();
  auto* child = parent->AddChildView(std::make_unique<View>());

  api.WaitForNextSerialization();

  parent->GetViewAccessibility().SetActiveDescendant(*child);

  widget()->Show();
  parent->RequestFocus();

  // focus_id should point to the focused VIEW, not its active descendant.
  // The BAM resolves active descendants itself via GetActiveDescendant()
  // in GetFocusFromThisOrDescendantFrame().
  EXPECT_EQ(manager()->GetFocusedNodeId(),
            static_cast<ui::AXNodeID>(
                parent->GetViewAccessibility().GetUniqueId()));
}

TEST_F(WidgetAXManagerTest, FocusTracking_HandlesManagerDestruction) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  auto* v = AddFocusableView();

  api.WaitForNextSerialization();

  widget()->Show();
  v->RequestFocus();
  ASSERT_NE(manager()->GetFocusedNodeId(), ui::kInvalidAXNodeID);

  // Simulate FocusManager destruction by calling the listener callback
  // directly, then verify focused_node_id_ is cleared.
  manager()->OnFocusManagerDestroying(widget()->GetFocusManager());
  EXPECT_EQ(manager()->GetFocusedNodeId(), ui::kInvalidAXNodeID);
}

TEST_F(WidgetAXManagerTest, FocusTracking_FocusIdInTreeData) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  auto* v = AddFocusableView();
  api.WaitForNextSerialization();

  widget()->Show();
  v->RequestFocus();
  api.WaitForNextSerialization();

  // Find the update that contains tree data with the focus_id.
  bool found_focus_id = false;
  const ui::AXNodeID expected_id =
      static_cast<ui::AXNodeID>(v->GetViewAccessibility().GetUniqueId());
  for (const auto& update : api.last_serialization().updates) {
    if (update.has_tree_data && update.tree_data.focus_id == expected_id) {
      found_focus_id = true;
      break;
    }
  }
  EXPECT_TRUE(found_focus_id);
}

TEST_F(WidgetAXManagerTest, FocusTracking_FocusIdUpdatesOnFocusChange) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  auto* v1 = AddFocusableView();
  auto* v2 = AddFocusableView();
  api.WaitForNextSerialization();

  widget()->Show();

  // Focus v1.
  v1->RequestFocus();
  api.WaitForNextSerialization();

  const ui::AXNodeID v1_id =
      static_cast<ui::AXNodeID>(v1->GetViewAccessibility().GetUniqueId());
  bool found_v1 = false;
  for (const auto& update : api.last_serialization().updates) {
    if (update.has_tree_data && update.tree_data.focus_id == v1_id) {
      found_v1 = true;
      break;
    }
  }
  EXPECT_TRUE(found_v1);

  // Move focus to v2.
  v2->RequestFocus();
  api.WaitForNextSerialization();

  const ui::AXNodeID v2_id =
      static_cast<ui::AXNodeID>(v2->GetViewAccessibility().GetUniqueId());
  bool found_v2 = false;
  for (const auto& update : api.last_serialization().updates) {
    if (update.has_tree_data && update.tree_data.focus_id == v2_id) {
      found_v2 = true;
      break;
    }
  }
  EXPECT_TRUE(found_v2);
}

TEST_F(WidgetAXManagerTest, TransientFocus_SerializesBeforeRestoredFocus) {
  struct FocusEvent {
    ui::AXNodeID id;
    std::string name;
  };

  WidgetAXManagerTestApi api(manager());
  api.Enable();

  auto* root = widget()->GetRootView();
  auto* focused_view = AddFocusableView();
  api.WaitForNextSerialization();

  widget()->Show();

  const ui::AXNodeID root_id =
      static_cast<ui::AXNodeID>(root->GetViewAccessibility().GetUniqueId());
  const ui::AXNodeID focused_view_id = static_cast<ui::AXNodeID>(
      focused_view->GetViewAccessibility().GetUniqueId());

  std::vector<FocusEvent> focus_events;
  ui::AXTreeManager::SetFocusChangeCallbackForTesting(base::BindRepeating(
      [](std::vector<FocusEvent>* focus_events,
         ui::BrowserAccessibilityManager* manager) {
        ui::BrowserAccessibility* focus = manager->GetFocus();
        focus_events->push_back({focus ? focus->GetId() : ui::kInvalidAXNodeID,
                                 focus ? focus->GetStringAttribute(
                                             ax::mojom::StringAttribute::kName)
                                       : std::string()});
      },
      &focus_events, api.ax_tree_manager()));
  base::ScopedClosureRunner reset_focus_callback(base::BindOnce(
      []() { ui::AXTreeManager::SetFocusChangeCallbackForTesting({}); }));

  constexpr char kUpdatedTitle[] = "Updated window title";
  root->GetViewAccessibility().SetName(kUpdatedTitle);
  root->GetViewAccessibility().NotifyTransientFocus();

  bool found_context_focus_id = false;
  bool found_updated_root = false;
  for (const auto& update : api.last_serialization().updates) {
    if (update.has_tree_data && update.tree_data.focus_id == root_id) {
      found_context_focus_id = true;
    }
    for (const auto& node : update.nodes) {
      if (node.id == root_id &&
          node.GetStringAttribute(ax::mojom::StringAttribute::kName) ==
              kUpdatedTitle) {
        found_updated_root = true;
      }
    }
  }
  EXPECT_TRUE(found_context_focus_id);
  EXPECT_TRUE(found_updated_root);
  EXPECT_TRUE(api.last_serialization().events.empty());
  ASSERT_EQ(focus_events.size(), 1u);
  EXPECT_EQ(focus_events[0].id, root_id);
  EXPECT_EQ(focus_events[0].name, kUpdatedTitle);
  EXPECT_EQ(api.ax_tree_manager()->GetTreeData().focus_id, root_id);

  focused_view->RequestFocus();
  api.WaitForNextSerialization();

  ASSERT_GE(focus_events.size(), 2u);
  EXPECT_EQ(focus_events[0].id, root_id);
  EXPECT_EQ(focus_events[1].id, focused_view_id);
  EXPECT_EQ(api.ax_tree_manager()->GetTreeData().focus_id, focused_view_id);
}

TEST_F(WidgetAXManagerTest, TransientFocus_SerializesVirtualView) {
  auto* root = widget()->GetRootView();
  auto* container = root->AddChildView(std::make_unique<View>());
  auto virtual_context = std::make_unique<AXVirtualView>();
  auto* virtual_context_ptr = virtual_context.get();
  virtual_context_ptr->SetRole(ax::mojom::Role::kGroup);
  virtual_context_ptr->SetName("Virtual context");
  const ui::AXNodeID virtual_context_id = static_cast<ui::AXNodeID>(
      virtual_context_ptr->ViewAccessibility::GetUniqueId());
  container->GetViewAccessibility().AddVirtualChildView(
      std::move(virtual_context));

  WidgetAXManagerTestApi api(manager());
  api.Enable();

  constexpr char kUpdatedContextName[] = "Updated virtual context";
  virtual_context_ptr->SetName(kUpdatedContextName);
  virtual_context_ptr->NotifyTransientFocus();

  bool found_virtual_focus_id = false;
  bool found_updated_virtual_context = false;
  for (const auto& update : api.last_serialization().updates) {
    if (update.has_tree_data &&
        update.tree_data.focus_id == virtual_context_id) {
      found_virtual_focus_id = true;
    }
    for (const auto& node : update.nodes) {
      if (node.id == virtual_context_id &&
          node.GetStringAttribute(ax::mojom::StringAttribute::kName) ==
              kUpdatedContextName) {
        found_updated_virtual_context = true;
      }
    }
  }
  EXPECT_TRUE(found_virtual_focus_id);
  EXPECT_TRUE(found_updated_virtual_context);
  EXPECT_TRUE(api.last_serialization().events.empty());
  EXPECT_EQ(api.ax_tree_manager()->GetTreeData().focus_id, virtual_context_id);
}

TEST_F(WidgetAXManagerTest, TextSelection_PopulatesTreeData) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  auto* v = widget()->GetRootView()->AddChildView(std::make_unique<View>());
  v->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  v->GetViewAccessibility().SetRole(ax::mojom::Role::kTextField);
  v->GetViewAccessibility().SetTextSelStart(2);
  v->GetViewAccessibility().SetTextSelEnd(5);
  api.WaitForNextSerialization();

  widget()->Show();
  v->RequestFocus();
  api.WaitForNextSerialization();

  const ui::AXNodeID v_id =
      static_cast<ui::AXNodeID>(v->GetViewAccessibility().GetUniqueId());
  bool found_selection = false;
  for (const auto& update : api.last_serialization().updates) {
    if (update.has_tree_data &&
        update.tree_data.sel_anchor_object_id == v_id &&
        update.tree_data.sel_focus_object_id == v_id &&
        !update.tree_data.sel_is_backward &&
        update.tree_data.sel_anchor_offset == 2 &&
        update.tree_data.sel_focus_offset == 5) {
      found_selection = true;
      break;
    }
  }
  EXPECT_TRUE(found_selection);
}

TEST_F(WidgetAXManagerTest, TextSelection_PopulatesBackwardTreeData) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  auto* v = widget()->GetRootView()->AddChildView(std::make_unique<View>());
  v->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  v->GetViewAccessibility().SetRole(ax::mojom::Role::kTextField);
  v->GetViewAccessibility().SetName(u"Selection text field");
  v->GetViewAccessibility().SetTextSelStart(5);
  v->GetViewAccessibility().SetTextSelEnd(2);
  api.WaitForNextSerialization();

  widget()->Show();
  v->RequestFocus();
  api.WaitForNextSerialization();

  const ui::AXNodeID v_id =
      static_cast<ui::AXNodeID>(v->GetViewAccessibility().GetUniqueId());
  bool found_selection = false;
  for (const auto& update : api.last_serialization().updates) {
    if (update.has_tree_data && update.tree_data.sel_anchor_object_id == v_id &&
        update.tree_data.sel_focus_object_id == v_id &&
        update.tree_data.sel_is_backward &&
        update.tree_data.sel_anchor_offset == 5 &&
        update.tree_data.sel_focus_offset == 2) {
      found_selection = true;
      break;
    }
  }
  EXPECT_TRUE(found_selection);
}

TEST_F(WidgetAXManagerTest, TextSelection_NotPopulatedWhenNoSelAttributes) {
  WidgetAXManagerTestApi api(manager());
  api.Enable();

  auto* v = widget()->GetRootView()->AddChildView(std::make_unique<View>());
  v->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  api.WaitForNextSerialization();

  widget()->Show();
  v->RequestFocus();
  api.WaitForNextSerialization();

  for (const auto& update : api.last_serialization().updates) {
    if (update.has_tree_data) {
      EXPECT_EQ(update.tree_data.sel_anchor_object_id, ui::kInvalidAXNodeID);
      EXPECT_EQ(update.tree_data.sel_focus_object_id, ui::kInvalidAXNodeID);
    }
  }
}

TEST_F(WidgetAXManagerTest,
       TextSelection_FiresTextfieldSelectionEventForViews) {
  enum class FiredEvent {
    kFocus,
    kDocumentSelectionChanged,
    kTextSelectionChanged,
  };

  WidgetAXManagerTestApi api(manager());
  api.Enable();

  auto* v = widget()->GetRootView()->AddChildView(std::make_unique<View>());
  v->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  v->GetViewAccessibility().SetRole(ax::mojom::Role::kTextField);
  v->GetViewAccessibility().SetName(u"Selection text field");
  v->GetViewAccessibility().SetTextSelStart(0);
  v->GetViewAccessibility().SetTextSelEnd(0);
  api.WaitForNextSerialization();

  std::vector<FiredEvent> fired_events;
  ui::AXTreeManager::SetFocusChangeCallbackForTesting(base::BindRepeating(
      [](std::vector<FiredEvent>* fired_events) {
        fired_events->push_back(FiredEvent::kFocus);
      },
      &fired_events));
  base::ScopedClosureRunner reset_focus_callback(base::BindOnce(
      []() { ui::AXTreeManager::SetFocusChangeCallbackForTesting({}); }));

  api.ax_tree_manager()->SetGeneratedEventCallbackForTesting(
      base::BindRepeating(
          [](std::vector<FiredEvent>* fired_events,
             ui::BrowserAccessibilityManager*,
             ui::AXEventGenerator::Event event_type, ui::AXNodeID) {
            if (event_type ==
                ui::AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED) {
              fired_events->push_back(FiredEvent::kDocumentSelectionChanged);
            } else if (event_type ==
                       ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED) {
              fired_events->push_back(FiredEvent::kTextSelectionChanged);
            }
          },
          &fired_events));

  widget()->Show();
  v->RequestFocus();
  api.WaitForNextSerialization();
  EXPECT_EQ(fired_events,
            (std::vector<FiredEvent>{FiredEvent::kFocus,
                                     FiredEvent::kTextSelectionChanged}));

  fired_events.clear();
  v->GetViewAccessibility().SetTextSelStart(2);
  v->GetViewAccessibility().SetTextSelEnd(5);
  api.WaitForNextSerialization();
  EXPECT_EQ(fired_events,
            (std::vector<FiredEvent>{FiredEvent::kTextSelectionChanged}));

  widget()->GetFocusManager()->ClearFocus();
  api.WaitForNextSerialization();

  fired_events.clear();
  v->GetViewAccessibility().SetTextSelStart(1);
  v->GetViewAccessibility().SetTextSelEnd(1);
  api.WaitForNextSerialization();
  EXPECT_TRUE(fired_events.empty());
}

}  // namespace views::test
