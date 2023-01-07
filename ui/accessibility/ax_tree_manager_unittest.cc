// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/test_ax_tree_manager.h"
#include "ui/accessibility/ax_node.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(AXTreeManagerTest, ConstructFromInitialState) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  std::string name = "Hello";
  root.AddStringAttribute(ax::mojom::StringAttribute::kName, name);

  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.push_back(root);
  initial_state.has_tree_data = true;

  TestAXTreeManager manager(std::make_unique<AXSerializableTree>());

  manager.Initialize(initial_state);

  AXNode* returned_root = manager.GetRoot();
  ASSERT_EQ(
      returned_root->GetStringAttribute(ax::mojom::StringAttribute::kName),
      name);
}

}  // namespace ui
