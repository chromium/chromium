// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_MANAGER_BASE_H_
#define UI_ACCESSIBILITY_AX_TREE_MANAGER_BASE_H_

#include <memory>
#include <optional>
#include <unordered_map>

#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

class AXNode;

// A class that owns an accessibility tree and manages its connections to other
// accessibility trees. Each accessibility tree could be connected to one parent
// tree and multiple child trees, the whole collection essentially being a
// forest of trees representing user interface elements. Each tree is typically
// backed by a rendering surface.
//
// This class is movable but not copyable.
class AX_EXPORT AXTreeManagerBase final {
 public:
  // Returns the manager of the tree with the given ID, if any.
  static AXTreeManagerBase* GetManager(const AXTreeID& tree_id);

  // Returns the node identified by the given `tree_id` and `node_id`. This
  // allows for callers to access nodes outside of their own tree.
  static AXNode* GetNodeFromTree(const AXTreeID& tree_id,
                                 const AXNodeID& node_id);

  // This default constructor does not create an empty accessibility tree. Call
  // `SetTree` if you need to manage a specific tree.
  AXTreeManagerBase();

  explicit AXTreeManagerBase(std::unique_ptr<AXTree> tree);
  explicit AXTreeManagerBase(const AXTreeUpdate& initial_state);
  virtual ~AXTreeManagerBase();
  AXTreeManagerBase(const AXTreeManagerBase&) = delete;
  AXTreeManagerBase& operator=(const AXTreeManagerBase&) = delete;
  AXTreeManagerBase(AXTreeManagerBase&& manager);
  AXTreeManagerBase& operator=(AXTreeManagerBase&& manager);

  // Returns a pointer to the managed tree, if any.
  AXTree* GetTree() const;

  // Takes ownership of a new accessibility tree and returns the one that is
  // currently being managed. It is considered an error to pass an empty
  // unique_ptr for `tree`. If no tree is currently being managed, returns an
  // empty unique_ptr.
  std::unique_ptr<AXTree> SetTree(std::unique_ptr<AXTree> tree);
  std::unique_ptr<AXTree> SetTree(const AXTreeUpdate& initial_state);

  // Detaches the managed tree, if any.
  std::unique_ptr<AXTree> ReleaseTree();

  // Returns a snapshot of the managed tree by serializing it into an
  // `AXTreeUpdate`.
  AXTreeUpdate SnapshotTree() const;

  // Tries to update the managed tree by unserializing and applying the provided
  // `update`. Returns a boolean value indicating success or failure.
  bool ApplyTreeUpdate(const AXTreeUpdate& update);

  // Returns the ID of the managed tree.
  //
  // Note that tree IDs are expensive to copy, hence the use of a const
  // reference.
  const AXTreeID& GetTreeID() const;

  // Returns the ID of the parent tree.
  //
  // Note that tree IDs are expensive to copy, hence the use of a const
  // reference.
  const AXTreeID& GetParentTreeID() const;

  // Returns the managed tree's data.
  const AXTreeData& GetTreeData() const;

  // Returns the node identified with the given `node_id).
  AXNode* GetNode(const AXNodeID& node_id) const;

  // Returns the rootnode of the managed tree. The rootnode could be nullptr
  // during some `AXTreeObserver` callbacks.
  AXNode* GetRoot() const;

  // Returns the rootnode of the child tree hosted at `host_node_id`. The
  // rootnode could be nullptr during some `AXTreeObserver` callbacks.
  AXNode* GetRootOfChildTree(const AXNodeID& host_node_id) const;
  AXNode* GetRootOfChildTree(const AXNode& host_node) const;

  // If this tree has a parent tree, returns the node in the parent tree that
  // is hosting the current tree.
  AXNode* GetHostNode() const;

  // Attaches the given child tree to the given host node. Returns a boolean
  // indicating success or failure.
  bool AttachChildTree(const AXNodeID& host_node_id,
                       AXTreeManagerBase& child_manager);
  bool AttachChildTree(AXNode& host_node, AXTreeManagerBase& child_manager);

  // Creates a child tree based on `initial_state` and attaches it to the given
  // host node. Returns the child tree's manager if successful.
  std::optional<AXTreeManagerBase> AttachChildTree(
      const AXNodeID& host_node_id,
      const AXTreeUpdate& initial_state);
  std::optional<AXTreeManagerBase> AttachChildTree(
      AXNode& host_node,
      const AXTreeUpdate& initial_state);

  // Detaches the child tree hosted by the given host node, returning a pointer
  // to its manager.
  AXTreeManagerBase* DetachChildTree(const AXNodeID& host_node_id);
  AXTreeManagerBase* DetachChildTree(AXNode& host_node);

 private:
  static std::unordered_map<AXTreeID, AXTreeManagerBase*, AXTreeIDHash>&
  GetTreeManagerMapInstance();

  std::unique_ptr<AXTree> tree_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_MANAGER_BASE_H_
