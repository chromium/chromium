// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node_data.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ui {

TEST(AXNodeDataTest, GetAndSetCheckedState) {
  AXNodeData root;
  EXPECT_EQ(ax::mojom::CheckedState::kNone, root.GetCheckedState());
  EXPECT_FALSE(root.HasIntAttribute(ax::mojom::IntAttribute::kCheckedState));

  root.SetCheckedState(ax::mojom::CheckedState::kMixed);
  EXPECT_EQ(ax::mojom::CheckedState::kMixed, root.GetCheckedState());
  EXPECT_TRUE(root.HasIntAttribute(ax::mojom::IntAttribute::kCheckedState));

  root.SetCheckedState(ax::mojom::CheckedState::kFalse);
  EXPECT_EQ(ax::mojom::CheckedState::kFalse, root.GetCheckedState());
  EXPECT_TRUE(root.HasIntAttribute(ax::mojom::IntAttribute::kCheckedState));

  root.SetCheckedState(ax::mojom::CheckedState::kNone);
  EXPECT_EQ(ax::mojom::CheckedState::kNone, root.GetCheckedState());
  EXPECT_FALSE(root.HasIntAttribute(ax::mojom::IntAttribute::kCheckedState));
}

TEST(AXNodeDataTest, TextAttributes) {
  AXNodeData node_1;
  node_1.AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize, 1.5);

  AXNodeData node_2;
  node_2.AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize, 1.5);
  EXPECT_TRUE(node_1.GetTextStyles() == node_2.GetTextStyles());

  node_2.AddIntAttribute(ax::mojom::IntAttribute::kColor, 100);
  EXPECT_TRUE(node_1.GetTextStyles() != node_2.GetTextStyles());

  node_1.AddIntAttribute(ax::mojom::IntAttribute::kColor, 100);
  EXPECT_TRUE(node_1.GetTextStyles() == node_2.GetTextStyles());

  node_2.RemoveIntAttribute(ax::mojom::IntAttribute::kColor);
  EXPECT_TRUE(node_1.GetTextStyles() != node_2.GetTextStyles());

  node_2.AddIntAttribute(ax::mojom::IntAttribute::kColor, 100);
  EXPECT_TRUE(node_1.GetTextStyles() == node_2.GetTextStyles());

  node_1.AddStringAttribute(ax::mojom::StringAttribute::kFontFamily,
                            "test font");
  EXPECT_TRUE(node_1.GetTextStyles() != node_2.GetTextStyles());

  node_2.AddStringAttribute(ax::mojom::StringAttribute::kFontFamily,
                            "test font");
  EXPECT_TRUE(node_1.GetTextStyles() == node_2.GetTextStyles());

  node_2.RemoveStringAttribute(ax::mojom::StringAttribute::kFontFamily);
  EXPECT_TRUE(node_1.GetTextStyles() != node_2.GetTextStyles());

  node_2.AddStringAttribute(ax::mojom::StringAttribute::kFontFamily,
                            "test font");
  EXPECT_TRUE(node_1.GetTextStyles() == node_2.GetTextStyles());

  node_2.AddStringAttribute(ax::mojom::StringAttribute::kFontFamily,
                            "different font");
  EXPECT_TRUE(node_1.GetTextStyles() != node_2.GetTextStyles());

  std::string tooltip;
  node_2.AddStringAttribute(ax::mojom::StringAttribute::kTooltip,
                            "test tooltip");
  EXPECT_TRUE(node_2.GetStringAttribute(ax::mojom::StringAttribute::kTooltip,
                                        &tooltip));
  EXPECT_EQ(tooltip, "test tooltip");

  AXNodeTextStyles node1_styles = node_1.GetTextStyles();
  AXNodeTextStyles moved_styles = std::move(node1_styles);
  EXPECT_TRUE(node1_styles != moved_styles);
  EXPECT_TRUE(moved_styles == node_1.GetTextStyles());
}

}  // namespace ui
