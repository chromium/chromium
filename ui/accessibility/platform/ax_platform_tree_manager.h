// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_TREE_MANAGER_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_TREE_MANAGER_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/accessibility/ax_node.h"
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
  explicit AXPlatformTreeManager(std::unique_ptr<AXTree> tree);
  AXPlatformTreeManager(const AXPlatformTreeManager&) = delete;
  AXPlatformTreeManager& operator=(const AXPlatformTreeManager&) = delete;
  ~AXPlatformTreeManager() override;

  // Returns an AXPlatformNode with the specified and |node_id|.
  virtual AXPlatformNode* GetPlatformNodeFromTree(
      const AXNodeID node_id) const = 0;

  // Returns an AXPlatformNode that corresponds to the given |node|.
  virtual AXPlatformNode* GetPlatformNodeFromTree(const AXNode& node) const = 0;

  // Returns an AXPlatformNodeDelegate that corresponds to a root node
  // of the accessibility tree.
  virtual AXPlatformNodeDelegate* RootDelegate() const = 0;

  bool IsPlatformTreeManager() const override;

  base::WeakPtr<AXPlatformTreeManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Fire a sentinel event and wait until it is received, to ensure all pending
  // notifications are processed.
  // Note: not all platforms need this. For example, IA2 can listen to events
  // synchronously.
  virtual void FireSentinelEventForTesting();

 private:
  base::WeakPtrFactory<AXPlatformTreeManager> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_TREE_MANAGER_H_
