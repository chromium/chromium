// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_UNITTEST_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_UNITTEST_H_

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

class AXPlatformNodeTest : public testing::Test, public AXTreeManager {
 public:
  AXPlatformNodeTest();
  ~AXPlatformNodeTest() override;

  // Initialize given an AXTreeUpdate.
  void Init(const AXTreeUpdate& initial_state);

  // Convenience functions to initialize directly from a few AXNodeData objects.
  void Init(const ui::AXNodeData& node1,
            const ui::AXNodeData& node2 = ui::AXNodeData(),
            const ui::AXNodeData& node3 = ui::AXNodeData(),
            const ui::AXNodeData& node4 = ui::AXNodeData(),
            const ui::AXNodeData& node5 = ui::AXNodeData(),
            const ui::AXNodeData& node6 = ui::AXNodeData(),
            const ui::AXNodeData& node7 = ui::AXNodeData(),
            const ui::AXNodeData& node8 = ui::AXNodeData(),
            const ui::AXNodeData& node9 = ui::AXNodeData(),
            const ui::AXNodeData& node10 = ui::AXNodeData(),
            const ui::AXNodeData& node11 = ui::AXNodeData(),
            const ui::AXNodeData& node12 = ui::AXNodeData());

  // AXTreeManager implementation.
  AXNode* GetNodeFromTree(const ui::AXTreeID tree_id,
                          const int32_t node_id) const override;
  AXTreeID GetTreeID() const override;
  AXTreeID GetParentTreeID() const override;
  ui::AXNode* GetRootAsAXNode() const override;
  ui::AXNode* GetParentNodeFromParentTreeAsAXNode() const override;

 protected:
  AXNode* GetRootNode() const { return tree_->root(); }

  AXTreeUpdate BuildTextField();
  AXTreeUpdate BuildTextFieldWithSelectionRange(int32_t start, int32_t stop);
  AXTreeUpdate BuildContentEditable();
  AXTreeUpdate BuildContentEditableWithSelectionRange(int32_t start,
                                                      int32_t end);
  AXTreeUpdate Build3X3Table();
  AXTreeUpdate BuildAriaColumnAndRowCountGrids();

  AXTreeUpdate BuildListBox(bool option_1_is_selected,
                            bool option_2_is_selected,
                            bool option_3_is_selected,
                            ax::mojom::State additional_state);

  std::unique_ptr<AXTree> tree_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_UNITTEST_H_
