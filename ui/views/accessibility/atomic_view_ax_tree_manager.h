// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_ATOMIC_VIEW_AX_TREE_MANAGER_H_
#define UI_VIEWS_ACCESSIBILITY_ATOMIC_VIEW_AX_TREE_MANAGER_H_

#include <memory>
#include <string>
#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"
#include "ui/views/accessibility/view_ax_platform_node_delegate.h"

namespace views {

// This class manages a "tree" which consists of a single node. This is intended
// for use with Views, enabling the UIA ITextProvider and ITextRangeProvider
// functionalities. This class is TEMPORARY and not a long term solution.
// TODO(crbug.com/40924888): Remove this temporary class once the ViewsAX
// project is completed.
class VIEWS_EXPORT AtomicViewAXTreeManager : public ui::AXPlatformTreeManager {
 public:
  static std::unique_ptr<AtomicViewAXTreeManager> Create(
      ViewAXPlatformNodeDelegate* delegate,
      ui::AXNodeData node_data);
  friend std::unique_ptr<AtomicViewAXTreeManager> Create(
      ViewAXPlatformNodeDelegate* delegate,
      ui::AXNodeData node_data);

  ~AtomicViewAXTreeManager() override;

  // AXTreeManager overrides.
  bool IsView() const override;
  ui::AXNode* GetNodeFromTree(const ui::AXTreeID& tree_id,
                              const ui::AXNodeID node_id) const override;
  ui::AXNode* GetNode(const ui::AXNodeID node_id) const override;
  ui::AXPlatformNode* GetPlatformNodeFromTree(
      const ui::AXNodeID node_id) const override;
  ui::AXPlatformNode* GetPlatformNodeFromTree(const ui::AXNode&) const override;
  ui::AXPlatformNodeDelegate* RootDelegate() const override;
  ui::AXTreeID GetParentTreeID() const override;
  ui::AXNode* GetRoot() const override;
  ui::AXNode* GetParentNodeFromParentTree() const override;

  void ClearComputedRootData();

 private:
  explicit AtomicViewAXTreeManager(ViewAXPlatformNodeDelegate* delegate,
                                   ui::AXNodeData node_data);

  raw_ptr<ViewAXPlatformNodeDelegate> delegate_;
};

}  // namespace views
#endif  // UI_VIEWS_ACCESSIBILITY_ATOMIC_VIEW_AX_TREE_MANAGER_H_
