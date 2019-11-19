// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_H_
#define UI_ACCESSIBILITY_AX_TREE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <unordered_map>

#include "base/observer_list.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

class AXTableInfo;
class AXTree;
class AXTreeObserver;
struct AXTreeUpdateState;
class AXLanguageDetectionManager;

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

  void AddObserver(AXTreeObserver* observer);
  bool HasObserver(AXTreeObserver* observer);
  void RemoveObserver(const AXTreeObserver* observer);

  base::ObserverList<AXTreeObserver>& observers() { return observers_; }

  AXNode* root() const { return root_; }

  const AXTreeData& data() const { return data_; }

  // AXNode::OwnerTree override.
  // Returns the globally unique ID of this accessibility tree.
  AXTreeID GetAXTreeID() const override;

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

  // Get all of the child tree IDs referenced by any node in this tree.
  const std::set<AXTreeID> GetAllChildTreeIds() const;

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

  // Returns the pos_in_set of node. Looks in ordered_set_info_map_ for cached
  // value. Calculates pos_in_set and set_size for node (and all other nodes in
  // the same ordered set) if no value is present in the cache.
  // This function is guaranteed to be only called on nodes that can hold
  // pos_in_set values, minimizing the size of the cache.
  int32_t GetPosInSet(const AXNode& node, const AXNode* ordered_set) override;
  // Returns the set_size of node. Looks in ordered_set_info_map_ for cached
  // value. Calculates pos_inset_set and set_size for node (and all other nodes
  // in the same ordered set) if no value is present in the cache.
  // This function is guaranteed to be only called on nodes that can hold
  // set_size values, minimizing the size of the cache.
  int32_t GetSetSize(const AXNode& node, const AXNode* ordered_set) override;

  Selection GetUnignoredSelection() const override;

  bool GetTreeUpdateInProgressState() const override;
  void SetTreeUpdateInProgressState(bool set_tree_update_value);

  // AXNode::OwnerTree override.
  // Returns true if the tree represents a paginated document
  bool HasPaginationSupport() const override;

  // Language detection manager, entry point to language detection features.
  // TODO(chrishall): Should this be stored by pointer or value?
  //                  When should we initialize this?
  std::unique_ptr<AXLanguageDetectionManager> language_detection_manager;

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
                     AXNode::AXID id,
                     size_t index_in_parent,
                     AXTreeUpdateState* update_state);

  // Accumulates the work that will be required to update the AXTree.
  // This allows us to notify observers of structure changes when the
  // tree is still in a stable and unchanged state.
  bool ComputePendingChanges(const AXTreeUpdate& update,
                             AXTreeUpdateState& update_state);

  // Populates |update_state| with information about actions that will
  // be performed on the tree during the update, such as adding or
  // removing nodes in the tree. Returns true on success.
  // Nothing within this call should modify tree structure or node data.
  bool ComputePendingChangesToNode(const AXNodeData& new_data,
                                   bool is_new_root,
                                   AXTreeUpdateState* update_state);

  // This is called from within Unserialize(), it returns true on success.
  bool UpdateNode(const AXNodeData& src,
                  bool is_new_root,
                  AXTreeUpdateState* update_state);

  // Notify the delegate that the subtree rooted at |node| will be
  // destroyed or reparented.
  void NotifySubtreeWillBeReparentedOrDeleted(
      AXNode* node,
      const AXTreeUpdateState* update_state);

  // Notify the delegate that |node| will be destroyed or reparented.
  void NotifyNodeWillBeReparentedOrDeleted(
      AXNode* node,
      const AXTreeUpdateState* update_state);

  // Notify the delegate that |node| and all of its descendants will be
  // destroyed. This function is called during AXTree teardown.
  void RecursivelyNotifyNodeDeletedForTreeTeardown(AXNode* node);

  // Notify the delegate that the node marked by |node_id| has been deleted.
  // We are passing the node id instead of ax node is because by the time this
  // function is called, the ax node in the tree will already have been
  // destroyed.
  void NotifyNodeHasBeenDeleted(AXNode::AXID node_id);

  // Notify the delegate that |node| has been created or reparented.
  void NotifyNodeHasBeenReparentedOrCreated(
      AXNode* node,
      const AXTreeUpdateState* update_state);

  // Notify the delegate that a node will change its data.
  void NotifyNodeDataWillChange(const AXNodeData& old_data,
                                const AXNodeData& new_data);

  // Notify the delegate that |node| has changed its data.
  void NotifyNodeDataHasBeenChanged(AXNode* node,
                                    const AXNodeData& old_data,
                                    const AXNodeData& new_data);

  void UpdateReverseRelations(AXNode* node, const AXNodeData& new_data);

  // Returns true if all pending changes in the |update_state| have been
  // handled. If this returns false, the |error_| message will be populated.
  // It's a fatal error to have pending changes after exhausting
  // the AXTreeUpdate.
  bool ValidatePendingChangesComplete(const AXTreeUpdateState& update_state);

  // Modifies |update_state| so that it knows what subtree and nodes are
  // going to be destroyed for the subtree rooted at |node|.
  void MarkSubtreeForDestruction(AXNode::AXID node_id,
                                 AXTreeUpdateState* update_state);

  // Modifies |update_state| so that it knows what nodes are
  // going to be destroyed for the subtree rooted at |node|.
  void MarkNodesForDestructionRecursive(AXNode::AXID node_id,
                                        AXTreeUpdateState* update_state);

  // Validates that destroying the subtree rooted at |node| has required
  // information in |update_state|, then calls DestroyNodeAndSubtree on it.
  void DestroySubtree(AXNode* node, AXTreeUpdateState* update_state);

  // Call Destroy() on |node|, and delete it from the id map, and then
  // call recursively on all nodes in its subtree.
  void DestroyNodeAndSubtree(AXNode* node, AXTreeUpdateState* update_state);

  // Iterate over the children of |node| and for each child, destroy the
  // child and its subtree if its id is not in |new_child_ids|.
  void DeleteOldChildren(AXNode* node,
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

  // Internal implementation of RelativeToTreeBounds. It calls itself
  // recursively but ensures that it can only do so exactly once!
  gfx::RectF RelativeToTreeBoundsInternal(const AXNode* node,
                                          gfx::RectF node_bounds,
                                          bool* offscreen,
                                          bool clip_bounds,
                                          bool allow_recursion) const;

  base::ObserverList<AXTreeObserver> observers_;
  AXNode* root_ = nullptr;
  std::unordered_map<int32_t, AXNode*> id_map_;
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
  mutable std::unordered_map<int32_t, AXTableInfo*> table_info_map_;

  // The next negative node ID to use for internal nodes.
  int32_t next_negative_internal_node_id_ = -1;

  // Whether we should create extra nodes that
  // are only useful on macOS. Implemented using this flag to allow
  // this code to be unit-tested on other platforms (for example, more
  // code sanitizers run on Linux).
  bool enable_extra_mac_nodes_ = false;

  // Contains pos_in_set and set_size data for an AXNode.
  struct OrderedSetInfo {
    int32_t pos_in_set;
    int32_t set_size;
    int32_t lowest_hierarchical_level;
    OrderedSetInfo() : pos_in_set(0), set_size(0) {}
    ~OrderedSetInfo() {}
  };

  // Populates items vector with all items within ordered_set.
  // Will only add items whose roles match the role of the
  // ordered_set.
  void PopulateOrderedSetItems(const AXNode* ordered_set,
                               const AXNode* local_parent,
                               std::vector<const AXNode*>& items,
                               const AXNode& original_node) const;

  // Helper for GetPosInSet and GetSetSize. Computes the pos_in_set and set_size
  // values of all items in ordered_set and caches those values.
  void ComputeSetSizePosInSetAndCache(const AXNode& node,
                                      const AXNode* ordered_set);

  // Map from node ID to OrderedSetInfo.
  // Item-like and ordered-set-like objects will map to populated OrderedSetInfo
  // objects.
  // All other objects will map to default-constructed OrderedSetInfo objects.
  // Invalidated every time the tree is updated.
  mutable std::unordered_map<int32_t, OrderedSetInfo> ordered_set_info_map_;

  // AXTree owns pointers so copying is non-trivial.
  DISALLOW_COPY_AND_ASSIGN(AXTree);

  // Indicates if the tree is updating.
  bool tree_update_in_progress_ = false;

  // Indicates if the tree represents a paginated document
  bool has_pagination_support_ = false;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_H_
