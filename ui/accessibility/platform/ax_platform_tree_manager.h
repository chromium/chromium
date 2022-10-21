// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_TREE_MANAGER_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_TREE_MANAGER_H_

#include <map>
#include <memory>

#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

class AXNode;
class AXPlatformNode;
class AXTreeID;

// Abstract class for a class that owns an AXTree and manages its
// connections to other AXTrees in the same page or desktop (parent and child
// trees). (This is still abstract because its parent class `AXTreeManager` has
// pure virtual methods.)
class AX_EXPORT AXPlatformTreeManager : public AXTreeManager {
 public:
  ~AXPlatformTreeManager() override;

  AXPlatformTreeManager(const AXPlatformTreeManager&) = delete;
  AXPlatformTreeManager& operator=(const AXPlatformTreeManager&) = delete;

  // Returns an `AXPlatformNode`, (a wrapper class) that adapts the `AXNode`
  // with the specified `node_id` to the interface used by the current
  // platform's accessibility APIs.
  AXPlatformNode* GetPlatformNode(const AXNodeID& node_id) const;

  // Returns an `AXPlatformNode`, (a wrapper class) that adapts the specified
  // `AXNode` to the interface used by the current platform's accessibility
  // APIs.
  AXPlatformNode* GetPlatformNode(const AXNode& node) const;

  // Returns an `AXPlatformNodeDelegate` that corresponds to the `AXNode` with
  // the specified `node_id`.
  AXPlatformNodeDelegate* GetPlatformNodeDelegate(
      const AXNodeID& node_id) const;

  // Returns an `AXPlatformNodeDelegate` that corresponds to the specified
  // `AXNode`.
  AXPlatformNodeDelegate* GetPlatformNodeDelegate(const AXNode& node) const;

  // Returns an `AXPlatformNodeDelegate` that corresponds to the root node
  // of the managed accessibility tree.
  AXPlatformNodeDelegate* GetPlatformNodeDelegateForRoot() const;

 protected:
  AXPlatformTreeManager(const AXTreeID& tree_id, std::unique_ptr<AXTree> tree);

  // Sets the `AXPlatformNodeDelegate` that corresponds to the specified
  // `AXNode`. If a delegate is already set, the existing one is destroyed and
  // the given one is put in its place.
  void SetPlatformNodeDelegate(
      const AXNode& node,
      std::unique_ptr<AXPlatformNodeDelegate> delegate);

  // Unsets the `AXPlatformNodeDelegate` that corresponds to the specified
  // `AXNodeID` or `AXNode`, and returns ownership of the delegate back to the
  // caller.
  std::unique_ptr<AXPlatformNodeDelegate> UnsetPlatformNodeDelegate(
      const AXNodeID& node_id);
  std::unique_ptr<AXPlatformNodeDelegate> UnsetPlatformNodeDelegate(
      const AXNode& node);

 private:
  // A mapping from an `AXNodeID`, i.e. a specific `AXNode` in the `AXTree`,
  // to a platform specific delegate that can carry out extra tasks that are
  // still not incorporated into `AXNode`.
  //
  // This is different from the map in `AXTree`, which does not contain
  // extra mac nodes from `AXTableInfo`.
  //
  // TODO(accessibility) Find a way to have a single map for both, perhaps
  // by turning `BrowserAccessibility` into a subclass of `AXNode`.
  std::map<AXNodeID, std::unique_ptr<AXPlatformNodeDelegate>> id_wrapper_map_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_TREE_MANAGER_H_
