// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/tree/widget_ax_manager.h"

#include <memory>
#include <utility>

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

class WidgetAXManagerScheduleTest : public testing::Test {
 public:
  void SetUp() override {
    manager_ = std::make_unique<WidgetAXManager>(/*widget=*/nullptr);
  }

  base::test::SingleThreadTaskEnvironment& task_env() { return task_env_; }
  WidgetAXManager* manager() { return manager_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_env_;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAccessibilityTreeForViews};
  std::unique_ptr<WidgetAXManager> manager_;
};

TEST_F(WidgetAXManagerScheduleTest, OnEvent_PostsSingleTaskAndQueuesCorrectly) {
  WidgetAXManagerTestApi api(manager());
  manager()->Enable();

  // Nothing pending initially.
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 0u);
  EXPECT_TRUE(api.pending_events().empty());
  EXPECT_TRUE(api.pending_data_updates().empty());
  EXPECT_FALSE(api.processing_update_posted());

  auto v1 = ViewAccessibility::Create(nullptr);
  auto v2 = ViewAccessibility::Create(nullptr);

  // Fire two events on v1, one on v2, before the first send.
  manager()->OnEvent(*v1, ax::mojom::Event::kFocus);
  manager()->OnEvent(*v1, ax::mojom::Event::kValueChanged);
  manager()->OnEvent(*v2, ax::mojom::Event::kBlur);

  // Still just one task posted.
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 1u);
  EXPECT_TRUE(api.processing_update_posted());

  // pending_events has three entries, pending_data_updates has two unique IDs.
  EXPECT_EQ(api.pending_events().size(), 3u);
  EXPECT_EQ(api.pending_data_updates().size(), 2u);

  // After run, everything clears.
  task_env().RunUntilIdle();
  EXPECT_EQ(api.pending_events().size(), 0u);
  EXPECT_EQ(api.pending_data_updates().size(), 0u);
  EXPECT_FALSE(api.processing_update_posted());
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(WidgetAXManagerScheduleTest,
       OnDataChanged_PostsSingleTaskAndQueuesCorrectly) {
  WidgetAXManagerTestApi api(manager());
  manager()->Enable();

  // Nothing pending initially.
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 0u);
  EXPECT_TRUE(api.pending_events().empty());
  EXPECT_TRUE(api.pending_data_updates().empty());
  EXPECT_FALSE(api.processing_update_posted());

  auto v1 = ViewAccessibility::Create(nullptr);
  auto v2 = ViewAccessibility::Create(nullptr);

  // Data-changes for both views.
  manager()->OnDataChanged(*v1);
  manager()->OnDataChanged(*v2);

  // One task scheduled, two unique IDs in pending_data_updates.
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 1u);
  EXPECT_TRUE(api.processing_update_posted());
  EXPECT_EQ(api.pending_events().size(), 0u);
  EXPECT_EQ(api.pending_data_updates().size(), 2u);

  // Duplicate data-change for v1 should not grow the set.
  manager()->OnDataChanged(*v1);
  EXPECT_EQ(api.pending_data_updates().size(), 2u);
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 1u);

  // After run, clear everything.
  task_env().RunUntilIdle();
  EXPECT_EQ(api.pending_data_updates().size(), 0u);
  EXPECT_FALSE(api.processing_update_posted());
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(WidgetAXManagerScheduleTest, OnEvent_CanScheduleAgainAfterSend) {
  WidgetAXManagerTestApi api(manager());
  manager()->Enable();

  auto v = ViewAccessibility::Create(nullptr);

  // First batch.
  manager()->OnEvent(*v, ax::mojom::Event::kFocus);
  task_env().RunUntilIdle();
  EXPECT_FALSE(api.processing_update_posted());
  EXPECT_TRUE(api.pending_events().empty());
  EXPECT_TRUE(api.pending_data_updates().empty());

  // Second batch.
  manager()->OnEvent(*v, ax::mojom::Event::kBlur);
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 1u);
  EXPECT_TRUE(api.processing_update_posted());
  EXPECT_EQ(api.pending_events().size(), 1u);
  EXPECT_EQ(api.pending_data_updates().size(), 1u);
}

TEST_F(WidgetAXManagerScheduleTest, OnDataChanged_CanScheduleAgainAfterSend) {
  WidgetAXManagerTestApi api(manager());
  manager()->Enable();

  auto v = ViewAccessibility::Create(nullptr);

  // First batch.
  manager()->OnDataChanged(*v);
  task_env().RunUntilIdle();
  EXPECT_FALSE(api.processing_update_posted());
  EXPECT_TRUE(api.pending_events().empty());
  EXPECT_TRUE(api.pending_data_updates().empty());

  // Second batch.
  manager()->OnDataChanged(*v);
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 1u);
  EXPECT_TRUE(api.processing_update_posted());
  EXPECT_EQ(api.pending_events().size(), 0u);
  EXPECT_EQ(api.pending_data_updates().size(), 1u);
}

TEST_F(WidgetAXManagerScheduleTest, UpdatesIgnoredWhenDisabled) {
  WidgetAXManagerTestApi api(manager());

  // Manager is disabled by default.
  auto v = ViewAccessibility::Create(nullptr);

  manager()->OnEvent(*v, ax::mojom::Event::kFocus);
  EXPECT_FALSE(api.processing_update_posted());
  EXPECT_TRUE(api.pending_events().empty());
  EXPECT_TRUE(api.pending_data_updates().empty());
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 0u);

  manager()->OnDataChanged(*v);
  EXPECT_FALSE(api.processing_update_posted());
  EXPECT_TRUE(api.pending_data_updates().empty());
  EXPECT_EQ(task_env().GetPendingMainThreadTaskCount(), 0u);
}

}  // namespace views::test
