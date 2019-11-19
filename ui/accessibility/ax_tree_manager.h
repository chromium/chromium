// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_MANAGER_H_
#define UI_ACCESSIBILITY_AX_TREE_MANAGER_H_

#include "base/macros.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ui {

// Each AXNode has access to its own tree, but a manager of multiple AXTrees
// is necessary for operations that span across trees.
class AX_EXPORT AXTreeManager {
 public:
  // Exposes the mapping between AXTreeID's and AXNodes based on a node_id.
  // This allows for callers to access nodes outside of their own tree.
  // Returns nullptr if the AXTreeID or node_id is not found.
  virtual AXNode* GetNodeFromTree(const AXTreeID tree_id,
                                  const int32_t node_id) const = 0;

  // Returns the tree id of the tree managed by this AXTreeManager.
  virtual AXTreeID GetTreeID() const = 0;

  // Returns the tree id for the parent node of the child with the provided
  // child_node_id and child_tree_id. This allows callers to access parent
  // nodes outside their own tree.
  virtual AXTreeID GetParentTreeID() const = 0;

  // Return a pointer to the root of the tree.
  virtual AXNode* GetRootAsAXNode() const = 0;

  // If this tree has a parent tree, return the parent node in that tree.
  virtual AXNode* GetParentNodeFromParentTreeAsAXNode() const = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_MANAGER_H_
