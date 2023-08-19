// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_TREE_MANAGER_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_TREE_MANAGER_H_

#include "base/component_export.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager.h"

namespace ui {

class AXPlatformNode;
class AXPlatformNodeDelegate;

// Abstract interface for a class that manages AXPlatformNodes and is
// able to query for them via `GetPlatformNodeFromTree`. Extends
// AXTreeManager, so AXNodes are also managed.
class COMPONENT_EXPORT(AX_PLATFORM) AXPlatformTreeManager
    : public AXTreeManager {
 public:
  // Returns an AXPlatformNode with the specified and |node_id|.
  virtual AXPlatformNode* GetPlatformNodeFromTree(
      const AXNodeID node_id) const = 0;

  // Returns an AXPlatformNode that corresponds to the given |node|.
  virtual AXPlatformNode* GetPlatformNodeFromTree(const AXNode& node) const = 0;

  // Returns an AXPlatformNodeDelegate that corresponds to a root node
  // of the accessibility tree.
  virtual AXPlatformNodeDelegate* RootDelegate() const = 0;

  bool IsPlatformTreeManager() const override;

 protected:
  explicit AXPlatformTreeManager(std::unique_ptr<AXTree> tree)
      : AXTreeManager(std::move(tree)) {}
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_TREE_MANAGER_H_
