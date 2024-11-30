// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_platform_node_unittest.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"

namespace {

struct FeatureState {
  bool mac_accessibility_api_migration_enabled = false;
};

}  // namespace

using AXRange = ui::AXPlatformNodeDelegate::AXRange;

@interface AXPlatformNodeCocoa (Private)

- (void)addTextAnnotationsIn:(const AXRange*)axRange
                          to:(NSMutableAttributedString*)attributedString;

@end

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

  AXPlatformNodeCocoa* GetCocoaNode(AXNode* node) const {
    TestAXNodeWrapper* wrapper =
        TestAXNodeWrapper::GetOrCreate(GetTree(), node);
    wrapper->SetNode(*node);
    return [[AXPlatformNodeCocoa alloc]
        initWithNode:(ui::AXPlatformNodeBase*)wrapper->ax_platform_node()];
  }

  AXPlatformNodeCocoa* GetCocoaNode(const AXNodeID id) const {
    return GetCocoaNode(GetNode(id));
  }

  void TestAddTextAnnotationsTo(bool skip_nodes = false) {
    // Set up the root node.
    ui::AXNodeData root_data;
    root_data.id = 1;
    root_data.role = ax::mojom::Role::kGenericContainer;

    // Set up the AXTree with separate nodes for each string.
    root_data.child_ids = {2, 3, 4, 5, 6, 7, 8, 9};

    // Node for "This ".
    ui::AXNodeData node1_data;
    node1_data.id = 2;
    node1_data.role = ax::mojom::Role::kStaticText;
    node1_data.SetName("This ");

    // Node for "is ".
    ui::AXNodeData node2_data;
    node2_data.id = 3;
    node2_data.role = ax::mojom::Role::kStaticText;
    std::string node2_text_content = "iz ";
    node2_data.SetName(node2_text_content);
    node2_data.AddIntListAttribute(
        ax::mojom::IntListAttribute::kMarkerTypes,
        {static_cast<int>(ax::mojom::MarkerType::kSpelling)});
    node2_data.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts,
                                   {0});
    node2_data.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds,
                                   {2});

    // Node for "a ".
    ui::AXNodeData node3_data;
    node3_data.id = 4;
    node3_data.role = ax::mojom::Role::kStaticText;
    std::string node3_text_content = "a ";
    node3_data.SetName(node3_text_content);
    node3_data.AddTextStyle(ax::mojom::TextStyle::kBold);
    node3_data.AddTextStyle(ax::mojom::TextStyle::kItalic);

    // Node for "teast of wills ".
    ui::AXNodeData node4_data;
    node4_data.id = 5;
    node4_data.role = ax::mojom::Role::kStaticText;
    std::string node4_text_content = "teast of wills ";
    node4_data.SetName(node4_text_content);

    node4_data.AddIntListAttribute(
        ax::mojom::IntListAttribute::kMarkerTypes,
        {static_cast<int>(ax::mojom::MarkerType::kSpelling),
         static_cast<int>(ax::mojom::MarkerType::kSpelling)});
    node4_data.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts,
                                   {0, 9});
    node4_data.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds,
                                   {5, 14});

    // Node for "that ".
    ui::AXNodeData node5_data;
    node5_data.id = 6;
    node5_data.role = ax::mojom::Role::kStaticText;
    std::string node5_text_content = "that ";
    node5_data.SetName(node5_text_content);
    SkColor foreground_color =
        SkColorSetARGB(255, 255, 0, 0);  // Red with full opacity
    node5_data.AddIntAttribute(ax::mojom::IntAttribute::kColor,
                               static_cast<int>(foreground_color));

    // Node for "will ".
    ui::AXNodeData node6_data;
    node6_data.id = 7;
    node6_data.role = ax::mojom::Role::kStaticText;
    std::string node6_text_content = "will ";
    node6_data.SetName(node6_text_content);
    SkColor background_color =
        SkColorSetARGB(0, 0, 0, 0);  // Black with full opacity
    node6_data.AddIntAttribute(ax::mojom::IntAttribute::kBackgroundColor,
                               static_cast<int>(background_color));

    // Node for "not ".
    ui::AXNodeData node7_data;
    node7_data.id = 8;
    node7_data.role = ax::mojom::Role::kStaticText;
    std::string node7_text_content = "not ";
    node7_data.SetName(node7_text_content);
    node7_data.AddIntAttribute(
        ax::mojom::IntAttribute::kTextUnderlineStyle,
        static_cast<int32_t>(ax::mojom::TextDecorationStyle::kDotted));

    // Node for "be denied."
    ui::AXNodeData node8_data;
    node8_data.id = 9;
    node8_data.role = ax::mojom::Role::kStaticText;
    std::string node8_text_content = "be denied.";
    node8_data.SetName(node8_text_content);
    node8_data.AddIntAttribute(
        ax::mojom::IntAttribute::kTextStrikethroughStyle,
        static_cast<int>(ax::mojom::TextDecorationStyle::kWavy));
    const float kFontSize = 1001.5;
    node8_data.AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize,
                                 kFontSize);

    // Build the tree.
    ui::AXTreeUpdate update;
    update.root_id = root_data.id;
    update.nodes.push_back(root_data);
    update.nodes.push_back(node1_data);
    update.nodes.push_back(node2_data);
    update.nodes.push_back(node3_data);
    update.nodes.push_back(node4_data);
    update.nodes.push_back(node5_data);
    update.nodes.push_back(node6_data);
    update.nodes.push_back(node7_data);
    update.nodes.push_back(node8_data);

    Init(update);

    ui::AXTree& tree = *GetTree();

    // Create an AXRange that spans all the nodes.
    ui::AXNode* start_node =
        tree.GetFromId(node1_data.id);  // Node with "This "
    ui::AXNode* end_node =
        tree.GetFromId(node8_data.id);  // Node with "be denied."

    // Create start and end positions.
    auto start_position = ui::AXNodePosition::CreateTextPosition(
        *start_node, /* offset = */ 0, ax::mojom::TextAffinity::kDownstream);

    auto end_position = ui::AXNodePosition::CreateTextPosition(
        *end_node, /* offset = */ node8_text_content.length(),
        ax::mojom::TextAffinity::kDownstream);

    // Create the AXRange.
    ui::AXRange ax_range(start_position->Clone(), end_position->Clone());

    // Create an attributed string with the concatenated text from the nodes.
    std::u16string full_text_utf16;
    for (const ui::AXPlatformNodeDelegate::AXRange& leaf_text_range :
         ax_range) {
      // Optionally skip a node, to set the attributed string up with a string
      // that doesn't include text from all the nodes. This allows us to test
      // how -addTextAnnotationsIn:to: handles an extra leaf node in the middle
      // of the string and extra nodes past the end.
      if (skip_nodes && (leaf_text_range.anchor()->anchor_id() == 3 ||
                         leaf_text_range.anchor()->anchor_id() == 8 ||
                         leaf_text_range.anchor()->anchor_id() == 9)) {
        continue;
      }
      full_text_utf16 += leaf_text_range.GetText();
    }

    NSMutableAttributedString* attributed_string =
        [[NSMutableAttributedString alloc]
            initWithString:base::SysUTF16ToNSString(full_text_utf16)];

    // Finally, create a node so that we can test -addTextAnnotationsIn:to:.
    AXPlatformNodeCocoa* platform_node =
        [[AXPlatformNodeCocoa alloc] initWithNode:nil];
    [platform_node addTextAnnotationsIn:&ax_range to:attributed_string];

    // Set up the ranges that match the attributes we've set on the nodes.
    NSRange mispelled_range1 = NSMakeRange(0, NSNotFound);
    if (!skip_nodes) {
      mispelled_range1 = [attributed_string.string
          rangeOfString:[NSString
                            stringWithUTF8String:node2_text_content.c_str()]];
      // The string includes a space that's not part of the misspelling.
      mispelled_range1.length -= 1;
    }

    NSRange node4_range = [attributed_string.string
        rangeOfString:[NSString
                          stringWithUTF8String:node4_text_content.c_str()]];
    NSRange mispelled_range2 = NSMakeRange(node4_range.location, 5);
    NSRange mispelled_range3 = NSMakeRange(node4_range.location + 9, 5);

    NSRange bold_and_italic_range = [attributed_string.string
        rangeOfString:[NSString
                          stringWithUTF8String:node3_text_content.c_str()]];

    NSRange foreground_color_range = [attributed_string.string
        rangeOfString:[NSString
                          stringWithUTF8String:node5_text_content.c_str()]];
    NSRange background_color_range = [attributed_string.string
        rangeOfString:[NSString
                          stringWithUTF8String:node6_text_content.c_str()]];

    NSRange underline_range = [attributed_string.string
        rangeOfString:[NSString
                          stringWithUTF8String:node7_text_content.c_str()]];
    NSRange strikethrough_range = [attributed_string.string
        rangeOfString:[NSString
                          stringWithUTF8String:node8_text_content.c_str()]];

    // Iterate over the entire attributed string and check attributes.
    __block int misspelled_attribute_count = 0;
    __block int bold_count = 0;
    __block int italic_count = 0;
    __block int foreground_color_count = 0;
    __block int background_color_count = 0;
    __block int underline_count = 0;
    __block int strikethrough_count = 0;
    __block float font_size = 0.0;
    __block int unexpected_attributes = 0;

    [attributed_string
        enumerateAttributesInRange:NSMakeRange(0, attributed_string.length)
                           options:0
                        usingBlock:^(
                            NSDictionary<NSAttributedStringKey, id>* attributes,
                            NSRange range, BOOL* stop) {
                          if (NSEqualRanges(range, bold_and_italic_range)) {
                            if (attributes[@"AXFont"][@"AXFontBold"]) {
                              bold_count++;
                            }
                            if (attributes[@"AXFont"][@"AXFontItalic"]) {
                              italic_count++;
                            }
                          } else if (NSEqualRanges(range,
                                                   foreground_color_range)) {
                            if (attributes
                                    [NSAccessibilityForegroundColorTextAttribute]) {
                              foreground_color_count++;
                            }
                          } else if (NSEqualRanges(range,
                                                   background_color_range)) {
                            if (attributes
                                    [NSAccessibilityBackgroundColorTextAttribute]) {
                              background_color_count++;
                            }
                          } else if (NSEqualRanges(range, underline_range)) {
                            if (attributes
                                    [NSAccessibilityUnderlineTextAttribute]) {
                              underline_count++;
                            }
                          } else if (NSEqualRanges(range,
                                                   strikethrough_range)) {
                            if (attributes
                                    [NSAccessibilityStrikethroughTextAttribute]) {
                              strikethrough_count++;
                            }
                            font_size = [(
                                NSNumber*)attributes[@"AXFont"]
                                                    [NSAccessibilityFontSizeKey]
                                floatValue];
                          } else if (NSEqualRanges(range, mispelled_range1) ||
                                     NSEqualRanges(range, mispelled_range2) ||
                                     NSEqualRanges(range, mispelled_range3)) {
                            if (attributes[@"AXMarkedMisspelled"]) {
                              misspelled_attribute_count++;
                            }
                          } else {
                            // Ensure other parts don't have attributes.
                            if (attributes.count > 1 ||
                                [attributes[@"AXFont"] count]) {
                              unexpected_attributes++;
                            }
                          }
                        }];

    int expected_misspelled = 3;
    int expected_underline = 1;
    int expected_strikethrough = 1;
    float expected_font_size = kFontSize;
    if (skip_nodes) {
      expected_misspelled--;
      expected_strikethrough--;
      expected_underline--;
      expected_font_size = 0;
    }
    EXPECT_EQ(misspelled_attribute_count, expected_misspelled);
    EXPECT_EQ(bold_count, 1);
    EXPECT_EQ(italic_count, 1);
    EXPECT_EQ(foreground_color_count, 1);
    EXPECT_EQ(background_color_count, 1);
    EXPECT_EQ(underline_count, expected_underline);
    EXPECT_EQ(strikethrough_count, expected_strikethrough);
    EXPECT_EQ(font_size, expected_font_size);
    EXPECT_EQ(unexpected_attributes, 0);
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
    @"accessibilityColumnCount", @"accessibilityDisclosedByRow",
    @"accessibilityDisclosedRows", @"accessibilityDisclosureLevel",
    @"accessibilityHeader", @"accessibilityIndex", @"accessibilityRowCount",
    @"accessibilitySortDirection", @"isAccessibilityDisclosed",
    @"isAccessibilityExpanded", @"isAccessibilityFocused"
  ];

  AXPlatformNodeCocoa* node = [[AXPlatformNodeCocoa alloc] initWithNode:nil];

  bool migration_enabled = features::IsMacAccessibilityAPIMigrationEnabled();

  for (NSString* newAPIMethodName in array) {
    EXPECT_EQ([node respondsToSelector:NSSelectorFromString(newAPIMethodName)],
              migration_enabled);
  }
}

// Tests that -addTextAnnotations:to: correctly applies attributes to the
// attributed string it's passed.
TEST_P(AXPlatformNodeCocoaTest, AddTextAnnotations) {
  TestAddTextAnnotationsTo();
}

// Tests that -addTextAnnotations:to: correctly skips over nodes that don't
// exist in the attributed string it's passed.
TEST_P(AXPlatformNodeCocoaTest, AddTextAnnotationsWithSkippedNodes) {
  TestAddTextAnnotationsTo(/* skip_nodes = */ true);
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

// accessibilityVisibleColumns on a table.
TEST_P(AXPlatformNodeCocoaTest, AccessibilityVisibleColumns) {
  ui::TestAXTreeUpdate update(std::string(R"HTML(
    ++1 kTable
    ++++2 kColumn
  )HTML"));
  Init(update);

  AXPlatformNodeCocoa* table = GetCocoaNode(1);
  AXPlatformNodeCocoa* column = GetCocoaNode(2);
  NSArray* columns = [table accessibilityVisibleColumns];
  EXPECT_EQ([columns count], 1UL);
  EXPECT_EQ([[columns firstObject] node]->GetUniqueId(),
            [column node]->GetUniqueId());
}

// accessibilityVisibleCells on a table.
TEST_P(AXPlatformNodeCocoaTest, AccessibilityVisibleCells) {
  ui::TestAXTreeUpdate update(std::string(R"HTML(
    ++1 kTable
    ++++2 kRow
    ++++++3 kCell
  )HTML"));
  Init(update);

  AXPlatformNodeCocoa* table = GetCocoaNode(1);
  AXPlatformNodeCocoa* cell = GetCocoaNode(3);
  NSArray* cells = [table accessibilityVisibleCells];
  EXPECT_EQ([cells count], 1UL);
  EXPECT_EQ([[cells firstObject] node]->GetUniqueId(),
            [cell node]->GetUniqueId());
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
  AXPlatformNodeCocoa* node = GetCocoaNode(GetRoot());
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
  AXPlatformNodeCocoa* node = GetCocoaNode(GetRoot());
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
  AXPlatformNodeCocoa* node = GetCocoaNode(GetRoot());
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
  AXPlatformNodeCocoa* node = GetCocoaNode(GetRoot());
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
  AXPlatformNodeCocoa* node = GetCocoaNode(GetRoot());
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
  AXPlatformNodeCocoa* node = GetCocoaNode(GetRoot());
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
  AXPlatformNodeCocoa* node = GetCocoaNode(GetRoot());
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
  AXPlatformNodeCocoa* node = GetCocoaNode(GetRoot());
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
  AXPlatformNodeCocoa* node = GetCocoaNode(GetRoot());
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
  AXPlatformNodeCocoa* node = GetCocoaNode(GetRoot());
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
  AXPlatformNodeCocoa* node = GetCocoaNode(GetRoot());
  EXPECT_FALSE([node isAccessibilityExpanded]);
}

// A menu item without the expanded or collapsed state should return false for
// `isAccessibilityExpanded`.
TEST_P(AXPlatformNodeCocoaTest, IsAccessibilityExpandedNotSet) {
  AXNodeData root = AXNodeData();
  root.id = 1;
  root.role = ax::mojom::Role::kMenuItem;
  Init(root);
  AXPlatformNodeCocoa* node = GetCocoaNode(GetRoot());
  EXPECT_FALSE([node isAccessibilityExpanded]);
}

// `accessibilityIndex` on rows and columns.
TEST_P(AXPlatformNodeCocoaTest, AccessibilityIndexOnRowsAndColumns) {
  ui::TestAXTreeUpdate update(std::string(R"HTML(
    ++1 kTable
    ++++2 kRow
    ++++++3 kCell
    ++++++4 kCell
    ++++5 kRow
    ++++++6 kCell
    ++++++7 kCell
    ++++8 kRow
    ++++++9 kCell
    ++++++10 kCell
  )HTML"));
  Init(update);
  EXPECT_EQ([GetCocoaNode(2) accessibilityIndex], 0U);
  EXPECT_EQ([GetCocoaNode(5) accessibilityIndex], 1U);
  EXPECT_EQ([GetCocoaNode(8) accessibilityIndex], 2U);

  // Exposing accessible column objects is unique to NSAccessibility so we
  // retrieve them via `GetExtraMacNodes`.
  const std::vector<raw_ptr<AXNode, VectorExperimental>>& nodes =
      *GetRoot()->GetExtraMacNodes();
  EXPECT_EQ(nodes.size(), 3U);

  // The first extra node is the first column, with index of 0.
  AXPlatformNodeCocoa* child = GetCocoaNode(nodes[0]);
  EXPECT_TRUE(
      [[child accessibilityRole] isEqualToString:NSAccessibilityColumnRole]);
  EXPECT_EQ([child accessibilityIndex], 0U);

  // The second extra node is the second column, with index of 1.
  child = GetCocoaNode(nodes[1]);
  EXPECT_TRUE(
      [[child accessibilityRole] isEqualToString:NSAccessibilityColumnRole]);
  EXPECT_EQ([child accessibilityIndex], 1U);

  // The final extra node is to hold the headers, and is present even without
  // headers. Index does not apply to this object.
  child = GetCocoaNode(nodes[2]);
  EXPECT_TRUE(
      [[child accessibilityRole] isEqualToString:NSAccessibilityGroupRole]);
  EXPECT_EQ([child accessibilityIndex], NSNotFound);
}

// `accessibilityRowCount` and `accessibilityColumnCount` on a table.
TEST_P(AXPlatformNodeCocoaTest, AccessibilityRowAndColumnCount) {
  ui::TestAXTreeUpdate update(std::string(R"HTML(
    ++1 kTable
    ++++2 kRow
    ++++++3 kCell
    ++++++4 kCell
    ++++5 kRow
    ++++++6 kCell
    ++++++7 kCell
    ++++8 kRow
    ++++++9 kCell
    ++++++10 kCell
  )HTML"));
  Init(update);
  AXPlatformNodeCocoa* table = GetCocoaNode(1);
  EXPECT_EQ([table accessibilityColumnCount], 2U);
  EXPECT_EQ([table accessibilityRowCount], 3U);

  // These attributes are only supported on the table/grid.
  AXPlatformNodeCocoa* row = GetCocoaNode(2);
  EXPECT_EQ([row accessibilityColumnCount], NSNotFound);
  EXPECT_EQ([row accessibilityRowCount], NSNotFound);
}

// Tests that the string keys returned in
// newAccessibilityAPIMethodToAttributeMap are all selectors supported on an
// AXPlatformNodeCocoa instance.
TEST_P(AXPlatformNodeCocoaTestNewAPI,
       AccessibilityAPIToAttributeMapKeysAreSelectors) {
  NSDictionary* map =
      [AXPlatformNodeCocoa newAccessibilityAPIMethodToAttributeMap];
  AXPlatformNodeCocoa* node = [[AXPlatformNodeCocoa alloc] initWithNode:nil];
  for (NSString* selector_string in map) {
    EXPECT_TRUE(
        [node respondsToSelector:NSSelectorFromString(selector_string)]);
  }
}

// Test that supportsNewAccessibilityAPIMethod returns true for a
// new accessibility API that is supported on all role types
// (isAccessibilityFocused) by testing whether it available
// for an arbitrary role.
TEST_P(AXPlatformNodeCocoaTest, SupportsNewAccessibilityAPIMethod) {
  AXNodeData root = AXNodeData();
  root.id = 1;
  root.role = ax::mojom::Role::kHeading;
  Init(root);
  TestAXNodeWrapper* wrapper =
      TestAXNodeWrapper::GetOrCreate(GetTree(), GetRoot());
  AXPlatformNodeCocoa* node = [[AXPlatformNodeCocoa alloc]
      initWithNode:(ui::AXPlatformNodeBase*)wrapper->ax_platform_node()];
  EXPECT_TRUE([[node accessibilityRole] isEqualToString:@"AXHeading"]);
  EXPECT_EQ([node internalRole], ax::mojom::Role::kHeading);
  EXPECT_TRUE(
      [node supportsNewAccessibilityAPIMethod:@"isAccessibilityFocused"]);
}

}  // namespace ui
