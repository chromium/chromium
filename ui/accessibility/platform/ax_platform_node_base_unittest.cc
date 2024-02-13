// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_base.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/ax_platform_for_test.h"
#include "ui/accessibility/platform/ax_platform_node_unittest.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"
#include "ui/accessibility/test_ax_tree_update.h"

using ax::mojom::Role;
using ax::mojom::State;

namespace ui {
namespace {

void SetIsInvisible(AXTree* tree, int id, bool invisible) {
  AXTreeUpdate update;
  update.nodes.resize(1);
  update.nodes[0] = tree->GetFromId(id)->data();
  if (invisible)
    update.nodes[0].AddState(ax::mojom::State::kInvisible);
  else
    update.nodes[0].RemoveState(ax::mojom::State::kInvisible);
  tree->Unserialize(update);
}

void SetRole(AXTree* tree, int id, ax::mojom::Role role) {
  AXTreeUpdate update;
  update.nodes.resize(1);
  update.nodes[0] = tree->GetFromId(id)->data();
  update.nodes[0].role = role;
  tree->Unserialize(update);
}

}  // namespace

TEST_F(AXPlatformNodeTest, GetHypertext) {
  // RootWebArea #1
  // ++++StaticText "text1" #2
  // ++++StaticText "text2" #3
  // ++++StaticText "text3" #4
  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;

  AXNodeData item1, item2, item3;
  item1.id = 2;
  item1.role = ax::mojom::Role::kStaticText;
  item1.SetName("text1");
  item2.id = 3;
  item2.role = ax::mojom::Role::kStaticText;
  item2.SetName("text2");
  item3.id = 4;
  item3.role = ax::mojom::Role::kStaticText;
  item3.SetName("text3");

  root_data.child_ids = {item1.id, item2.id, item3.id};

  AXTreeUpdate update;
  update.root_id = 1;
  update.nodes = {root_data, item1, item2, item3};

  AXTree* tree = Init(update);

  // Set an AXMode on the AXPlatformNode as some platforms (auralinux) use it to
  // determine if it should enable accessibility.
  ScopedAXModeSetter ax_mode_setter(kAXModeComplete);

  AXPlatformNodeBase* root = static_cast<AXPlatformNodeBase*>(
      TestAXNodeWrapper::GetOrCreate(tree, tree->root())->ax_platform_node());

  EXPECT_EQ(root->GetHypertext(), u"text1text2text3");

  AXPlatformNodeBase* text1 = static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(root->ChildAtIndex(0)));
  EXPECT_EQ(text1->GetHypertext(), u"text1");

  AXPlatformNodeBase* text2 = static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(root->ChildAtIndex(1)));
  EXPECT_EQ(text2->GetHypertext(), u"text2");

  AXPlatformNodeBase* text3 = static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(root->ChildAtIndex(2)));
  EXPECT_EQ(text3->GetHypertext(), u"text3");
}

TEST_F(AXPlatformNodeTest, GetHypertextIgnoredContainerSiblings) {
  // RootWebArea #1
  // ++genericContainer IGNORED #2
  // ++++StaticText "text1" #3
  // ++genericContainer IGNORED #4
  // ++++StaticText "text2" #5
  // ++genericContainer IGNORED #6
  // ++++StaticText "text3" #7
  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.child_ids = {2, 4, 6};

  AXNodeData container1, container2, container3;
  container1.id = 2;
  container1.role = ax::mojom::Role::kGenericContainer;
  container1.AddState(ax::mojom::State::kIgnored);
  container1.child_ids = {3};
  container2.id = 4;
  container2.role = ax::mojom::Role::kGenericContainer;
  container2.AddState(ax::mojom::State::kIgnored);
  container2.child_ids = {5};
  container3.id = 6;
  container3.role = ax::mojom::Role::kGenericContainer;
  container3.AddState(ax::mojom::State::kIgnored);
  container3.child_ids = {7};

  AXNodeData item1, item2, item3;
  item1.id = 3;
  item1.role = ax::mojom::Role::kStaticText;
  item1.SetName("text1");
  item2.id = 5;
  item2.role = ax::mojom::Role::kStaticText;
  item2.SetName("text2");
  item3.id = 7;
  item3.role = ax::mojom::Role::kStaticText;
  item3.SetName("text3");

  AXTreeUpdate update;
  update.root_id = 1;
  update.nodes = {root_data, container1, container2, container3,
                  item1,     item2,      item3};

  AXTree* tree = Init(update);

  // Set an AXMode on the AXPlatformNode as some platforms (auralinux) use it to
  // determine if it should enable accessibility.
  ScopedAXModeSetter ax_mode_setter(kAXModeComplete);

  AXPlatformNodeBase* root = static_cast<AXPlatformNodeBase*>(
      TestAXNodeWrapper::GetOrCreate(tree, tree->root())->ax_platform_node());

  EXPECT_EQ(root->GetHypertext(), u"text1text2text3");

  AXPlatformNodeBase* text1_ignored_container =
      static_cast<AXPlatformNodeBase*>(
          AXPlatformNode::FromNativeViewAccessible(root->ChildAtIndex(0)));
  EXPECT_EQ(text1_ignored_container->GetHypertext(), u"text1");

  AXPlatformNodeBase* text2_ignored_container =
      static_cast<AXPlatformNodeBase*>(
          AXPlatformNode::FromNativeViewAccessible(root->ChildAtIndex(1)));
  EXPECT_EQ(text2_ignored_container->GetHypertext(), u"text2");

  AXPlatformNodeBase* text3_ignored_container =
      static_cast<AXPlatformNodeBase*>(
          AXPlatformNode::FromNativeViewAccessible(root->ChildAtIndex(2)));
  EXPECT_EQ(text3_ignored_container->GetHypertext(), u"text3");
}

TEST_F(AXPlatformNodeTest, GetTextContentIgnoresInvisibleAndIgnored) {
  // kGroup
  // ++kStaticText "a"
  // ++kStaticText "b"
  // ++kGroup
  // ++++kStaticText "d"
  // ++++kStaticText "e"

  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kGroup;
  root_data.child_ids = {2, 3, 4};

  AXNodeData item1, item2, group1, item3, item4;
  item1.id = 2;
  item1.role = ax::mojom::Role::kStaticText;
  item1.SetName("a");
  item2.id = 3;
  item2.role = ax::mojom::Role::kStaticText;
  item2.SetName("b");

  group1.id = 4;
  group1.role = ax::mojom::Role::kGroup;
  group1.child_ids = {5, 6};

  item3.id = 5;
  item3.role = ax::mojom::Role::kStaticText;
  item3.SetName("d");
  item4.id = 6;
  item4.role = ax::mojom::Role::kStaticText;
  item4.SetName("e");

  AXTreeUpdate update;
  update.root_id = 1;
  update.nodes = {root_data, item1, item2, group1, item3, item4};

  AXTree* tree = Init(update);
  auto* root = static_cast<AXPlatformNodeBase*>(
      TestAXNodeWrapper::GetOrCreate(tree, tree->root())->ax_platform_node());

  // Set an AXMode on the AXPlatformNode as some platforms (auralinux) use it to
  // determine if it should enable accessibility.
  ScopedAXModeSetter ax_mode_setter(kAXModeComplete);

  EXPECT_EQ(root->GetTextContentUTF16(), u"abde");

  // Setting invisible or ignored on a static text node causes it to be included
  // or excluded from the root node's text content:
  {
    SetIsInvisible(tree, 2, true);
    EXPECT_EQ(root->GetTextContentUTF16(), u"bde");

    SetIsInvisible(tree, 2, false);
    EXPECT_EQ(root->GetTextContentUTF16(), u"abde");

    SetRole(tree, 2, ax::mojom::Role::kNone);
    EXPECT_EQ(root->GetTextContentUTF16(), u"bde");

    SetRole(tree, 2, ax::mojom::Role::kStaticText);
    EXPECT_EQ(root->GetTextContentUTF16(), u"abde");
  }

  // Setting invisible or ignored on a group node has no effect on the
  // text content:
  {
    SetIsInvisible(tree, 4, true);
    EXPECT_EQ(root->GetTextContentUTF16(), u"abde");

    SetRole(tree, 4, ax::mojom::Role::kNone);
    EXPECT_EQ(root->GetTextContentUTF16(), u"abde");
  }
}

TEST_F(AXPlatformNodeTest, TestMenuSelectedItems) {
  ScopedAXModeSetter ax_mode_setter(kAXModeComplete);

  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kMenu;

  AXNodeData item_1_data;
  item_1_data.id = 2;
  item_1_data.role = ax::mojom::Role::kMenuItem;
  item_1_data.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  AXNodeData item_2_data;
  item_2_data.id = 3;
  item_2_data.role = ax::mojom::Role::kMenuItem;

  root_data.child_ids = {item_1_data.id, item_2_data.id};

  AXTreeUpdate update;
  update.root_id = 1;
  update.nodes = {root_data, item_1_data, item_2_data};
  Init(update);

  AXTree& tree = *GetTree();
  auto* root = static_cast<AXPlatformNodeBase*>(
      TestAXNodeWrapper::GetOrCreate(&tree, tree.root())->ax_platform_node());

  int num = root->GetSelectionCount();
  EXPECT_EQ(num, 1);

  gfx::NativeViewAccessible first_child = root->ChildAtIndex(0);
  AXPlatformNodeBase* first_selected_node = root->GetSelectedItem(0);
  EXPECT_EQ(first_child, first_selected_node->GetNativeViewAccessible());
  EXPECT_EQ(nullptr, root->GetSelectedItem(1));
}

TEST_F(AXPlatformNodeTest, TestSelectedChildren) {
  ScopedAXModeSetter ax_mode_setter(kAXModeComplete);

  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kListBox;
  root_data.AddState(ax::mojom::State::kFocusable);
  root_data.child_ids = {2, 3};

  AXNodeData item_1_data;
  item_1_data.id = 2;
  item_1_data.role = ax::mojom::Role::kListBoxOption;
  item_1_data.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  AXNodeData item_2_data;
  item_2_data.id = 3;
  item_2_data.role = ax::mojom::Role::kListBoxOption;

  AXTreeUpdate update;
  update.root_id = 1;
  update.nodes = {root_data, item_1_data, item_2_data};
  Init(update);

  AXTree& tree = *GetTree();
  auto* root = static_cast<AXPlatformNodeBase*>(
      TestAXNodeWrapper::GetOrCreate(&tree, tree.root())->ax_platform_node());

  int num = root->GetSelectionCount();
  EXPECT_EQ(num, 1);

  gfx::NativeViewAccessible first_child = root->ChildAtIndex(0);
  AXPlatformNodeBase* first_selected_node = root->GetSelectedItem(0);
  EXPECT_EQ(first_child, first_selected_node->GetNativeViewAccessible());
  EXPECT_EQ(nullptr, root->GetSelectedItem(1));
}

TEST_F(AXPlatformNodeTest, TestSelectedChildrenWithGroup) {
  ScopedAXModeSetter ax_mode_setter(kAXModeComplete);

  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kListBox;
  root_data.AddState(ax::mojom::State::kFocusable);
  root_data.AddState(ax::mojom::State::kMultiselectable);
  root_data.child_ids = {2, 3};

  AXNodeData group_1_data;
  group_1_data.id = 2;
  group_1_data.role = ax::mojom::Role::kGroup;
  group_1_data.child_ids = {4, 5};

  AXNodeData group_2_data;
  group_2_data.id = 3;
  group_2_data.role = ax::mojom::Role::kGroup;
  group_2_data.child_ids = {6, 7};

  AXNodeData item_1_data;
  item_1_data.id = 4;
  item_1_data.role = ax::mojom::Role::kListBoxOption;
  item_1_data.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  AXNodeData item_2_data;
  item_2_data.id = 5;
  item_2_data.role = ax::mojom::Role::kListBoxOption;

  AXNodeData item_3_data;
  item_3_data.id = 6;
  item_3_data.role = ax::mojom::Role::kListBoxOption;

  AXNodeData item_4_data;
  item_4_data.id = 7;
  item_4_data.role = ax::mojom::Role::kListBoxOption;
  item_4_data.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  AXTreeUpdate update;
  update.root_id = 1;
  update.nodes = {root_data,   group_1_data, group_2_data, item_1_data,
                  item_2_data, item_3_data,  item_4_data};
  Init(update);

  AXTree& tree = *GetTree();
  auto* root = static_cast<AXPlatformNodeBase*>(
      TestAXNodeWrapper::GetOrCreate(&tree, tree.root())->ax_platform_node());

  int num = root->GetSelectionCount();
  EXPECT_EQ(num, 2);

  gfx::NativeViewAccessible first_group_child =
      static_cast<AXPlatformNodeBase*>(
          AXPlatformNode::FromNativeViewAccessible(root->ChildAtIndex(0)))
          ->ChildAtIndex(0);
  AXPlatformNodeBase* first_selected_node = root->GetSelectedItem(0);
  EXPECT_EQ(first_group_child, first_selected_node->GetNativeViewAccessible());

  gfx::NativeViewAccessible second_group_child =
      static_cast<AXPlatformNodeBase*>(
          AXPlatformNode::FromNativeViewAccessible(root->ChildAtIndex(1)))
          ->ChildAtIndex(1);
  AXPlatformNodeBase* second_selected_node = root->GetSelectedItem(1);
  EXPECT_EQ(second_group_child,
            second_selected_node->GetNativeViewAccessible());
}

TEST_F(AXPlatformNodeTest, TestSelectedChildrenMixed) {
  ScopedAXModeSetter ax_mode_setter(kAXModeComplete);

  // Build the below tree which is mixed with listBoxOption and group.
  // id=1 listBox FOCUSABLE MULTISELECTABLE (0, 0)-(0, 0) child_ids=2,3,4,9
  // ++id=2 listBoxOption (0, 0)-(0, 0) selected=true
  // ++id=3 group (0, 0)-(0, 0) child_ids=5,6
  // ++++id=5 listBoxOption (0, 0)-(0, 0) selected=true
  // ++++id=6 listBoxOption (0, 0)-(0, 0)
  // ++id=4 group (0, 0)-(0, 0) child_ids=7,8
  // ++++id=7 listBoxOption (0, 0)-(0, 0)
  // ++++id=8 listBoxOption (0, 0)-(0, 0) selected=true
  // ++id=9 listBoxOption (0, 0)-(0, 0) selected=true

  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kListBox;
  root_data.AddState(ax::mojom::State::kFocusable);
  root_data.AddState(ax::mojom::State::kMultiselectable);
  root_data.child_ids = {2, 3, 4, 9};

  AXNodeData item_1_data;
  item_1_data.id = 2;
  item_1_data.role = ax::mojom::Role::kListBoxOption;
  item_1_data.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  AXNodeData group_1_data;
  group_1_data.id = 3;
  group_1_data.role = ax::mojom::Role::kGroup;
  group_1_data.child_ids = {5, 6};

  AXNodeData item_2_data;
  item_2_data.id = 5;
  item_2_data.role = ax::mojom::Role::kListBoxOption;
  item_2_data.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  AXNodeData item_3_data;
  item_3_data.id = 6;
  item_3_data.role = ax::mojom::Role::kListBoxOption;

  AXNodeData group_2_data;
  group_2_data.id = 4;
  group_2_data.role = ax::mojom::Role::kGroup;
  group_2_data.child_ids = {7, 8};

  AXNodeData item_4_data;
  item_4_data.id = 7;
  item_4_data.role = ax::mojom::Role::kListBoxOption;

  AXNodeData item_5_data;
  item_5_data.id = 8;
  item_5_data.role = ax::mojom::Role::kListBoxOption;
  item_5_data.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  AXNodeData item_6_data;
  item_6_data.id = 9;
  item_6_data.role = ax::mojom::Role::kListBoxOption;
  item_6_data.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  AXTreeUpdate update;
  update.root_id = 1;
  update.nodes = {root_data,   item_1_data, group_1_data,
                  item_2_data, item_3_data, group_2_data,
                  item_4_data, item_5_data, item_6_data};
  Init(update);

  AXTree& tree = *GetTree();
  auto* root = static_cast<AXPlatformNodeBase*>(
      TestAXNodeWrapper::GetOrCreate(&tree, tree.root())->ax_platform_node());

  int num = root->GetSelectionCount();
  EXPECT_EQ(num, 4);

  gfx::NativeViewAccessible first_child = root->ChildAtIndex(0);
  AXPlatformNodeBase* first_selected_node = root->GetSelectedItem(0);
  EXPECT_EQ(first_child, first_selected_node->GetNativeViewAccessible());

  gfx::NativeViewAccessible first_group_child =
      static_cast<AXPlatformNodeBase*>(
          AXPlatformNode::FromNativeViewAccessible(root->ChildAtIndex(1)))
          ->ChildAtIndex(0);
  AXPlatformNodeBase* second_selected_node = root->GetSelectedItem(1);
  EXPECT_EQ(first_group_child, second_selected_node->GetNativeViewAccessible());

  gfx::NativeViewAccessible second_group_child =
      static_cast<AXPlatformNodeBase*>(
          AXPlatformNode::FromNativeViewAccessible(root->ChildAtIndex(2)))
          ->ChildAtIndex(1);
  AXPlatformNodeBase* third_selected_node = root->GetSelectedItem(2);
  EXPECT_EQ(second_group_child, third_selected_node->GetNativeViewAccessible());

  gfx::NativeViewAccessible fourth_child = root->ChildAtIndex(3);
  AXPlatformNodeBase* fourth_selected_node = root->GetSelectedItem(3);
  EXPECT_EQ(fourth_child, fourth_selected_node->GetNativeViewAccessible());
}

TEST_F(AXPlatformNodeTest, CompareTo) {
  // Compare the nodes' logical orders for the following tree. Node name is
  // denoted according to its id (i.e. "n#" is id#). Nodes that have smaller ids
  // are always logically less than nodes with bigger ids.
  //
  //        n1
  //        |
  //      __ n2 ___
  //    /      \    \
  //   n3 _     n8   n9
  //  / \   \         \
  // n4  n5  n6       n10
  //         /
  //        n7
  ScopedAXModeSetter ax_mode_setter(kAXModeComplete);
  AXNodeData node1;
  node1.id = 1;
  node1.role = ax::mojom::Role::kRootWebArea;
  node1.child_ids = {2};

  AXNodeData node2;
  node2.id = 2;
  node2.role = ax::mojom::Role::kStaticText;
  node2.child_ids = {3, 8, 9};

  AXNodeData node3;
  node3.id = 3;
  node3.role = ax::mojom::Role::kStaticText;
  node3.child_ids = {4, 5, 6};

  AXNodeData node4;
  node4.id = 4;
  node4.role = ax::mojom::Role::kStaticText;

  AXNodeData node5;
  node5.id = 5;
  node5.role = ax::mojom::Role::kStaticText;

  AXNodeData node6;
  node6.id = 6;
  node6.role = ax::mojom::Role::kStaticText;
  node6.child_ids = {7};

  AXNodeData node7;
  node7.id = 7;
  node7.role = ax::mojom::Role::kStaticText;

  AXNodeData node8;
  node8.id = 8;
  node8.role = ax::mojom::Role::kStaticText;

  AXNodeData node9;
  node9.id = 9;
  node9.role = ax::mojom::Role::kStaticText;
  node9.child_ids = {10};

  AXNodeData node10;
  node10.id = 10;
  node10.role = ax::mojom::Role::kStaticText;

  AXTreeUpdate update;
  update.root_id = 1;
  update.nodes = {node1, node2, node3, node4, node5,
                  node6, node7, node8, node9, node10};

  Init(update);

  AXTree& tree = *GetTree();
  // Retrieve the nodes in a level-order traversal way.
  auto* n1 = static_cast<AXPlatformNodeBase*>(
      TestAXNodeWrapper::GetOrCreate(&tree, tree.root())->ax_platform_node());
  auto* n2 = static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(n1->ChildAtIndex(0)));
  auto* n3 = static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(n2->ChildAtIndex(0)));
  auto* n8 = static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(n2->ChildAtIndex(1)));
  auto* n9 = static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(n2->ChildAtIndex(2)));
  auto* n4 = static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(n3->ChildAtIndex(0)));
  auto* n5 = static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(n3->ChildAtIndex(1)));
  auto* n6 = static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(n3->ChildAtIndex(2)));
  auto* n10 = static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(n9->ChildAtIndex(0)));
  auto* n7 = static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(n6->ChildAtIndex(0)));

  // Test for two nodes that do not share the same root. They should not be
  // comparable.
  AXPlatformNodeDelegate detached_delegate;
  AXPlatformNodeBase* detached_node = static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::Create(&detached_delegate));
  EXPECT_EQ(std::nullopt, n1->CompareTo(*detached_node));
  detached_node->Destroy();
  detached_node = nullptr;

  // Create a test vector of all the tree nodes arranged in a pre-order
  // traversal way. The node that has a smaller index in the vector should also
  // be logically less (comes before) the nodes with bigger index.
  std::vector<AXPlatformNodeBase*> preorder_tree_nodes = {n1, n2, n3, n4, n5,
                                                          n6, n7, n8, n9, n10};
  // Test through all permutations of lhs/rhs comparisons of nodes from
  // |preorder_tree_nodes|.
  for (auto* lhs : preorder_tree_nodes) {
    for (auto* rhs : preorder_tree_nodes) {
      int expected_result = 0;
      if (lhs->GetData().id < rhs->GetData().id)
        expected_result = -1;
      else if (lhs->GetData().id > rhs->GetData().id)
        expected_result = 1;

      EXPECT_NE(std::nullopt, lhs->CompareTo(*rhs));
      int actual_result = 0;
      if (lhs->CompareTo(*rhs) < 0)
        actual_result = -1;
      else if (lhs->CompareTo(*rhs) > 0)
        actual_result = 1;

      SCOPED_TRACE(::testing::Message()
                   << "lhs.id=" << base::NumberToString(lhs->GetData().id)
                   << ", rhs.id=" << base::NumberToString(rhs->GetData().id)
                   << ", lhs->CompareTo(*rhs)={actual:"
                   << base::NumberToString(actual_result) << ", expected:"
                   << base::NumberToString(expected_result) << "}");

      EXPECT_EQ(expected_result, actual_result);
    }
  }
}

TEST_F(AXPlatformNodeTest, HypertextOffsetFromEndpoint) {
  // <p>
  //   <a href="google.com">link</a>
  // </p>
  //
  // kRootWebArea
  // ++kParagraph
  // ++++kLink
  // ++++++kStaticText "link"
  // ++++++kStaticText "link#2"
  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.child_ids = {2};

  AXNodeData container1, link1, item1, item2;
  container1.id = 2;
  container1.role = ax::mojom::Role::kParagraph;
  container1.child_ids = {3};
  link1.id = 3;
  link1.role = ax::mojom::Role::kLink;
  link1.child_ids = {4, 5};
  item1.id = 4;
  item1.role = ax::mojom::Role::kStaticText;
  item1.SetName("link");
  item2.id = 5;
  item2.role = ax::mojom::Role::kStaticText;
  item2.SetName("link#2");

  AXTreeUpdate update;
  update.root_id = 1;
  update.nodes = {root_data, container1, link1, item1, item2};

  AXTree* tree = Init(update);
  auto* root = static_cast<AXPlatformNodeBase*>(
      TestAXNodeWrapper::GetOrCreate(tree, tree->root())->ax_platform_node());

  // Set an AXMode on the AXPlatformNode as some platforms (auralinux) use it to
  // determine if it should enable accessibility.
  ScopedAXModeSetter ax_mode_setter(kAXModeComplete);

  auto* paragraph = static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(root->ChildAtIndex(0)));

  auto* link = static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(paragraph->ChildAtIndex(0)));

  auto* static_text = static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(link->ChildAtIndex(0)));

  auto* static_text2 = static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(link->ChildAtIndex(1)));

  // End point is a parent, points before/after the link.
  {
    EXPECT_EQ(link->GetHypertextOffsetFromEndpoint(paragraph, 0), 0);
    EXPECT_EQ(link->GetHypertextOffsetFromEndpoint(paragraph, 1), 10);
  }

  // End point is a parent, points before/after the static texts.
  {
    EXPECT_EQ(static_text->GetHypertextOffsetFromEndpoint(link, 0), 0);
    EXPECT_EQ(static_text->GetHypertextOffsetFromEndpoint(link, 1), 4);
    EXPECT_EQ(static_text->GetHypertextOffsetFromEndpoint(link, 2), 4);

    EXPECT_EQ(static_text2->GetHypertextOffsetFromEndpoint(link, 0), 0);
    EXPECT_EQ(static_text2->GetHypertextOffsetFromEndpoint(link, 1), 0);
    EXPECT_EQ(static_text2->GetHypertextOffsetFromEndpoint(link, 2), 6);
  }

  // End point is a grand parent, points before/after the static text.
  {
    EXPECT_EQ(static_text->GetHypertextOffsetFromEndpoint(paragraph, 0), 0);
    EXPECT_EQ(static_text->GetHypertextOffsetFromEndpoint(paragraph, 1), 4);
  }

  // End point is |this|, points into |this| text leaf object.
  {
    EXPECT_EQ(static_text->GetHypertextOffsetFromEndpoint(static_text, 0), 0);
    EXPECT_EQ(static_text->GetHypertextOffsetFromEndpoint(static_text, 4), 4);
  }

  // End point is |this|, points into |this| hypertext object.
  {
    EXPECT_EQ(link->GetHypertextOffsetFromEndpoint(link, 0), 0);
    EXPECT_EQ(link->GetHypertextOffsetFromEndpoint(link, 1), 4);
  }
}

}  // namespace ui
