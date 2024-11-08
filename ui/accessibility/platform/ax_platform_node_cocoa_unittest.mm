// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_platform_node_unittest.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"

namespace {

struct FeatureState {
  bool mac_accessibility_api_migration_enabled = false;
};

}  // namespace

namespace ui {

// A test class for AXPlatformNodeCocoa unit tests.
class AXPlatformNodeCocoaTest
    : public ::testing::WithParamInterface<FeatureState>,
      public AXPlatformNodeTest {
 public:
  AXPlatformNodeCocoaTest() {
    if (GetParam().mac_accessibility_api_migration_enabled) {
      base::FieldTrialParams params;
      params["MacAccessibilityAPIMigrationEnabled"] = "true";
      features_.InitAndEnableFeatureWithParameters(
          features::kMacAccessibilityAPIMigration, params);
    } else {
      features_.InitAndDisableFeature(features::kMacAccessibilityAPIMigration);
    }
  }

  AXPlatformNodeCocoa* GetCocoaNode(const AXNodeID id) const {
    TestAXNodeWrapper* wrapper =
        TestAXNodeWrapper::GetOrCreate(GetTree(), GetNode(id));
    return [[AXPlatformNodeCocoa alloc]
        initWithNode:(ui::AXPlatformNodeBase*)wrapper->ax_platform_node()];
  }

 private:
  base::test::ScopedFeatureList features_;
};

using AXPlatformNodeCocoaTestOldAPI = AXPlatformNodeCocoaTest;
using AXPlatformNodeCocoaTestNewAPI = AXPlatformNodeCocoaTest;

// Tests that should pass regardless of new or old Cocoa a11y API.
INSTANTIATE_TEST_SUITE_P(
    Common,
    AXPlatformNodeCocoaTest,
    ::testing::Values(
        FeatureState{.mac_accessibility_api_migration_enabled = false},
        FeatureState{.mac_accessibility_api_migration_enabled = true}));

// Tests that should only pass with the old Cocoa a11y API.
INSTANTIATE_TEST_SUITE_P(
    NoFeature,
    AXPlatformNodeCocoaTestOldAPI,
    ::testing::Values(FeatureState{
        .mac_accessibility_api_migration_enabled = false}));

// Tests that should only pass with the new Cocoa a11y API.
INSTANTIATE_TEST_SUITE_P(MacAccessibilityAPIMigrationEnabled,
                         AXPlatformNodeCocoaTestNewAPI,
                         ::testing::Values(FeatureState{
                             .mac_accessibility_api_migration_enabled = true}));

// Tests that the Cocoa action list is correctly formed.
TEST_P(AXPlatformNodeCocoaTest, TestCocoaActionListLayout) {
  // Make sure the first action is NSAccessibilityPressAction.
  const ui::CocoaActionList& action_list = GetCocoaActionListForTesting();
  EXPECT_TRUE(
      [action_list[0].second isEqualToString:NSAccessibilityPressAction]);
}

// Tests that the correct methods are enabled based on migration mode.
TEST_P(AXPlatformNodeCocoaTest, TestRespondsToSelector) {
  NSArray<NSString*>* array = @[
    @"accessibilityDisclosedByRow", @"accessibilityDisclosedRows",
    @"accessibilityDisclosureLevel", @"accessibilitySortDirection",
    @"isAccessibilityDisclosed", @"isAccessibilityExpanded",
    @"isAccessibilityFocused"
  ];

  AXPlatformNodeCocoa* node = [[AXPlatformNodeCocoa alloc] initWithNode:nil];

  bool migration_enabled = features::IsMacAccessibilityAPIMigrationEnabled();

  for (NSString* newAPIMethodName in array) {
    EXPECT_EQ([node respondsToSelector:NSSelectorFromString(newAPIMethodName)],
              migration_enabled);
  }
}

// Tests the actions contained in the old API action list.
TEST_P(AXPlatformNodeCocoaTestOldAPI, TestActionListActions) {
  EXPECT_EQ(ui::GetCocoaActionListForTesting().size(), 4u);

  for (const auto& actionPair : ui::GetCocoaActionListForTesting()) {
    EXPECT_TRUE(actionPair.first == ax::mojom::Action::kDoDefault ||
                actionPair.first == ax::mojom::Action::kDecrement ||
                actionPair.first == ax::mojom::Action::kIncrement ||
                actionPair.first == ax::mojom::Action::kShowContextMenu);
  }
}

// Tests the detection of attributes that are available through the new Cocoa
// accessibility API.
TEST_P(AXPlatformNodeCocoaTestNewAPI,
       TestDetectAttributesAvailableThroughNewA11yAPI) {
  NSArray<NSString*>* attributeNames = @[
    NSAccessibilityDisclosedByRowAttribute,
    NSAccessibilityDisclosedRowsAttribute, NSAccessibilityDisclosingAttribute,
    NSAccessibilityDisclosureLevelAttribute, NSAccessibilityFocusedAttribute
  ];

  for (NSString* attributeName in attributeNames) {
    EXPECT_TRUE([AXPlatformNodeCocoa
        isAttributeAvailableThroughNewAccessibilityAPI:attributeName]);
  }
}

// accessibilityColumnIndexRange on a table cell.
TEST_P(AXPlatformNodeCocoaTest, AccessibilityColumnIndexRange) {
  ui::TestAXTreeUpdate update(std::string(R"HTML(
    ++1 kTable
    ++++2 kRow
    ++++++3 kCell
  )HTML"));
  Init(update);

  AXPlatformNodeCocoa* cell = GetCocoaNode(3);
  NSRange range = [cell accessibilityColumnIndexRange];
  EXPECT_EQ(range.location, 0UL);  // Column index should start at 0
  EXPECT_EQ(range.length, 1UL);    // Only one column in this simple setup
}

// accessibilityRowIndexRange on a table cell.
TEST_P(AXPlatformNodeCocoaTest, AccessibilityRowIndexRange) {
  ui::TestAXTreeUpdate update(std::string(R"HTML(
    ++1 kTable
    ++++2 kRow
    ++++++3 kCell
  )HTML"));
  Init(update);

  AXPlatformNodeCocoa* cell = GetCocoaNode(3);
  NSRange range = [cell accessibilityRowIndexRange];
  EXPECT_EQ(range.location, 0UL);  // Row index should start at 0
  EXPECT_EQ(range.length, 1UL);    // Only one row in this simple setup
}

// Non-header cells should not support accessibilitySortDirection, even if
// there's a sort direction in the AXNodeData. Their sort order is "unknown".
TEST_P(AXPlatformNodeCocoaTest, AccessibilitySortDirectionOnCell) {
  AXNodeData root = AXNodeData();
  root.id = 1;
  root.role = ax::mojom::Role::kCell;
  root.AddIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                       static_cast<int>(ax::mojom::SortDirection::kAscending));
  Init(root);
  TestAXNodeWrapper* wrapper =
      TestAXNodeWrapper::GetOrCreate(GetTree(), GetRoot());
  AXPlatformNodeCocoa* node = [[AXPlatformNodeCocoa alloc]
      initWithNode:(ui::AXPlatformNodeBase*)wrapper->ax_platform_node()];
  EXPECT_TRUE(
      [[node accessibilityRole] isEqualToString:NSAccessibilityCellRole]);
  EXPECT_EQ([node internalRole], ax::mojom::Role::kCell);
  EXPECT_EQ([node accessibilitySortDirection],
            NSAccessibilitySortDirectionUnknown);
}

// A row header whose AXNodeData lacks a sort order has an "unknown" sort order.
TEST_P(AXPlatformNodeCocoaTest,
       AccessibilitySortDirectionUnspecifiedOnRowHeader) {
  AXNodeData root = AXNodeData();
  root.id = 1;
  root.role = ax::mojom::Role::kRowHeader;
  Init(root);
  TestAXNodeWrapper* wrapper =
      TestAXNodeWrapper::GetOrCreate(GetTree(), GetRoot());
  AXPlatformNodeCocoa* node = [[AXPlatformNodeCocoa alloc]
      initWithNode:(ui::AXPlatformNodeBase*)wrapper->ax_platform_node()];
  EXPECT_TRUE(
      [[node accessibilityRole] isEqualToString:NSAccessibilityCellRole]);
  EXPECT_EQ([node internalRole], ax::mojom::Role::kRowHeader);
  EXPECT_EQ([node accessibilitySortDirection],
            NSAccessibilitySortDirectionUnknown);
}

// A column header whose AXNodeData lacks a sort order has an "unknown" sort
// order.
TEST_P(AXPlatformNodeCocoaTest,
       AccessibilitySortDirectionUnspecifiedOnColumnHeader) {
  AXNodeData root = AXNodeData();
  root.id = 1;
  root.role = ax::mojom::Role::kColumnHeader;
  Init(root);
  TestAXNodeWrapper* wrapper =
      TestAXNodeWrapper::GetOrCreate(GetTree(), GetRoot());
  AXPlatformNodeCocoa* node = [[AXPlatformNodeCocoa alloc]
      initWithNode:(ui::AXPlatformNodeBase*)wrapper->ax_platform_node()];

  EXPECT_TRUE(
      [[node accessibilityRole] isEqualToString:NSAccessibilityCellRole]);
  EXPECT_EQ([node internalRole], ax::mojom::Role::kColumnHeader);
  EXPECT_EQ([node accessibilitySortDirection],
            NSAccessibilitySortDirectionUnknown);
}

// A row header whose AXNodeData contains an "ascending" sort order has an
// "ascending" sort order.
TEST_P(AXPlatformNodeCocoaTest,
       AccessibilitySortDirectionAscendingOnRowHeader) {
  AXNodeData root = AXNodeData();
  root.id = 1;
  root.role = ax::mojom::Role::kRowHeader;
  root.AddIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                       static_cast<int>(ax::mojom::SortDirection::kAscending));
  Init(root);
  TestAXNodeWrapper* wrapper =
      TestAXNodeWrapper::GetOrCreate(GetTree(), GetRoot());
  AXPlatformNodeCocoa* node = [[AXPlatformNodeCocoa alloc]
      initWithNode:(ui::AXPlatformNodeBase*)wrapper->ax_platform_node()];
  EXPECT_TRUE(
      [[node accessibilityRole] isEqualToString:NSAccessibilityCellRole]);
  EXPECT_EQ([node internalRole], ax::mojom::Role::kRowHeader);
  EXPECT_EQ([node accessibilitySortDirection],
            NSAccessibilitySortDirectionAscending);
}

// A column header whose AXNodeData contains an "ascending" sort order has an
// "ascending" sort order.
TEST_P(AXPlatformNodeCocoaTest,
       AccessibilitySortDirectionAscendingOnColumnHeader) {
  AXNodeData root = AXNodeData();
  root.id = 1;
  root.role = ax::mojom::Role::kColumnHeader;
  root.AddIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                       static_cast<int>(ax::mojom::SortDirection::kAscending));
  Init(root);
  TestAXNodeWrapper* wrapper =
      TestAXNodeWrapper::GetOrCreate(GetTree(), GetRoot());
  AXPlatformNodeCocoa* node = [[AXPlatformNodeCocoa alloc]
      initWithNode:(ui::AXPlatformNodeBase*)wrapper->ax_platform_node()];
  EXPECT_TRUE(
      [[node accessibilityRole] isEqualToString:NSAccessibilityCellRole]);
  EXPECT_EQ([node internalRole], ax::mojom::Role::kColumnHeader);
  EXPECT_EQ([node accessibilitySortDirection],
            NSAccessibilitySortDirectionAscending);
}

// A row header whose AXNodeData contains a "descending" sort order has an
// "descending" sort order.
TEST_P(AXPlatformNodeCocoaTest,
       AccessibilitySortDirectionDescendingOnRowHeader) {
  AXNodeData root = AXNodeData();
  root.id = 1;
  root.role = ax::mojom::Role::kRowHeader;
  root.AddIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                       static_cast<int>(ax::mojom::SortDirection::kDescending));
  Init(root);
  TestAXNodeWrapper* wrapper =
      TestAXNodeWrapper::GetOrCreate(GetTree(), GetRoot());
  AXPlatformNodeCocoa* node = [[AXPlatformNodeCocoa alloc]
      initWithNode:(ui::AXPlatformNodeBase*)wrapper->ax_platform_node()];
  EXPECT_TRUE(
      [[node accessibilityRole] isEqualToString:NSAccessibilityCellRole]);
  EXPECT_EQ([node internalRole], ax::mojom::Role::kRowHeader);
  EXPECT_EQ([node accessibilitySortDirection],
            NSAccessibilitySortDirectionDescending);
}

// A column header whose AXNodeData contains a "descending" sort order has an
// "descending" sort order.
TEST_P(AXPlatformNodeCocoaTest,
       AccessibilitySortDirectionDescendingOnColumnHeader) {
  AXNodeData root = AXNodeData();
  root.id = 1;
  root.role = ax::mojom::Role::kColumnHeader;
  root.AddIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                       static_cast<int>(ax::mojom::SortDirection::kDescending));
  Init(root);
  TestAXNodeWrapper* wrapper =
      TestAXNodeWrapper::GetOrCreate(GetTree(), GetRoot());
  AXPlatformNodeCocoa* node = [[AXPlatformNodeCocoa alloc]
      initWithNode:(ui::AXPlatformNodeBase*)wrapper->ax_platform_node()];
  EXPECT_TRUE(
      [[node accessibilityRole] isEqualToString:NSAccessibilityCellRole]);
  EXPECT_EQ([node internalRole], ax::mojom::Role::kColumnHeader);
  EXPECT_EQ([node accessibilitySortDirection],
            NSAccessibilitySortDirectionDescending);
}

// A row header whose AXNodeData contains an "other" sort order has an "unknown"
// sort order.
TEST_P(AXPlatformNodeCocoaTest, AccessibilitySortDirectionOtherOnRowHeader) {
  AXNodeData root = AXNodeData();
  root.id = 1;
  root.role = ax::mojom::Role::kRowHeader;
  root.AddIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                       static_cast<int>(ax::mojom::SortDirection::kOther));
  Init(root);
  TestAXNodeWrapper* wrapper =
      TestAXNodeWrapper::GetOrCreate(GetTree(), GetRoot());
  AXPlatformNodeCocoa* node = [[AXPlatformNodeCocoa alloc]
      initWithNode:(ui::AXPlatformNodeBase*)wrapper->ax_platform_node()];
  EXPECT_TRUE(
      [[node accessibilityRole] isEqualToString:NSAccessibilityCellRole]);
  EXPECT_EQ([node internalRole], ax::mojom::Role::kRowHeader);
  EXPECT_EQ([node accessibilitySortDirection],
            NSAccessibilitySortDirectionUnknown);
}

// A column header whose AXNodeData contains an "other" sort order has an
// "unknown" sort order.
TEST_P(AXPlatformNodeCocoaTest, AccessibilitySortDirectionOtherOnColumnHeader) {
  AXNodeData root = AXNodeData();
  root.id = 1;
  root.role = ax::mojom::Role::kColumnHeader;
  root.AddIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                       static_cast<int>(ax::mojom::SortDirection::kOther));
  Init(root);
  TestAXNodeWrapper* wrapper =
      TestAXNodeWrapper::GetOrCreate(GetTree(), GetRoot());
  AXPlatformNodeCocoa* node = [[AXPlatformNodeCocoa alloc]
      initWithNode:(ui::AXPlatformNodeBase*)wrapper->ax_platform_node()];
  EXPECT_TRUE(
      [[node accessibilityRole] isEqualToString:NSAccessibilityCellRole]);
  EXPECT_EQ([node internalRole], ax::mojom::Role::kColumnHeader);
  EXPECT_EQ([node accessibilitySortDirection],
            NSAccessibilitySortDirectionUnknown);
}

// A menu item with the expanded state should return true for
// `isAccessibilityExpanded`.
TEST_P(AXPlatformNodeCocoaTest, IsAccessibilityExpandedSetToExpanded) {
  AXNodeData root = AXNodeData();
  root.id = 1;
  root.role = ax::mojom::Role::kMenuItem;
  root.AddState(ax::mojom::State::kExpanded);
  Init(root);
  TestAXNodeWrapper* wrapper =
      TestAXNodeWrapper::GetOrCreate(GetTree(), GetRoot());
  AXPlatformNodeCocoa* node = [[AXPlatformNodeCocoa alloc]
      initWithNode:(ui::AXPlatformNodeBase*)wrapper->ax_platform_node()];
  EXPECT_TRUE([node isAccessibilityExpanded]);
}

// A menu item with the collpased state should return false for
// `isAccessibilityExpanded`.
TEST_P(AXPlatformNodeCocoaTest, IsAccessibilityExpandedSetToCollapsed) {
  AXNodeData root = AXNodeData();
  root.id = 1;
  root.role = ax::mojom::Role::kMenuItem;
  root.AddState(ax::mojom::State::kCollapsed);
  Init(root);
  TestAXNodeWrapper* wrapper =
      TestAXNodeWrapper::GetOrCreate(GetTree(), GetRoot());
  AXPlatformNodeCocoa* node = [[AXPlatformNodeCocoa alloc]
      initWithNode:(ui::AXPlatformNodeBase*)wrapper->ax_platform_node()];
  EXPECT_FALSE([node isAccessibilityExpanded]);
}

// A menu item without the expanded or collapsed state should return false for
// `isAccessibilityExpanded`.
TEST_P(AXPlatformNodeCocoaTest, IsAccessibilityExpandedNotSet) {
  AXNodeData root = AXNodeData();
  root.id = 1;
  root.role = ax::mojom::Role::kMenuItem;
  Init(root);
  TestAXNodeWrapper* wrapper =
      TestAXNodeWrapper::GetOrCreate(GetTree(), GetRoot());
  AXPlatformNodeCocoa* node = [[AXPlatformNodeCocoa alloc]
      initWithNode:(ui::AXPlatformNodeBase*)wrapper->ax_platform_node()];
  EXPECT_FALSE([node isAccessibilityExpanded]);
}

}  // namespace ui
