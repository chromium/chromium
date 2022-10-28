// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_DUMMY_TREE_MANAGER_H_
#define UI_ACCESSIBILITY_AX_DUMMY_TREE_MANAGER_H_

#include <memory>

#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager.h"

namespace ui {

class AXNode;

// A basic implementation of AXTreeManager.
//
// For simplicity, this class supports only a single tree and doesn't perform
// any walking across multiple trees.
class AX_EXPORT AXDummyTreeManager : public AXTreeManager {
 public:
  // This constructor does not create an empty AXTree. Call "SetTree" if you
  // need to manage a specific tree. Useful when you need to test for the
  // situation when no AXTree has been loaded yet.
  AXDummyTreeManager();

  // Takes ownership of |tree|.
  explicit AXDummyTreeManager(std::unique_ptr<AXTree> tree);

  ~AXDummyTreeManager() override;

  AXDummyTreeManager(const AXDummyTreeManager& manager) = delete;
  AXDummyTreeManager& operator=(const AXDummyTreeManager& manager) = delete;

  AXDummyTreeManager(AXDummyTreeManager&& manager);
  AXDummyTreeManager& operator=(AXDummyTreeManager&& manager);

  void DestroyTree();
  AXTree* GetTree() const;
  // Takes ownership of |tree|.
  void SetTree(std::unique_ptr<AXTree> tree);

  // AXTreeManager implementation.
  AXNode* GetParentNodeFromParentTree() const override;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_DUMMY_TREE_MANAGER_H_
