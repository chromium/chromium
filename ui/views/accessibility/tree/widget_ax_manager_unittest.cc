// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/tree/widget_ax_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
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

 private:
  WidgetAutoclosePtr widget_;

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAccessibilityTreeForViews};
};

TEST_F(WidgetAXManagerTest, InitiallyDisabled) {
  EXPECT_FALSE(manager()->is_enabled());
}

TEST_F(WidgetAXManagerTest, EnableSetsEnabled) {
  manager()->Enable();
  EXPECT_TRUE(manager()->is_enabled());
}

TEST_F(WidgetAXManagerTest, IsEnabledAfterAXModeAdded) {
  // Initially, the manager should not be enabled.
  ASSERT_FALSE(manager()->is_enabled());

  // Simulate that AXMode with kNativeAPIs was added.
  ui::AXPlatform::GetInstance().NotifyModeAdded(ui::AXMode::kNativeAPIs);
  EXPECT_TRUE(manager()->is_enabled());
}

TEST_F(WidgetAXManagerTest, EnableInitializesBrowserAccessibilityManager) {
  WidgetAXManagerTestApi test_api(manager());

  EXPECT_EQ(test_api.ax_tree_manager(), nullptr);
  manager()->Enable();
  EXPECT_NE(test_api.ax_tree_manager(), nullptr);
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
  manager()->Enable();

  EXPECT_TRUE(api.pending_events().empty());
  EXPECT_TRUE(api.pending_data_updates().empty());
  EXPECT_FALSE(api.processing_update_posted());

  auto* v1 = widget()->GetRootView()->AddChildView(std::make_unique<View>());
  auto* v2 = widget()->GetRootView()->AddChildView(std::make_unique<View>());

  task_environment()->RunUntilIdle();

  // Fire two events on v1, one on v2, before the first send.
  auto before = task_environment()->GetPendingMainThreadTaskCount();
  manager()->OnEvent(v1->GetViewAccessibility(), ax::mojom::Event::kFocus);
  manager()->OnEvent(v1->GetViewAccessibility(),
                     ax::mojom::Event::kValueChanged);
  manager()->OnEvent(v2->GetViewAccessibility(), ax::mojom::Event::kBlur);

  // Still just one task posted.
  EXPECT_EQ(task_environment()->GetPendingMainThreadTaskCount(), before + 1u);
  EXPECT_TRUE(api.processing_update_posted());

  // pending_events has three entries, pending_data_updates has two unique IDs.
  EXPECT_EQ(api.pending_events().size(), 3u);
  EXPECT_EQ(api.pending_data_updates().size(), 2u);

  // After run, everything clears.
  task_environment()->RunUntilIdle();
  EXPECT_EQ(api.pending_events().size(), 0u);
  EXPECT_EQ(api.pending_data_updates().size(), 0u);
  EXPECT_FALSE(api.processing_update_posted());
}

TEST_F(WidgetAXManagerTest, OnDataChanged_PostsSingleTaskAndQueuesCorrectly) {
  WidgetAXManagerTestApi api(manager());
  manager()->Enable();

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
  EXPECT_EQ(api.pending_data_updates().size(), 2u);

  // Duplicate data-change for v1 should not grow the set.
  before = task_environment()->GetPendingMainThreadTaskCount();
  manager()->OnDataChanged(v1->GetViewAccessibility());
  EXPECT_EQ(api.pending_data_updates().size(), 2u);
  EXPECT_EQ(task_environment()->GetPendingMainThreadTaskCount(), before);

  // After run, clear everything.
  task_environment()->RunUntilIdle();
  EXPECT_EQ(api.pending_data_updates().size(), 0u);
  EXPECT_FALSE(api.processing_update_posted());
}

TEST_F(WidgetAXManagerTest, OnEvent_CanScheduleAgainAfterSend) {
  WidgetAXManagerTestApi api(manager());
  manager()->Enable();

  auto v = ViewAccessibility::Create(nullptr);

  // First batch.
  manager()->OnEvent(*v, ax::mojom::Event::kFocus);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(api.processing_update_posted());
  EXPECT_TRUE(api.pending_events().empty());
  EXPECT_TRUE(api.pending_data_updates().empty());

  // Second batch.
  auto before = task_environment()->GetPendingMainThreadTaskCount();
  manager()->OnEvent(*v, ax::mojom::Event::kBlur);
  EXPECT_EQ(task_environment()->GetPendingMainThreadTaskCount(), before + 1u);
  EXPECT_TRUE(api.processing_update_posted());
  EXPECT_EQ(api.pending_events().size(), 1u);
  EXPECT_EQ(api.pending_data_updates().size(), 1u);
}

TEST_F(WidgetAXManagerTest, OnDataChanged_CanScheduleAgainAfterSend) {
  WidgetAXManagerTestApi api(manager());
  manager()->Enable();

  auto v = ViewAccessibility::Create(nullptr);

  // First batch.
  manager()->OnDataChanged(*v);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(api.processing_update_posted());
  EXPECT_TRUE(api.pending_events().empty());
  EXPECT_TRUE(api.pending_data_updates().empty());

  // Second batch.
  auto before = task_environment()->GetPendingMainThreadTaskCount();
  manager()->OnDataChanged(*v);
  EXPECT_EQ(task_environment()->GetPendingMainThreadTaskCount(), before + 1u);
  EXPECT_TRUE(api.processing_update_posted());
  EXPECT_EQ(api.pending_events().size(), 0u);
  EXPECT_EQ(api.pending_data_updates().size(), 1u);
}

TEST_F(WidgetAXManagerTest, UpdatesIgnoredWhenDisabled) {
  WidgetAXManagerTestApi api(manager());

  // Manager is disabled by default.
  auto v = ViewAccessibility::Create(nullptr);

  auto before = task_environment()->GetPendingMainThreadTaskCount();
  manager()->OnEvent(*v, ax::mojom::Event::kFocus);
  EXPECT_FALSE(api.processing_update_posted());
  EXPECT_TRUE(api.pending_events().empty());
  EXPECT_TRUE(api.pending_data_updates().empty());
  EXPECT_EQ(task_environment()->GetPendingMainThreadTaskCount(), before);

  manager()->OnDataChanged(*v);
  EXPECT_FALSE(api.processing_update_posted());
  EXPECT_TRUE(api.pending_data_updates().empty());
  EXPECT_EQ(task_environment()->GetPendingMainThreadTaskCount(), before);
}

// TODO: In a follow-up CL, this test should confirmt that only the root gets
// serialized.
TEST_F(WidgetAXManagerTest, SendPendingUpdate_NoAXTreeWhenDisabled) {
  WidgetAXManagerTestApi api(manager());
  EXPECT_EQ(api.ax_tree_manager(), nullptr);
}

TEST_F(WidgetAXManagerTest, SendPendingUpdate_SerializationOnEnable) {
  WidgetAXManagerTestApi api(manager());

  // On enable, the manager should serialize the root automatically.
  manager()->Enable();

  EXPECT_NE(api.ax_tree_manager(), nullptr);
  EXPECT_EQ(api.ax_tree_manager()->ax_tree()->root()->id(),
            static_cast<int32_t>(
                widget()->GetRootView()->GetViewAccessibility().GetUniqueId()));

  // TODO: In a follow-up CL, the root should be serialized on class creation,
  // not on enable. The rest of the tree should be serialized on enable.
}

TEST_F(WidgetAXManagerTest,
       SendPendingUpdate_SerializationOnChildAddedAndRemoved) {
  WidgetAXManagerTestApi api(manager());

  manager()->Enable();

  EXPECT_EQ(api.ax_tree_manager()->ax_tree()->root()->id(),
            static_cast<int32_t>(
                widget()->GetRootView()->GetViewAccessibility().GetUniqueId()));
  EXPECT_EQ(api.ax_tree_manager()->ax_tree()->size(), 1);

  // Adding a child view should automatically call OnDataChanged, which in turn
  // should schedule a pending serialization.
  auto* child = widget()->GetRootView()->AddChildView(std::make_unique<View>());
  ui::AXNodeID child_id =
      static_cast<ui::AXNodeID>(child->GetViewAccessibility().GetUniqueId());
  task_environment()->RunUntilIdle();

  EXPECT_NE(api.ax_tree_manager()->ax_tree()->GetFromId(child_id), nullptr);

  // Removing a child view should also schedule a pending serialization.
  widget()->GetRootView()->RemoveChildViewT(child);
  task_environment()->RunUntilIdle();

  EXPECT_EQ(api.ax_tree_manager()->ax_tree()->GetFromId(child_id), nullptr);
}

TEST_F(WidgetAXManagerTest, SendPendingUpdate_SerializeOnEvent) {
  // This is far from complete, but it should at least confirm that fired events
  // lead to serialization and are themselves serialized.
  // TODO(https://crbug.com/40672441): Replace this test by a new dump event
  // test frameworks for views.
  base::HistogramTester histogram_tester;

  WidgetAXManagerTestApi api(manager());

  std::string histogram_name =
      "Accessibility.Performance.BrowserAccessibilityManager::"
      "OnAccessibilityEvents2";
  manager()->Enable();

  histogram_tester.ExpectTotalCount(histogram_name, 0);

  manager()->OnEvent(widget()->GetRootView()->GetViewAccessibility(),
                     ax::mojom::Event::kLoadComplete);
  task_environment()->RunUntilIdle();

  histogram_tester.ExpectTotalCount(histogram_name, 1);

  EXPECT_NE(
      api.ax_tree_manager()->ax_tree()->GetFromId(static_cast<ui::AXNodeID>(
          widget()->GetRootView()->GetViewAccessibility().GetUniqueId())),
      nullptr);
}

TEST_F(WidgetAXManagerTest, SendPendingUpdate_NoSerializeWhenNodeNotInTree) {
  WidgetAXManagerTestApi api(manager());
  manager()->Enable();

  // This view is not part of the widget.
  auto v = ViewAccessibility::Create(nullptr);

  manager()->OnDataChanged(*v.get());
  task_environment()->RunUntilIdle();

  EXPECT_EQ(api.ax_tree_manager()->ax_tree()->GetFromId(
                static_cast<ui::AXNodeID>(v->GetUniqueId())),
            nullptr);
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

  // Newly created widget is inactive by default.
  EXPECT_FALSE(widget()->IsActive());
  EXPECT_FALSE(manager()->CanFireAccessibilityEvents());

  // Once activated, it should return true.
  widget()->Activate();
  EXPECT_TRUE(widget()->IsActive());
  EXPECT_TRUE(manager()->CanFireAccessibilityEvents());
}

TEST_F(WidgetAXManagerTest, GetOrCreateAXNodeUniqueId) {
  auto v = ViewAccessibility::Create(nullptr);

  WidgetAXManagerTestApi test_api(manager());
  ASSERT_FALSE(test_api.cache()->HasCachedChildren(v.get()));
  EXPECT_EQ(manager()->GetOrCreateAXNodeUniqueId(v->GetUniqueId()),
            ui::AXUniqueId::CreateInvalid());

  test_api.cache()->Insert(v.get());

  EXPECT_EQ(manager()->GetOrCreateAXNodeUniqueId(v->GetUniqueId()),
            v->GetUniqueId());
}

}  // namespace views::test
