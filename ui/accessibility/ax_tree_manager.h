// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_MANAGER_H_
#define UI_ACCESSIBILITY_AX_TREE_MANAGER_H_

#include "base/scoped_observation.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_observer.h"

namespace ui {

class AXNode;
class AXTreeManagerMap;

// Abstract interface for a class that owns an AXTree and manages its
// connections to other AXTrees in the same page or desktop (parent and child
// trees).
class AX_EXPORT AXTreeManager : public AXTreeObserver {
 public:
  static AXTreeManager* FromID(AXTreeID ax_tree_id);
  // If the child of `parent_node` exists in a separate child tree, return the
  // tree manager for that child tree. Otherwise, return nullptr.
  static AXTreeManager* ForChildTree(const AXNode& parent_node);

  AXTreeManager(const AXTreeManager&) = delete;
  AXTreeManager& operator=(const AXTreeManager&) = delete;

  ~AXTreeManager() override;

  // Returns the AXNode with the given |node_id| from the tree that has the
  // given |tree_id|. This allows for callers to access nodes outside of their
  // own tree. Returns nullptr if |tree_id| or |node_id| is not found.
  // TODO(kschmi): Remove |tree_id| parameter, as it's unnecessary.
  virtual AXNode* GetNodeFromTree(const AXTreeID tree_id,
                                  const AXNodeID node_id) const = 0;

  // Returns the AXNode in the current tree that has the given |node_id|.
  // Returns nullptr if |node_id| is not found.
  virtual AXNode* GetNodeFromTree(const AXNodeID node_id) const = 0;

  // Returns the tree id of the tree managed by this AXTreeManager.
  AXTreeID GetTreeID() const;

  // Returns the tree id of the parent tree.
  // Returns AXTreeIDUnknown if this tree doesn't have a parent tree.
  virtual AXTreeID GetParentTreeID() const;

  // Returns the AXNode that is at the root of the current tree.
  AXNode* GetRootAsAXNode() const;

  // If this tree has a parent tree, returns the node in the parent tree that
  // hosts the current tree. Returns nullptr if this tree doesn't have a parent
  // tree.
  virtual AXNode* GetParentNodeFromParentTreeAsAXNode() const = 0;

  // Called when the tree manager is about to be removed from the tree map,
  // `AXTreeManagerMap`.
  void WillBeRemovedFromMap();

  const AXTreeID& ax_tree_id() const { return ax_tree_id_; }
  AXTree* ax_tree() const { return ax_tree_.get(); }

  const AXEventGenerator& event_generator() const { return event_generator_; }
  AXEventGenerator& event_generator() { return event_generator_; }

  // AXTreeObserver implementation.
  void OnTreeDataChanged(ui::AXTree* tree,
                         const ui::AXTreeData& old_data,
                         const ui::AXTreeData& new_data) override;
  void OnNodeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override {}
  void OnSubtreeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override {}
  void OnNodeCreated(ui::AXTree* tree, ui::AXNode* node) override {}
  void OnNodeDeleted(ui::AXTree* tree, int32_t node_id) override {}
  void OnNodeReparented(ui::AXTree* tree, ui::AXNode* node) override {}
  void OnRoleChanged(ui::AXTree* tree,
                     ui::AXNode* node,
                     ax::mojom::Role old_role,
                     ax::mojom::Role new_role) override {}
  void OnAtomicUpdateFinished(
      ui::AXTree* tree,
      bool root_changed,
      const std::vector<ui::AXTreeObserver::Change>& changes) override {}

 protected:
  AXTreeManager();
  explicit AXTreeManager(std::unique_ptr<AXTree> tree);
  explicit AXTreeManager(const AXTreeID& tree_id, std::unique_ptr<AXTree> tree);

  // TODO(benjamin.beaudry): Remove this helper once we move the logic related
  // to the parent connection from `BrowserAccessibilityManager` to this class.
  // `BrowserAccessibilityManager` needs to remove the manager from the map
  // before calling `BrowserAccessibilityManager::ParentConnectionChanged`, so
  // the default removal of the manager in `~AXTreeManager` occurs too late.
  void RemoveFromMap();

  AXTreeID ax_tree_id_;
  std::unique_ptr<AXTree> ax_tree_;

  AXEventGenerator event_generator_;

 private:
  friend class TestAXTreeManager;

  static AXTreeManagerMap& GetMap();

  // Automatically stops observing notifications from the AXTree when this class
  // is destructed.
  //
  // This member needs to be destructed before any observed AXTrees. Since
  // destructors for non-static member fields are called in the reverse order of
  // declaration, do not move this member above other members.
  base::ScopedObservation<AXTree, AXTreeObserver> tree_observation_{this};
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_MANAGER_H_
