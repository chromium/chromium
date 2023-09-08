// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_TEST_SINGLE_AX_TREE_MANAGER_H_
#define UI_ACCESSIBILITY_TEST_SINGLE_AX_TREE_MANAGER_H_

#include <memory>

#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager.h"

namespace ui {

class AXNode;
struct AXTreeUpdate;

// Test only.
//
// A basic implementation of AXTreeManager that supports a single tree,
// and doesn't perform any walking across multiple trees.
class AX_EXPORT TestSingleAXTreeManager : public AXTreeManager {
 public:
  // This constructor does not create an empty AXTree. Call "SetTree" if you
  // need to manage a specific tree. Useful when you need to test for the
  // situation when no AXTree has been loaded yet.
  TestSingleAXTreeManager();

  // Takes ownership of |tree|.
  explicit TestSingleAXTreeManager(std::unique_ptr<AXTree> tree);

  ~TestSingleAXTreeManager() override;

  TestSingleAXTreeManager(const TestSingleAXTreeManager& manager) = delete;
  TestSingleAXTreeManager& operator=(const TestSingleAXTreeManager& manager) =
      delete;

  TestSingleAXTreeManager(TestSingleAXTreeManager&& manager);
  TestSingleAXTreeManager& operator=(TestSingleAXTreeManager&& manager);

  void DestroyTree();
  AXTree* GetTree() const;

  // Takes ownership of |tree|.
  void SetTree(std::unique_ptr<AXTree> tree);

  // Creates and set the tree by a given AXTreeUpdate instance.
  AXTree* Init(AXTreeUpdate tree_update);

  // Convenience functions to initialize directly from a few AXNodeData objects.
  AXTree* Init(const AXNodeData& node1,
               const AXNodeData& node2 = AXNodeData(),
               const AXNodeData& node3 = AXNodeData(),
               const AXNodeData& node4 = AXNodeData(),
               const AXNodeData& node5 = AXNodeData(),
               const AXNodeData& node6 = AXNodeData(),
               const AXNodeData& node7 = AXNodeData(),
               const AXNodeData& node8 = AXNodeData(),
               const AXNodeData& node9 = AXNodeData(),
               const AXNodeData& node10 = AXNodeData(),
               const AXNodeData& node11 = AXNodeData(),
               const AXNodeData& node12 = AXNodeData());

  // Creates a tree position, a simple wrapper around
  // AXNodePosition::CreateTreePosition.
  AXNodePosition::AXPositionInstance CreateTreePosition(const AXNode& anchor,
                                                        int child_index) const;

  // Creates a tree position for the given |anchor_data| of a node from the
  // given tree.
  AXNodePosition::AXPositionInstance CreateTreePosition(
      const AXTree* tree,
      const AXNodeData& anchor_data,
      int child_index) const;

  // Creates a tree position for the given |anchor_data| of a node from the
  // current tree.
  AXNodePosition::AXPositionInstance CreateTreePosition(
      const AXNodeData& anchor_data,
      int child_index) const;

  // Creates a text position, a simple wrapper around
  // AXNodePosition::CreateTextPosition.
  AXNodePosition::AXPositionInstance CreateTextPosition(
      const AXNode& anchor,
      int text_offset,
      ax::mojom::TextAffinity affinity) const;

  // Creates a text position for the given |anchor_data| of a node from the
  // given tree.
  AXNodePosition::AXPositionInstance CreateTextPosition(
      const AXTree* tree,
      const AXNodeData& anchor_data,
      int text_offset,
      ax::mojom::TextAffinity affinity) const;

  // Creates a text position for the given |anchor_data| belonging to the
  // current tree.
  AXNodePosition::AXPositionInstance CreateTextPosition(
      const AXNodeData& anchor_data,
      int text_offset,
      ax::mojom::TextAffinity affinity) const;

  // Creates a text position for the given |anchor_id| of a node from the
  // given tree.
  AXNodePosition::AXPositionInstance CreateTextPosition(
      const AXNodeID& anchor_id,
      int text_offset,
      ax::mojom::TextAffinity affinity) const;

  // AXTreeManager implementation.
  AXNode* GetParentNodeFromParentTree() const override;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_TEST_SINGLE_AX_TREE_MANAGER_H_
