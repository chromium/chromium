// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_OBSERVER_H_
#define UI_ACCESSIBILITY_AX_TREE_OBSERVER_H_

#include <set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ui {

class AXNode;
class AXTree;
struct AXTreeData;

// Used when you want to be notified when changes happen to an AXTree.
//
// |OnAtomicUpdateFinished| is notified at the end of an atomic update.
// It provides a vector of nodes that were added or changed, for final
// postprocessing.
class AX_EXPORT AXTreeObserver : public base::CheckedObserver {
 public:
  AXTreeObserver();
  ~AXTreeObserver() override;

  // Called before any tree modifications have occurred, notifying that a single
  // node will change its ignored state or its data. Its id and data will be
  // valid, but its links to parents and children are only valid within this
  // callstack. Do not hold a reference to the node or any relative nodes such
  // as ancestors or descendants described by the node or its node data outside
  // of these events.
  virtual void OnIgnoredWillChange(
      AXTree* tree,
      AXNode* node,
      bool is_ignored_new_value,
      bool is_changing_unignored_parents_children) {}
  virtual void OnNodeDataWillChange(AXTree* tree,
                                    const AXNodeData& old_node_data,
                                    const AXNodeData& new_node_data) {}

  // Called after all tree modifications have occurred, notifying that a single
  // node has changed its data. Its id, data, and links to parent and children
  // will all be valid, since the tree is in a stable state after updating.
  virtual void OnNodeDataChanged(AXTree* tree,
                                 const AXNodeData& old_node_data,
                                 const AXNodeData& new_node_data) {}

  // Individual callbacks for every attribute of AXNodeData that can change.
  // Called after all tree mutations have occurred, notifying that a single node
  // changed its data. Its id, data, and links to parent and children will all
  // be valid, since the tree is in a stable state after updating.
  virtual void OnRoleChanged(AXTree* tree,
                             AXNode* node,
                             ax::mojom::Role old_role,
                             ax::mojom::Role new_role) {}
  virtual void OnIgnoredChanged(AXTree* tree,
                                AXNode* node,
                                bool is_ignored_new_value) {}
  virtual void OnStateChanged(AXTree* tree,
                              AXNode* node,
                              ax::mojom::State state,
                              bool new_value) {}
  virtual void OnStringAttributeChanged(AXTree* tree,
                                        AXNode* node,
                                        ax::mojom::StringAttribute attr,
                                        const std::string& old_value,
                                        const std::string& new_value) {}
  virtual void OnIntAttributeChanged(AXTree* tree,
                                     AXNode* node,
                                     ax::mojom::IntAttribute attr,
                                     int32_t old_value,
                                     int32_t new_value) {}
  virtual void OnFloatAttributeChanged(AXTree* tree,
                                       AXNode* node,
                                       ax::mojom::FloatAttribute attr,
                                       float old_value,
                                       float new_value) {}
  virtual void OnBoolAttributeChanged(AXTree* tree,
                                      AXNode* node,
                                      ax::mojom::BoolAttribute attr,
                                      bool new_value) {}
  virtual void OnIntListAttributeChanged(
      AXTree* tree,
      AXNode* node,
      ax::mojom::IntListAttribute attr,
      const std::vector<int32_t>& old_value,
      const std::vector<int32_t>& new_value) {}
  virtual void OnStringListAttributeChanged(
      AXTree* tree,
      AXNode* node,
      ax::mojom::StringListAttribute attr,
      const std::vector<std::string>& old_value,
      const std::vector<std::string>& new_value) {}

  // Called when tree data changes, after all nodes have been updated.
  virtual void OnTreeDataChanged(AXTree* tree,
                                 const AXTreeData& old_data,
                                 const AXTreeData& new_data) {}

  // Called before any tree modifications have occurred, notifying that a single
  // node will be deleted. Its id and data will be valid, but its links to
  // parents and children are only valid within this callstack. Do not hold
  // a reference to node outside of the event.
  virtual void OnNodeWillBeDeleted(AXTree* tree, AXNode* node) {}

  // Same as OnNodeWillBeDeleted, but only called once for an entire subtree.
  virtual void OnSubtreeWillBeDeleted(AXTree* tree, AXNode* node) {}

  // Called just before a node is deleted for reparenting. See
  // |OnNodeWillBeDeleted| for additional information.
  virtual void OnNodeWillBeReparented(AXTree* tree, AXNode* node) {}

  // Called just before a subtree is deleted for reparenting. See
  // |OnSubtreeWillBeDeleted| for additional information.
  virtual void OnSubtreeWillBeReparented(AXTree* tree, AXNode* node) {}

  // Called after all tree mutations have occurred, notifying that a single node
  // has been created. Its id, data, and links to parent and children will all
  // be valid, since the tree is in a stable state after updating.
  virtual void OnNodeCreated(AXTree* tree, AXNode* node) {}

  // Called after all tree mutations have occurred or during tree teardown,
  // notifying that a single node has been deleted from the tree.
  // TODO(crbug.com/366338645): Migrate AXTree observers to use
  // OnNodeWillBeDeleted.
  virtual void OnNodeDeleted(AXTree* tree, AXNodeID node_id) {}

  // Same as |OnNodeCreated|, but called for nodes that have been reparented.
  virtual void OnNodeReparented(AXTree* tree, AXNode* node) {}

  // Called after all tree mutations have occurred, notifying that a single node
  // has updated its data or children. Its id, data, and links to parent and
  // children will all be valid, since the tree is in a stable state after
  // updating.
  virtual void OnNodeChanged(AXTree* tree, AXNode* node) {}

  // Called when a child tree hosted by `host_node` is connected or
  // disconnected.
  virtual void OnChildTreeConnectionChanged(AXNode* host_node) {}

  // Called just before a tree manager is removed from the AXTreeManagerMap.
  //
  // Why is this needed?
  // In some cases, we update the tree id of an AXTree and need to update the
  // map entry that corresponds to that tree. The observers maintained in the
  // observers list of that AXTree might need to be notified of that change to
  // remove themselves from the list, if needed.
  virtual void OnTreeManagerWillBeRemoved(AXTreeID previous_tree_id) {}

  virtual void OnTextDeletionOrInsertion(const AXNode& node,
                                         const AXNodeData& new_data) {}

  enum ChangeType {
    NODE_CREATED,
    SUBTREE_CREATED,
    NODE_CHANGED,
    NODE_REPARENTED,
    SUBTREE_REPARENTED
  };

  struct Change {
    Change(AXNode* node, ChangeType type) {
      this->node = node;
      this->type = type;
    }
    raw_ptr<AXNode> node;
    ChangeType type;
  };

  // Called just before the atomic update is committed. This is the last
  // notification, after all individual nodes and subtree are notified of
  // upcoming operations. Observers will receive a list of nodes to be deleted
  // in `deleting_nodes` and a list of nodes to be reparented in
  // `reparenting_nodes`. Here they have a last chance to clear any pointers
  // they may be holding to AXNodes. After this call nodes may be deleted or
  // reparented at any point, thus making node pointers to dangle. After the
  // update happens, the changes applied will be available in
  // `OnAtomicUpdateFinished()`.
  virtual void OnAtomicUpdateStarting(
      AXTree* tree,
      const std::set<AXNodeID>& deleting_nodes,
      const std::set<AXNodeID>& reparenting_nodes) {}

  // Called at the end of the update operation. Every node that was added
  // or changed will be included in |changes|, along with an enum indicating
  // the type of change - either (1) a node was created, (2) a node was created
  // and it's the root of a new subtree, or (3) a node was changed. Finally,
  // a bool indicates if the root of the tree was changed or not.
  virtual void OnAtomicUpdateFinished(AXTree* tree,
                                      bool root_changed,
                                      const std::vector<Change>& changes) {}
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_OBSERVER_H_
