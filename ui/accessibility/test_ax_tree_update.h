// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_TEST_AX_TREE_UPDATE_H_
#define UI_ACCESSIBILITY_TEST_AX_TREE_UPDATE_H_

#include "ui/accessibility/ax_tree_update.h"

namespace ui {

// These utility classes, help construct an AXTreeUpdate  together with all of
// the updated nodes more easily. Only for use in tests for constructing /
// updating simple accessibility trees.

// Used to construct AXTreeUpdate node.
struct TestAXTreeUpdateNode final {
  TestAXTreeUpdateNode() = delete;
  ~TestAXTreeUpdateNode();

  TestAXTreeUpdateNode(const TestAXTreeUpdateNode&);
  TestAXTreeUpdateNode(TestAXTreeUpdateNode&&);

  TestAXTreeUpdateNode(ax::mojom::Role role,
                       const std::vector<TestAXTreeUpdateNode>& children);
  TestAXTreeUpdateNode(ax::mojom::Role role,
                       ax::mojom::State state,
                       const std::vector<TestAXTreeUpdateNode>& children);
  TestAXTreeUpdateNode(const std::string& text);

  AXNodeData data;
  std::vector<TestAXTreeUpdateNode> children;
};

// Used to construct an accessible tree from a hierarchical list of nodes
// {<node_properties>, {<node_children>}}. For example,
// {Role::kRootWebArea, {"text"}} will create the following tree:
// kRootWebArea
// ++kStaticText "text"
class TestAXTreeUpdate final : public AXTreeUpdate {
 public:
  TestAXTreeUpdate(const TestAXTreeUpdateNode& root);

  TestAXTreeUpdate(const TestAXTreeUpdate&) = delete;
  TestAXTreeUpdate& operator=(const TestAXTreeUpdate&) = delete;

 private:
  // Recursively creates the tree update structure.
  AXNodeID SetSubtree(const TestAXTreeUpdateNode& node);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_TEST_AX_TREE_UPDATE_H_
