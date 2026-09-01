// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/browser_accessibility_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/apple/foundation_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_platform_node_cocoa.h"
#include "ui/accessibility/platform/ax_platform_node_unittest.h"
#include "ui/accessibility/platform/ax_private_webkit_constants_mac.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/browser_accessibility_manager_mac.h"
#include "ui/accessibility/platform/test_ax_node_id_delegate.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"
#include "ui/accessibility/platform/test_ax_platform_tree_manager_delegate.h"

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

  NSArray<NSString*>* LegacyActionNames(const AXNodeData& data) {
    Init(data);
    TestAXNodeWrapper* wrapper =
        TestAXNodeWrapper::GetOrCreate(GetTree(), GetRoot());
    AXPlatformNodeCocoa* node =
        base::apple::ObjCCastStrict<AXPlatformNodeCocoa>(
            wrapper->ax_platform_node()->GetNativeViewAccessible().Get());
    NSArray<NSString*>* actions =
        [NSArray arrayWithArray:[node internalAccessibilityActionNames]];
    TestAXNodeWrapper::ResetGlobalState();
    DestroyTree();
    return actions;
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

  TestAXNodeIdDelegate node_id_delegate;
  std::unique_ptr<MockBrowserAccessibilityManagerMac> mock_manager =
      std::make_unique<MockBrowserAccessibilityManagerMac>(
          update, node_id_delegate, nullptr);

  BrowserAccessibility* accessibility =
      mock_manager->GetBrowserAccessibilityRoot();
  BrowserAccessibilityCocoa* node =
      base::apple::ObjCCastStrict<BrowserAccessibilityCocoa>(
          accessibility->GetNativeViewAccessible().Get());

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

  TestAXNodeIdDelegate node_id_delegate;
  std::unique_ptr<MockBrowserAccessibilityManagerMac> mock_manager =
      std::make_unique<MockBrowserAccessibilityManagerMac>(
          update, node_id_delegate, nullptr);

  BrowserAccessibility* accessibility =
      mock_manager->GetBrowserAccessibilityRoot();
  BrowserAccessibilityCocoa* node =
      base::apple::ObjCCastStrict<BrowserAccessibilityCocoa>(
          accessibility->GetNativeViewAccessible().Get());

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

TEST_P(BrowserAccessibilityCocoaTest, AXPressAdvertisementMatchesExecution) {
  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kListBox;
  root_data.child_ids = {2, 3};

  AXNodeData data_2;
  data_2.id = 2;
  data_2.role = ax::mojom::Role::kListBoxOption;

  AXNodeData data_3;
  data_3.id = 3;
  data_3.role = ax::mojom::Role::kListBoxOption;
  data_3.SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kOpen);

  AXTreeUpdate update;
  update.root_id = root_data.id;
  update.nodes = {root_data, data_2, data_3};

  TestAXNodeIdDelegate node_id_delegate;
  auto manager = std::make_unique<MockBrowserAccessibilityManagerMac>(
      update, node_id_delegate, nullptr);
  BrowserAccessibility* node_2 = manager->GetFromID(data_2.id);
  BrowserAccessibility* node_3 = manager->GetFromID(data_3.id);
  ASSERT_NE(node_2, nullptr);
  ASSERT_NE(node_3, nullptr);
  EXPECT_TRUE(node_2->IsClickable());
  EXPECT_TRUE(node_3->IsClickable());

  BrowserAccessibilityCocoa* cocoa_node_2 =
      base::apple::ObjCCastStrict<BrowserAccessibilityCocoa>(
          node_2->GetNativeViewAccessible().Get());
  BrowserAccessibilityCocoa* cocoa_node_3 =
      base::apple::ObjCCastStrict<BrowserAccessibilityCocoa>(
          node_3->GetNativeViewAccessible().Get());

  EXPECT_FALSE([[cocoa_node_2 internalAccessibilityActionNames]
      containsObject:NSAccessibilityPressAction]);
  EXPECT_TRUE([[cocoa_node_3 internalAccessibilityActionNames]
      containsObject:NSAccessibilityPressAction]);

  EXPECT_CALL(*manager, DoDefaultAction(::testing::Ref(*node_3)));
  EXPECT_FALSE([cocoa_node_2 accessibilityPerformPress]);
  EXPECT_TRUE([cocoa_node_3 accessibilityPerformPress]);
}

TEST_P(BrowserAccessibilityCocoaTest, ViewsShowMenuUsesSerializedCapabilities) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kGroup;
  AXTreeUpdate update;
  update.root_id = root.id;
  update.nodes.push_back(root);

  TestAXNodeIdDelegate node_id_delegate;
  TestAXPlatformTreeManagerDelegate delegate;
  delegate.is_web_content_source_ = false;
  auto manager = std::make_unique<BrowserAccessibilityManagerMac>(
      update, node_id_delegate, &delegate);
  BrowserAccessibility* root_node = manager->GetBrowserAccessibilityRoot();
  BrowserAccessibilityCocoa* node =
      base::apple::ObjCCastStrict<BrowserAccessibilityCocoa>(
          root_node->GetNativeViewAccessible().Get());

  EXPECT_FALSE([[node internalAccessibilityActionNames]
      containsObject:NSAccessibilityShowMenuAction]);
  EXPECT_FALSE([node accessibilityPerformShowMenu]);

  root.AddAction(ax::mojom::Action::kShowContextMenu);
  root_node->node()->SetData(root);

  EXPECT_TRUE([[node internalAccessibilityActionNames]
      containsObject:NSAccessibilityShowMenuAction]);
  EXPECT_TRUE([node accessibilityPerformShowMenu]);
}

TEST_P(BrowserAccessibilityCocoaTest, WebContentRetainsShowMenuAction) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kGroup;
  AXTreeUpdate update;
  update.root_id = root.id;
  update.nodes.push_back(root);

  TestAXNodeIdDelegate node_id_delegate;
  TestAXPlatformTreeManagerDelegate delegate;
  delegate.is_web_content_source_ = true;
  auto manager = std::make_unique<BrowserAccessibilityManagerMac>(
      update, node_id_delegate, &delegate);
  BrowserAccessibilityCocoa* node =
      base::apple::ObjCCastStrict<BrowserAccessibilityCocoa>(
          manager->GetBrowserAccessibilityRoot()
              ->GetNativeViewAccessible()
              .Get());

  EXPECT_TRUE([[node internalAccessibilityActionNames]
      containsObject:NSAccessibilityShowMenuAction]);
  EXPECT_TRUE([node accessibilityPerformShowMenu]);
}

TEST_P(BrowserAccessibilityCocoaTest, ViewsPopupShowMenuUsesDefaultAction) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kPopUpButton;
  root.SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kOpen);
  AXTreeUpdate update;
  update.root_id = root.id;
  update.nodes.push_back(root);

  TestAXNodeIdDelegate node_id_delegate;
  TestAXPlatformTreeManagerDelegate delegate;
  delegate.is_web_content_source_ = false;
  auto manager = std::make_unique<MockBrowserAccessibilityManagerMac>(
      update, node_id_delegate, &delegate);
  BrowserAccessibility* root_node = manager->GetBrowserAccessibilityRoot();
  BrowserAccessibilityCocoa* node =
      base::apple::ObjCCastStrict<BrowserAccessibilityCocoa>(
          root_node->GetNativeViewAccessible().Get());

  EXPECT_TRUE([[node internalAccessibilityActionNames]
      containsObject:NSAccessibilityShowMenuAction]);
  EXPECT_CALL(*manager, DoDefaultAction(::testing::Ref(*root_node)));
  EXPECT_TRUE([node accessibilityPerformShowMenu]);
}

TEST_P(BrowserAccessibilityCocoaTest, ScrollToVisibleMatchesSource) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kGroup;
  root.AddAction(ax::mojom::Action::kScrollToMakeVisible);
  AXTreeUpdate update;
  update.root_id = root.id;
  update.nodes.push_back(root);

  TestAXNodeIdDelegate node_id_delegate;
  TestAXPlatformTreeManagerDelegate views_delegate;
  views_delegate.is_web_content_source_ = false;
  auto views_manager = std::make_unique<BrowserAccessibilityManagerMac>(
      update, node_id_delegate, &views_delegate);
  BrowserAccessibilityCocoa* views_node =
      base::apple::ObjCCastStrict<BrowserAccessibilityCocoa>(
          views_manager->GetBrowserAccessibilityRoot()
              ->GetNativeViewAccessible()
              .Get());

  EXPECT_FALSE([[views_node internalAccessibilityActionNames]
      containsObject:NSAccessibilityScrollToVisibleAction]);
  EXPECT_FALSE([LegacyActionNames(root)
      containsObject:NSAccessibilityScrollToVisibleAction]);

  TestAXNodeIdDelegate web_node_id_delegate;
  TestAXPlatformTreeManagerDelegate web_delegate;
  web_delegate.is_web_content_source_ = true;
  auto web_manager = std::make_unique<BrowserAccessibilityManagerMac>(
      update, web_node_id_delegate, &web_delegate);
  BrowserAccessibilityCocoa* web_node =
      base::apple::ObjCCastStrict<BrowserAccessibilityCocoa>(
          web_manager->GetBrowserAccessibilityRoot()
              ->GetNativeViewAccessible()
              .Get());

  EXPECT_TRUE([[web_node internalAccessibilityActionNames]
      containsObject:NSAccessibilityScrollToVisibleAction]);
}

TEST_P(BrowserAccessibilityCocoaTest, CancelActionMatchesSource) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kMenuItem;
  AXTreeUpdate update;
  update.root_id = root.id;
  update.nodes.push_back(root);

  TestAXNodeIdDelegate node_id_delegate;
  TestAXPlatformTreeManagerDelegate views_delegate;
  views_delegate.is_web_content_source_ = false;
  auto views_manager = std::make_unique<BrowserAccessibilityManagerMac>(
      update, node_id_delegate, &views_delegate);
  BrowserAccessibilityCocoa* views_node =
      base::apple::ObjCCastStrict<BrowserAccessibilityCocoa>(
          views_manager->GetBrowserAccessibilityRoot()
              ->GetNativeViewAccessible()
              .Get());

  EXPECT_FALSE([[views_node internalAccessibilityActionNames]
      containsObject:NSAccessibilityCancelAction]);
  EXPECT_FALSE(
      [LegacyActionNames(root) containsObject:NSAccessibilityCancelAction]);

  TestAXNodeIdDelegate web_node_id_delegate;
  TestAXPlatformTreeManagerDelegate web_delegate;
  web_delegate.is_web_content_source_ = true;
  auto web_manager = std::make_unique<BrowserAccessibilityManagerMac>(
      update, web_node_id_delegate, &web_delegate);
  BrowserAccessibilityCocoa* web_node =
      base::apple::ObjCCastStrict<BrowserAccessibilityCocoa>(
          web_manager->GetBrowserAccessibilityRoot()
              ->GetNativeViewAccessible()
              .Get());

  EXPECT_TRUE([[web_node internalAccessibilityActionNames]
      containsObject:NSAccessibilityCancelAction]);
}
}  // namespace ui
