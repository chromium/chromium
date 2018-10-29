// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_H_
#define UI_ACCESSIBILITY_AX_TREE_H_

#include <stdint.h>

#include <set>

#include "base/containers/hash_tables.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

class AXTableInfo;
class AXTree;
struct AXTreeUpdateState;

// Used when you want to be notified when changes happen to the tree.
//
// Some of the notifications are called in the middle of an update operation.
// Be careful, as the tree may be in an inconsistent state at this time;
// don't walk the parents and children at this time:
//   OnNodeWillBeDeleted
//   OnSubtreeWillBeDeleted
//   OnNodeWillBeReparented
//   OnSubtreeWillBeReparented
//   OnNodeCreated
//   OnNodeReparented
//   OnNodeChanged
//
// In addition, one additional notification is fired at the end of an
// atomic update, and it provides a vector of nodes that were added or
// changed, for final postprocessing:
//   OnAtomicUpdateFinished
//
class AX_EXPORT AXTreeDelegate {
 public:
  AXTreeDelegate();
  virtual ~AXTreeDelegate();

  // Called before a node's data gets updated.
  virtual void OnNodeDataWillChange(AXTree* tree,
                                    const AXNodeData& old_node_data,
                                    const AXNodeData& new_node_data) = 0;

  // Individual callbacks for every attribute of AXNodeData that can change.
  virtual void OnRoleChanged(AXTree* tree,
                             AXNode* node,
                             ax::mojom::Role old_role,
                             ax::mojom::Role new_role) {}
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

  // Called when tree data changes.
  virtual void OnTreeDataChanged(AXTree* tree,
                                 const ui::AXTreeData& old_data,
                                 const ui::AXTreeData& new_data) = 0;
  // Called just before a node is deleted. Its id and data will be valid,
  // but its links to parents and children are invalid. This is called
  // in the middle of an update, the tree may be in an invalid state!
  virtual void OnNodeWillBeDeleted(AXTree* tree, AXNode* node) = 0;

  // Same as OnNodeWillBeDeleted, but only called once for an entire subtree.
  // This is called in the middle of an update, the tree may be in an
  // invalid state!
  virtual void OnSubtreeWillBeDeleted(AXTree* tree, AXNode* node) = 0;

  // Called just before a node is deleted for reparenting. See
  // |OnNodeWillBeDeleted| for additional information.
  virtual void OnNodeWillBeReparented(AXTree* tree, AXNode* node) = 0;

  // Called just before a subtree is deleted for reparenting. See
  // |OnSubtreeWillBeDeleted| for additional information.
  virtual void OnSubtreeWillBeReparented(AXTree* tree, AXNode* node) = 0;

  // Called immediately after a new node is created. The tree may be in
  // the middle of an update, don't walk the parents and children now.
  virtual void OnNodeCreated(AXTree* tree, AXNode* node) = 0;

  // Called immediately after a node is reparented. The tree may be in the
  // middle of an update, don't walk the parents and children now.
  virtual void OnNodeReparented(AXTree* tree, AXNode* node) = 0;

  // Called when a node changes its data or children. The tree may be in
  // the middle of an update, don't walk the parents and children now.
  virtual void OnNodeChanged(AXTree* tree, AXNode* node) = 0;

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
    AXNode* node;
    ChangeType type;
  };

  // Called at the end of the update operation. Every node that was added
  // or changed will be included in |changes|, along with an enum indicating
  // the type of change - either (1) a node was created, (2) a node was created
  // and it's the root of a new subtree, or (3) a node was changed. Finally,
  // a bool indicates if the root of the tree was changed or not.
  virtual void OnAtomicUpdateFinished(AXTree* tree,
                                      bool root_changed,
                                      const std::vector<Change>& changes) = 0;
};

// AXTree is a live, managed tree of AXNode objects that can receive
// updates from another AXTreeSource via AXTreeUpdates, and it can be
// used as a source for sending updates to another client tree.
// It's designed to be subclassed to implement support for native
// accessibility APIs on a specific platform.
class AX_EXPORT AXTree : public AXNode::OwnerTree {
 public:
  typedef std::map<ax::mojom::IntAttribute,
                   std::map<int32_t, std::set<int32_t>>>
      IntReverseRelationMap;
  typedef std::map<ax::mojom::IntListAttribute,
                   std::map<int32_t, std::set<int32_t>>>
      IntListReverseRelationMap;

  AXTree();
  explicit AXTree(const AXTreeUpdate& initial_state);
  virtual ~AXTree();

  virtual void SetDelegate(AXTreeDelegate* delegate);
  AXTreeDelegate* delegate() const { return delegate_; }

  AXNode* root() const { return root_; }

  const AXTreeData& data() const { return data_; }

  // AXNode::OwnerTree override.
  // Returns the AXNode with the given |id| if it is part of this AXTree.
  AXNode* GetFromId(int32_t id) const override;

  // Returns true on success. If it returns false, it's a fatal error
  // and this tree should be destroyed, and the source of the tree update
  // should not be trusted any longer.
  virtual bool Unserialize(const AXTreeUpdate& update);

  virtual void UpdateData(const AXTreeData& data);

  // Convert any rectangle from the local coordinate space of one node in
  // the tree, to bounds in the coordinate space of the tree.
  // If set, updates |offscreen| boolean to be true if the node is offscreen
  // relative to its rootWebArea. Callers should initialize |offscreen|
  // to false: this method may get called multiple times in a row and
  // |offscreen| will be propagated.
  // If |clip_bounds| is true, result bounds will be clipped.
  gfx::RectF RelativeToTreeBounds(const AXNode* node,
                                  gfx::RectF node_bounds,
                                  bool* offscreen = nullptr,
                                  bool clip_bounds = true) const;

  // Get the bounds of a node in the coordinate space of the tree.
  // If set, updates |offscreen| boolean to be true if the node is offscreen
  // relative to its rootWebArea. Callers should initialize |offscreen|
  // to false: this method may get called multiple times in a row and
  // |offscreen| will be propagated.
  // If |clip_bounds| is true, result bounds will be clipped.
  gfx::RectF GetTreeBounds(const AXNode* node,
                           bool* offscreen = nullptr,
                           bool clip_bounds = true) const;

  // Given a node ID attribute (one where IsNodeIdIntAttribute is true),
  // and a destination node ID, return a set of all source node IDs that
  // have that relationship attribute between them and the destination.
  std::set<int32_t> GetReverseRelations(ax::mojom::IntAttribute attr,
                                        int32_t dst_id) const;

  // Given a node ID list attribute (one where
  // IsNodeIdIntListAttribute is true), and a destination node ID,
  // return a set of all source node IDs that have that relationship
  // attribute between them and the destination.
  std::set<int32_t> GetReverseRelations(ax::mojom::IntListAttribute attr,
                                        int32_t dst_id) const;

  // Given a child tree ID, return the node IDs of all nodes in the tree who
  // have a kChildTreeId int attribute with that value.
  std::set<int32_t> GetNodeIdsForChildTreeId(AXTreeID child_tree_id) const;

  // Map from a relation attribute to a map from a target id to source ids.
  const IntReverseRelationMap& int_reverse_relations() {
    return int_reverse_relations_;
  }
  const IntListReverseRelationMap& intlist_reverse_relations() {
    return intlist_reverse_relations_;
  }

  // Return a multi-line indented string representation, for logging.
  std::string ToString() const;

  // A string describing the error from an unsuccessful Unserialize,
  // for testing and debugging.
  const std::string& error() const { return error_; }

  int size() { return static_cast<int>(id_map_.size()); }

  // Call this to enable support for extra Mac nodes - for each table,
  // a table column header and a node for each column.
  void SetEnableExtraMacNodes(bool enabled);
  bool enable_extra_mac_nodes() const { return enable_extra_mac_nodes_; }

  // Return a negative number that's suitable to use for a node ID for
  // internal nodes created automatically by an AXTree, so as not to
  // conflict with positive-numbered node IDs from tree sources.
  int32_t GetNextNegativeInternalNodeId();

 private:
  friend class AXTableInfoTest;

  // AXNode::OwnerTree override.
  //
  // Given a node in this accessibility tree that corresponds to a table
  // or grid, return an object containing information about the
  // table structure. This object is computed lazily on-demand and
  // cached until the next time the tree is updated. Clients should
  // not retain this pointer, they should just request it every time
  // it's needed.
  //
  // Returns nullptr if the node is not a valid table.
  AXTableInfo* GetTableInfo(const AXNode* table_node) const override;

  AXNode* CreateNode(AXNode* parent,
                     int32_t id,
                     int32_t index_in_parent,
                     AXTreeUpdateState* update_state);

  // This is called from within Unserialize(), it returns true on success.
  bool UpdateNode(const AXNodeData& src,
                  bool is_new_root,
                  AXTreeUpdateState* update_state);

  void CallNodeChangeCallbacks(AXNode* node, const AXNodeData& new_data);

  void UpdateReverseRelations(AXNode* node, const AXNodeData& new_data);

  void OnRootChanged();

  // Notify the delegate that the subtree rooted at |node| will be destroyed,
  // then call DestroyNodeAndSubtree on it.
  void DestroySubtree(AXNode* node, AXTreeUpdateState* update_state);

  // Call Destroy() on |node|, and delete it from the id map, and then
  // call recursively on all nodes in its subtree.
  void DestroyNodeAndSubtree(AXNode* node, AXTreeUpdateState* update_state);

  // Iterate over the children of |node| and for each child, destroy the
  // child and its subtree if its id is not in |new_child_ids|. Returns
  // true on success, false on fatal error.
  bool DeleteOldChildren(AXNode* node,
                         const std::vector<int32_t>& new_child_ids,
                         AXTreeUpdateState* update_state);

  // Iterate over |new_child_ids| and populate |new_children| with
  // pointers to child nodes, reusing existing nodes already in the tree
  // if they exist, and creating otherwise. Reparenting is disallowed, so
  // if the id already exists as the child of another node, that's an
  // error. Returns true on success, false on fatal error.
  bool CreateNewChildVector(AXNode* node,
                            const std::vector<int32_t>& new_child_ids,
                            std::vector<AXNode*>* new_children,
                            AXTreeUpdateState* update_state);

  AXTreeDelegate* delegate_ = nullptr;
  AXNode* root_ = nullptr;
  base::hash_map<int32_t, AXNode*> id_map_;
  std::string error_;
  AXTreeData data_;

  // Map from an int attribute (if IsNodeIdIntAttribute is true) to
  // a reverse mapping from target nodes to source nodes.
  IntReverseRelationMap int_reverse_relations_;
  // Map from an int list attribute (if IsNodeIdIntListAttribute is true) to
  // a reverse mapping from target nodes to source nodes.
  IntListReverseRelationMap intlist_reverse_relations_;
  // Map from child tree ID to the set of node IDs that contain that attribute.
  std::map<AXTreeID, std::set<int32_t>> child_tree_id_reverse_map_;

  // Map from node ID to cached table info, if the given node is a table.
  // Invalidated every time the tree is updated.
  mutable base::hash_map<int32_t, AXTableInfo*> table_info_map_;

  // The next negative node ID to use for internal nodes.
  int32_t next_negative_internal_node_id_ = -1;

  // Whether we should create extra nodes that
  // are only useful on macOS. Implemented using this flag to allow
  // this code to be unit-tested on other platforms (for example, more
  // code sanitizers run on Linux).
  bool enable_extra_mac_nodes_ = false;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_H_
