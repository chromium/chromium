// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/test_ax_tree_update.h"

namespace ui {

TestAXTreeUpdateNode::TestAXTreeUpdateNode(const TestAXTreeUpdateNode&) =
    default;

TestAXTreeUpdateNode::TestAXTreeUpdateNode(TestAXTreeUpdateNode&&) = default;

TestAXTreeUpdateNode::~TestAXTreeUpdateNode() = default;

TestAXTreeUpdateNode::TestAXTreeUpdateNode(
    ax::mojom::Role role,
    const std::vector<TestAXTreeUpdateNode>& children)
    : children(children) {
  DCHECK_NE(role, ax::mojom::Role::kUnknown);
  data.role = role;
}

TestAXTreeUpdateNode::TestAXTreeUpdateNode(
    ax::mojom::Role role,
    ax::mojom::State state,
    const std::vector<TestAXTreeUpdateNode>& children)
    : children(children) {
  DCHECK_NE(role, ax::mojom::Role::kUnknown);
  DCHECK_NE(state, ax::mojom::State::kNone);
  data.role = role;
  data.AddState(state);
}

TestAXTreeUpdateNode::TestAXTreeUpdateNode(const std::string& text) {
  data.role = ax::mojom::Role::kStaticText;
  data.SetName(text);
}

TestAXTreeUpdate::TestAXTreeUpdate(const TestAXTreeUpdateNode& root) {
  root_id = SetSubtree(root);
}

AXNodeID TestAXTreeUpdate::SetSubtree(const TestAXTreeUpdateNode& node) {
  size_t node_index = nodes.size();
  nodes.push_back(node.data);
  nodes[node_index].id = node_index + 1;
  std::vector<AXNodeID> child_ids;
  for (const auto& child : node.children) {
    child_ids.push_back(SetSubtree(child));
  }
  nodes[node_index].child_ids = child_ids;
  return nodes[node_index].id;
}

}  // namespace ui
