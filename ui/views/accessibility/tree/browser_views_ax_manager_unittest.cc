// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/tree/browser_views_ax_manager.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/ax_platform_for_test.h"

namespace views {
namespace {

class BrowserViewsAXManagerTest : public ::testing::Test {
 protected:
  BrowserViewsAXManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAccessibilityTreeForViews);
  }

  void SetUp() override {
    browser_views_ax_manager_handle_ = views::BrowserViewsAXManager::Create();
  }

  std::unique_ptr<views::BrowserViewsAXManager::LifetimeHandle>
      browser_views_ax_manager_handle_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BrowserViewsAXManagerTest, IsEnabledAfterAXModeAdded) {
  BrowserViewsAXManager* manager = BrowserViewsAXManager::GetInstance();
  ASSERT_NE(manager, nullptr);
  // Initially, the manager should not be enabled.
  EXPECT_FALSE(manager->is_enabled());

  // Simulate that AXMode with kNativeAPIs was added.
  ui::AXPlatform::GetInstance().NotifyModeAdded(ui::AXMode::kNativeAPIs);
  EXPECT_TRUE(manager->is_enabled());
}

// This death test verifies that Create() crashes (via CHECK) when the flag is
// off.
TEST_F(BrowserViewsAXManagerTest, CreateCrashesWhenFlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAccessibilityTreeForViews);
  EXPECT_DEATH(views::BrowserViewsAXManager::Create(), "");
}

TEST_F(BrowserViewsAXManagerTest, GetOrCreateAXNodeUniqueId) {
  BrowserViewsAXManager* manager = BrowserViewsAXManager::GetInstance();

  ASSERT_NE(manager, nullptr);
  int ax_node_id = 100;
  ui::AXPlatformNodeId id1 = manager->GetOrCreateAXNodeUniqueId(ax_node_id);
  ui::AXPlatformNodeId id2 = manager->GetOrCreateAXNodeUniqueId(ax_node_id);
  EXPECT_EQ(id1, id2);

  int another_ax_node_id = 200;
  ui::AXPlatformNodeId id3 =
      manager->GetOrCreateAXNodeUniqueId(another_ax_node_id);
  EXPECT_NE(id1, id3);
}

TEST_F(BrowserViewsAXManagerTest, OnAXNodeDeleted) {
  BrowserViewsAXManager* manager = BrowserViewsAXManager::GetInstance();

  ASSERT_NE(manager, nullptr);
  int ax_node_id = 300;
  ui::AXPlatformNodeId id1 = manager->GetOrCreateAXNodeUniqueId(ax_node_id);
  manager->OnAXNodeDeleted(ax_node_id);
  ui::AXPlatformNodeId id2 = manager->GetOrCreateAXNodeUniqueId(ax_node_id);
  EXPECT_NE(id1, id2);
}

}  // namespace
}  // namespace views
