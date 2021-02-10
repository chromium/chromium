// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ui {

TEST(AXNodeTest, GetLowestPlatformAncestor) {
  // ++kRootWebArea
  // ++++kButton (IsLeaf=false)
  // ++++++kGenericContainer ignored
  // ++++++++kStaticText "Hello"
  // ++++++++++kInlineTextBox "Hello" (IsLeaf=true)
  // ++++kTextField "World" (IsLeaf=true)
  // ++++++kStaticText "World"
  // ++++++++kInlineTextBox "World" (IsLeaf=true)
  AXNodeData root;
  AXNodeData button;
  AXNodeData generic_container;
  AXNodeData static_text_1;
  AXNodeData inline_box_1;
  AXNodeData text_field;
  AXNodeData static_text_2;
  AXNodeData inline_box_2;

  root.id = 1;
  button.id = 2;
  generic_container.id = 3;
  static_text_1.id = 4;
  inline_box_1.id = 5;
  text_field.id = 6;
  static_text_2.id = 7;
  inline_box_2.id = 8;

  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {button.id, text_field.id};

  button.role = ax::mojom::Role::kButton;
  button.SetValue("Hello");
  button.child_ids = {generic_container.id};

  generic_container.role = ax::mojom::Role::kGenericContainer;
  generic_container.AddState(ax::mojom::State::kIgnored);
  generic_container.child_ids = {static_text_1.id};

  static_text_1.role = ax::mojom::Role::kStaticText;
  static_text_1.SetName("Hello");
  static_text_1.child_ids = {inline_box_1.id};

  inline_box_1.role = ax::mojom::Role::kInlineTextBox;
  inline_box_1.SetName("Hello");

  text_field.role = ax::mojom::Role::kTextField;
  text_field.AddState(ax::mojom::State::kEditable);
  text_field.AddBoolAttribute(ax::mojom::BoolAttribute::kEditableRoot, true);
  text_field.SetValue("World");
  text_field.child_ids = {static_text_2.id};

  static_text_2.role = ax::mojom::Role::kStaticText;
  static_text_2.AddState(ax::mojom::State::kEditable);
  static_text_2.SetName("World");
  static_text_2.child_ids = {inline_box_2.id};

  inline_box_2.role = ax::mojom::Role::kInlineTextBox;
  inline_box_2.AddState(ax::mojom::State::kEditable);
  inline_box_2.SetName("World");

  AXTreeUpdate initial_state;
  initial_state.root_id = root.id;
  initial_state.nodes = {root,          button,       generic_container,
                         static_text_1, inline_box_1, text_field,
                         static_text_2, inline_box_2};
  initial_state.has_tree_data = true;

  AXTreeData tree_data;
  tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  tree_data.title = "Application";
  initial_state.tree_data = tree_data;

  AXTree tree;
  ASSERT_TRUE(tree.Unserialize(initial_state));

  const AXNode* root_node = tree.root();
  ASSERT_EQ(root.id, root_node->id());
  EXPECT_EQ(root_node, root_node->GetLowestPlatformAncestor());

  const AXNode* button_node = root_node->children()[0];
  ASSERT_EQ(button.id, button_node->id());
  EXPECT_EQ(button_node, button_node->GetLowestPlatformAncestor());

  const AXNode* generic_container_node = button_node->children()[0];
  ASSERT_EQ(generic_container.id, generic_container_node->id());
  EXPECT_EQ(button_node, generic_container_node->GetLowestPlatformAncestor());

  const AXNode* static_text_1_node = generic_container_node->children()[0];
  ASSERT_EQ(static_text_1.id, static_text_1_node->id());
  EXPECT_EQ(static_text_1_node,
            static_text_1_node->GetLowestPlatformAncestor());

  const AXNode* inline_box_1_node = static_text_1_node->children()[0];
  ASSERT_EQ(inline_box_1.id, inline_box_1_node->id());
  EXPECT_EQ(static_text_1_node, inline_box_1_node->GetLowestPlatformAncestor());

  const AXNode* text_field_node = root_node->children()[1];
  ASSERT_EQ(text_field.id, text_field_node->id());
  EXPECT_EQ(text_field_node, text_field_node->GetLowestPlatformAncestor());

  const AXNode* static_text_2_node = text_field_node->children()[0];
  ASSERT_EQ(static_text_2.id, static_text_2_node->id());
  EXPECT_EQ(text_field_node, static_text_2_node->GetLowestPlatformAncestor());

  const AXNode* inline_box_2_node = static_text_2_node->children()[0];
  ASSERT_EQ(inline_box_2.id, inline_box_2_node->id());
  EXPECT_EQ(text_field_node, inline_box_2_node->GetLowestPlatformAncestor());
}

}  // namespace ui
