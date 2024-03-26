// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_TEST_AX_TREE_UPDATE_H_
#define UI_ACCESSIBILITY_TEST_AX_TREE_UPDATE_H_

#include <set>
#include <unordered_map>

#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

struct AXNodeData;
struct AXTreeData;
class AXTreeID;

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
  explicit TestAXTreeUpdateNode(const std::string& text);

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
  explicit TestAXTreeUpdate(const TestAXTreeUpdateNode& root);

  // Returns an `AXTreeUpdate` from a tree structure string of the format:
  // ++<id> <role> <name?>="<name>" <state?>=<state_value>,<state_value>
  // Where the properties denoted with a `?` are optional.
  // For example:
  // ++++4 kTextField name="placeholder" state=kEditable,kFocusable 
  //
  // Other supported properties are:
  // * intAttribute=<intAttributeType>,<intAttributeValue>
  // * stringAttribute=<stringAttributeType>,<stringAttributeValue>
  // * boolAttribute=<boolAttributeType>,<boolAttributeValue>
  //
  // Some of the expectations this function makes are:
  // * Roles and states are always valid.
  // * Roles and states are prefixed by a 'k'.
  // * The `name` property value passed in is surrounded by double quotes.
  // * Properties that may have multiple possible values passed in have those
  // values separated ONLY by a comma.
  // * There is always a newline separating each node of the tree.
  // 
  // LIMITATIONS:
  // - Because the parsing uses newline characters to distinguish between lines
  // (where each line is one node), names for nodes can't be newline characters.
  // - For similar reasons, spaces, '+', '=', commas, and double quotes can't be
  // used as or within node names.
  // Follow up CLs will be made so alleviate these limitations.
  explicit TestAXTreeUpdate(const std::string& tree_structure);

  TestAXTreeUpdate(const TestAXTreeUpdate&) = delete;
  TestAXTreeUpdate& operator=(const TestAXTreeUpdate&) = delete;

 private:
  // Recursively creates the tree update structure.
  AXNodeID SetSubtree(const TestAXTreeUpdateNode& node);

};
}  // namespace ui

#endif  // UI_ACCESSIBILITY_TEST_AX_TREE_UPDATE_H_
