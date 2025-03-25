// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/browser_accessibility_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_platform_node_unittest.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/browser_accessibility_manager_mac.h"
#include "ui/accessibility/platform/test_ax_node_id_delegate.h"

namespace {

struct FeatureState {
  bool mac_accessibility_api_migration_enabled = false;
};

class TestBrowserAccessibilityManager : public ui::BrowserAccessibilityManager {
 public:
  explicit TestBrowserAccessibilityManager(
      const ui::AXTreeUpdate& initial_tree,
      ui::AXNodeIdDelegate& node_id_delegate)
      : BrowserAccessibilityManager(node_id_delegate, nullptr) {
    Initialize(initial_tree);
  }
};

class MockBrowserAccessibilityManagerMac
    : public ui::BrowserAccessibilityManagerMac {
 public:
  MockBrowserAccessibilityManagerMac(
      ui::AXTreeUpdate& update,
      ui::TestAXNodeIdDelegate& node_id_delegate,
      ui::AXPlatformTreeManagerDelegate* delegate)
      : ui::BrowserAccessibilityManagerMac(update, node_id_delegate, delegate) {
  }

  MOCK_METHOD(void,
              DoDefaultAction,
              (const ui::BrowserAccessibility& node),
              (override));
};

}  // namespace

namespace ui {

// A test class for BrowserAccessibilityCocoa unit tests.
class BrowserAccessibilityCocoaTest
    : public ::testing::WithParamInterface<FeatureState>,
      public AXPlatformNodeTest {
 public:
  BrowserAccessibilityCocoaTest() {
    if (GetParam().mac_accessibility_api_migration_enabled) {
      base::FieldTrialParams params;
      params["MacAccessibilityAPIMigrationEnabled"] = "true";
      features_.InitAndEnableFeatureWithParameters(
          features::kMacAccessibilityAPIMigration, params);
    } else {
      features_.InitAndDisableFeature(features::kMacAccessibilityAPIMigration);
    }
  }

 private:
  base::test::ScopedFeatureList features_;
};

using BrowserAccessibilityCocoaTestOldAPI = BrowserAccessibilityCocoaTest;
using BrowserAccessibilityCocoaTestNewAPI = BrowserAccessibilityCocoaTest;

// Tests that should pass regardless of new or old Cocoa a11y API.
INSTANTIATE_TEST_SUITE_P(
    Common,
    BrowserAccessibilityCocoaTest,
    ::testing::Values(
        FeatureState{.mac_accessibility_api_migration_enabled = false},
        FeatureState{.mac_accessibility_api_migration_enabled = true}));

/*
 // Tests that should only pass with the old Cocoa a11y API.
 INSTANTIATE_TEST_SUITE_P(
 NoFeature,
 BrowserAccessibilityCocoaTestOldAPI,
 ::testing::Values(FeatureState{
 .mac_accessibility_api_migration_enabled = false}));

 // Tests that should only pass with the new Cocoa a11y API.
 INSTANTIATE_TEST_SUITE_P(MacAccessibilityAPIMigrationEnabled,
 BrowserAccessibilityCocoaTestNewAPI,
 ::testing::Values(FeatureState{
 .mac_accessibility_api_migration_enabled = true}));
 */

// Tests that accessibilityPerformAction: fires a node's default action.
TEST_P(BrowserAccessibilityCocoaTest, TestHasDefaultAction) {
  AXNodeData root = AXNodeData();
  root.id = 1;
  root.role = ax::mojom::Role::kCheckBox;
  root.SetCheckedState(ax::mojom::CheckedState::kFalse);
  root.SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kCheck);
  ui::AXTreeUpdate update;
  update.root_id = root.id;
  update.nodes.push_back(root);
  ui::AXTree tree(update);

  TestAXNodeIdDelegate node_id_delegate;
  std::unique_ptr<MockBrowserAccessibilityManagerMac> mock_manager =
      std::make_unique<MockBrowserAccessibilityManagerMac>(
          update, node_id_delegate, nullptr);

  std::unique_ptr<BrowserAccessibility> accessibility =
      BrowserAccessibility::Create(mock_manager.get(), tree.root());

  ui::AXPlatformNodeMac* platform_node = static_cast<ui::AXPlatformNodeMac*>(
      AXPlatformNodeMac::GetFromUniqueId(root.id));
  BrowserAccessibilityCocoa* node =
      [[BrowserAccessibilityCocoa alloc] initWithObject:accessibility.get()
                                       withPlatformNode:platform_node];

  EXPECT_CALL(*mock_manager, DoDefaultAction(::testing::Ref(*accessibility)))
      .Times(1);

  ASSERT_EQ(accessibility->node()->data().GetCheckedState(),
            ax::mojom::CheckedState::kFalse);

// TODO(https://crbug.com/406190900): Remove this deprecation pragma.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  [node accessibilityPerformAction:NSAccessibilityPressAction];
#pragma clang diagnostic pop

  EXPECT_EQ(accessibility->node()->data().GetCheckedState(),
            ax::mojom::CheckedState::kTrue);
}

// Tests that accessibilityPerformAction: does nothing if a node has no default
// action.
TEST_P(BrowserAccessibilityCocoaTest, TestNoDefaultAction) {
  AXNodeData root = AXNodeData();
  root.id = 1;
  root.role = ax::mojom::Role::kCheckBox;
  root.SetCheckedState(ax::mojom::CheckedState::kFalse);
  root.SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kNone);
  ui::AXTreeUpdate update;
  update.root_id = root.id;
  update.nodes.push_back(root);
  ui::AXTree tree(update);

  TestAXNodeIdDelegate node_id_delegate;
  std::unique_ptr<MockBrowserAccessibilityManagerMac> mock_manager =
      std::make_unique<MockBrowserAccessibilityManagerMac>(
          update, node_id_delegate, nullptr);

  std::unique_ptr<BrowserAccessibility> accessibility =
      BrowserAccessibility::Create(mock_manager.get(), tree.root());

  ui::AXPlatformNodeMac* platform_node = static_cast<ui::AXPlatformNodeMac*>(
      AXPlatformNodeMac::GetFromUniqueId(root.id));
  BrowserAccessibilityCocoa* node =
      [[BrowserAccessibilityCocoa alloc] initWithObject:accessibility.get()
                                       withPlatformNode:platform_node];

  EXPECT_CALL(*mock_manager, DoDefaultAction(::testing::Ref(*accessibility)))
      .Times(0);

  ASSERT_EQ(accessibility->node()->data().GetCheckedState(),
            ax::mojom::CheckedState::kFalse);

// TODO(https://crbug.com/406190900): Remove this deprecation pragma.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  [node accessibilityPerformAction:NSAccessibilityPressAction];
#pragma clang diagnostic pop

  EXPECT_EQ(accessibility->node()->data().GetCheckedState(),
            ax::mojom::CheckedState::kFalse);
}

// Tests that accessibilityPerformAction: does nothing if the
// BrowserAccessibility has no node.
TEST_P(BrowserAccessibilityCocoaTest, TestNoNodeForDefaultAction) {
  AXNodeData root = AXNodeData();
  root.id = 1;
  root.role = ax::mojom::Role::kCheckBox;
  root.SetCheckedState(ax::mojom::CheckedState::kFalse);
  root.SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kNone);
  ui::AXTreeUpdate update;
  update.root_id = root.id;
  update.nodes.push_back(root);
  ui::AXTree tree(update);

  TestAXNodeIdDelegate node_id_delegate;
  std::unique_ptr<MockBrowserAccessibilityManagerMac> mock_manager =
      std::make_unique<MockBrowserAccessibilityManagerMac>(
          update, node_id_delegate, nullptr);

  std::unique_ptr<BrowserAccessibility> accessibility =
      BrowserAccessibility::Create(mock_manager.get(), tree.root());

  ui::AXPlatformNodeMac* platform_node = static_cast<ui::AXPlatformNodeMac*>(
      AXPlatformNodeMac::GetFromUniqueId(root.id));
  BrowserAccessibilityCocoa* node =
      [[BrowserAccessibilityCocoa alloc] initWithObject:accessibility.get()
                                       withPlatformNode:platform_node];

  accessibility->reset_node();
  ASSERT_FALSE(accessibility->node());

  EXPECT_CALL(*mock_manager, DoDefaultAction(::testing::Ref(*accessibility)))
      .Times(0);

// TODO(https://crbug.com/406190900): Remove this deprecation pragma.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  [node accessibilityPerformAction:NSAccessibilityPressAction];
#pragma clang diagnostic pop
}
}  // namespace ui
