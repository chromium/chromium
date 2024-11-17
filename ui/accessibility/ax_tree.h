// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_H_
#define UI_ACCESSIBILITY_AX_TREE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/debug/crash_logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

struct AXEvent;
class AXLanguageDetectionManager;
class AXNode;
struct AXNodeData;
class AXTableInfo;
class AXTreeObserver;
struct AXTreeUpdateState;
class AXSelection;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AXTreeUnserializeError)
enum class AXTreeUnserializeError {
  // Tree has no root.
  kNoRoot = 0,
  // Node will not be in the tree and is not the new root.
  kNotInTree = 1,
  // Node is already pending for creation, cannot be the new root
  kCreationPending = 2,
  // Node has duplicate child.
  kDuplicateChild = 3,
  // Node is already pending for creation, cannot be a new child.
  kCreationPendingForChild = 4,
  // Node is not marked for destruction, would be reparented.
  kReparent = 5,
  // Nodes are left pending by the update.
  kPendingNodes = 6,
  // Changes left pending by the update;
  kPendingChanges = 7,
  // This must always be the last enum. It's okay for its value to
  // increase, but none of the other enum values may change.
  kMaxValue = kPendingChanges
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:AccessibilityTreeUnserializeError)

#define ACCESSIBILITY_TREE_UNSERIALIZE_ERROR_HISTOGRAM(enum_value) \
  base::UmaHistogramEnumeration(                                   \
      "Accessibility.Reliability.Tree.UnserializeError", enum_value)

// AXTree is a live, managed tree of AXNode objects that can receive
// updates from another AXTreeSource via AXTreeUpdates, and it can be
// used as a source for sending updates to another client tree.
// It's designed to be subclassed to implement support for native
// accessibility APIs on a specific platform.
class AX_EXPORT AXTree {
 public:
  using IntReverseRelationMap =
      std::map<ax::mojom::IntAttribute, std::map<AXNodeID, std::set<AXNodeID>>>;
  using IntListReverseRelationMap =
      std::map<ax::mojom::IntListAttribute,
               std::map<AXNodeID, std::set<AXNodeID>>>;

  // If called, the focused node in this tree will never be ignored, even if it
  // has the ignored state set. For now, this boolean will be set to false for
  // all trees except in test scenarios, in order to thoroughly test the
  // relevant code without causing any potential regressions. Ultimately, we
  // want to expose all focused nodes so that a user of an assistive technology
  // will be able to interact with the application / website, even if there is
  // an authoring error, e.g. the aria-hidden attribute has been applied to the
  // focused element.
  // TODO(nektar): Removed once the feature has been fully tested.
  static void SetFocusedNodeShouldNeverBeIgnored();

  // Determines the ignored state of a node, given information about the node
  // and the tree.
  static bool ComputeNodeIsIgnored(const AXTreeData* optional_tree_data,
                                   const AXNodeData& node_data);

  // Determines whether a node has flipped its ignored state, given information
  // about the previous and current state of the node / tree.
  static bool ComputeNodeIsIgnoredChanged(
      const AXTreeData* optional_old_tree_data,
      const AXNodeData& old_node_data,
      const AXTreeData* optional_new_tree_data,
      const AXNodeData& new_node_data);

  AXTree();
  explicit AXTree(const AXTreeUpdate& initial_state);
  virtual ~AXTree();

  // AXTree owns pointers so copying is non-trivial.
  AXTree(const AXTree&) = delete;
  AXTree& operator=(const AXTree&) = delete;

  void AddObserver(AXTreeObserver* observer);
  bool HasObserver(AXTreeObserver* observer);
  void RemoveObserver(AXTreeObserver* observer);

  base::ObserverList<AXTreeObserver>& observers() { return observers_; }

  AXNode* root() const { return root_; }

  const AXTreeData& data() const;

  // Destroys the tree and notifies all observers.
  void Destroy();

  // Returns the globally unique ID of this accessibility tree.
  const AXTreeID& GetAXTreeID() const;

  // Given a node in this accessibility tree that corresponds to a table
  // or grid, return an object containing information about the
  // table structure. This object is computed lazily on-demand and
  // cached until the next time the tree is updated. Clients should
  // not retain this pointer, they should just request it every time
  // it's needed.
  //
  // Returns nullptr if the node is not a valid table.
  AXTableInfo* GetTableInfo(const AXNode* table_node) const;

  // Returns the AXNode with the given |id| if it is part of this AXTree.
  AXNode* GetFromId(AXNodeID id) const;

  // Returns true on success. If it returns false, it's a fatal error
  // and this tree should be destroyed, and the source of the tree update
  // should not be trusted any longer.
  virtual bool Unserialize(const AXTreeUpdate& update);

  // Used by tests to update the tree data without changing any of the nodes in
  // the tree, notifying all tree observers in the process.
  virtual void UpdateDataForTesting(const AXTreeData& data);

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
                                  bool clip_bounds = true,
                                  bool skip_container_offset = false) const;

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
  std::set<AXNodeID> GetReverseRelations(ax::mojom::IntAttribute attr,
                                         AXNodeID dst_id) const;

  // Given a node ID list attribute (one where
  // IsNodeIdIntListAttribute is true), and a destination node ID,
  // return a set of all source node IDs that have that relationship
  // attribute between them and the destination.
  std::set<AXNodeID> GetReverseRelations(ax::mojom::IntListAttribute attr,
                                         AXNodeID dst_id) const;

  // Given a child tree ID, return the node IDs of all nodes in the tree who
  // have a kChildTreeId int attribute with that value.
  //
  // TODO(accessibility): There should really be only one host node per child
  // tree, so the return value should not be a set but a single node ID or
  // `kInvalidAXNodeID`.
  std::set<AXNodeID> GetNodeIdsForChildTreeId(AXTreeID child_tree_id) const;

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
  std::string ToString(bool verbose = true) const;

  // A string describing the error from an unsuccessful Unserialize,
  // for testing and debugging.
  const std::string& error() const { return error_; }

  int size() { return static_cast<int>(id_map_.size()); }

  // Return a negative number that's suitable to use for a node ID for
  // internal nodes created automatically by an AXTree, so as not to
  // conflict with positive-numbered node IDs from tree sources.
  AXNodeID GetNextNegativeInternalNodeId();

  // Returns the PosInSet of |node|. Looks in node_set_size_pos_in_set_info_map_
  // for cached value. Calls |ComputeSetSizePosInSetAndCache|if no value is
  // present in the cache.
  std::optional<int> GetPosInSet(const AXNode& node);

  // Returns the SetSize of |node|. Looks in node_set_size_pos_in_set_info_map_
  // for cached value. Calls |ComputeSetSizePosInSetAndCache|if no value is
  // present in the cache.
  std::optional<int> GetSetSize(const AXNode& node);

  // Returns the part of the current selection that falls within this
  // accessibility tree, if any.
  AXSelection GetSelection() const;

  // Returns the part of the current selection that falls within this
  // accessibility tree, if any, adjusting its endpoints to be within unignored
  // nodes. (An "ignored" node is a node that is not exposed to platform APIs:
  // See `AXNode::IsIgnored`.)
  AXSelection GetUnignoredSelection() const;

  bool GetTreeUpdateInProgressState() const;

  // Returns true if the tree represents a paginated document
  bool HasPaginationSupport() const;

  // Language detection manager, entry point to language detection features.
  // TODO(chrishall): Should this be stored by pointer or value?
  //                  When should we initialize this?
  std::unique_ptr<AXLanguageDetectionManager> language_detection_manager;

  // Event metadata while applying a tree update during unserialization.
  AXEvent* event_data() const { return event_data_.get(); }

  // Notify the delegate that the tree manager for |previous_tree_id| will be
  // removed from the AXTreeManagerMap. Because we sometimes remove the tree
  // manager after the tree's id has been modified, we need to pass the (old)
  // tree id associated with the manager we are removing even though it is the
  // same tree.
  void NotifyTreeManagerWillBeRemoved(AXTreeID previous_tree_id);

  void NotifyChildTreeConnectionChanged(AXNode* node, AXTree* child_tree);

 private:
  friend class ScopedTreeUpdateInProgressStateSetter;
  friend class AXTableInfoTest;

  // Indicates if the node with the focus should never be ignored, (see
  // `SetFocusedNodeShouldNeverBeIgnored` above).
  static bool is_focused_node_always_unignored_;

#if DCHECK_IS_ON()
  void CheckTreeConsistency(const AXTreeUpdate& update);
#endif

  // Accumulate errors as there can be more than one before Chrome is crashed
  // via UnrecoverableAccessibilityError();
  // In an AX_FAIL_FAST_BUILD or if |is_fatal|, will assert/crash immediately.
  void RecordError(const AXTreeUpdateState& update_state,
                   std::string new_error,
                   bool is_fatal = false);

  AXNode* CreateNode(AXNode* parent,
                     AXNodeID id,
                     size_t index_in_parent,
                     AXTreeUpdateState* update_state);

  // Accumulates the work that will be required to update the AXTree.
  // This allows us to notify observers of structure changes when the
  // tree is still in a stable and unchanged state.
  bool ComputePendingChanges(const AXTreeUpdate& update,
                             AXTreeUpdateState* update_state);

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
      AXNode& node,
      const AXTreeUpdateState& update_state);

  // Notify the delegate that |node| and all of its descendants will be
  // destroyed. This function is called during AXTree teardown.
  void RecursivelyNotifyNodeWillBeDeletedForTreeTeardown(
      AXNode& node,
      std::set<AXNodeID>& deleted_nodes);

  // Notify the delegate that the node marked by |node_id| has been deleted.
  // We are passing the node id instead of ax node is because by the time this
  // function is called, the ax node in the tree will already have been
  // destroyed.
  void NotifyNodeHasBeenDeleted(AXNodeID node_id);

  // Notify the delegate that |node| has been created or reparented.
  void NotifyNodeHasBeenReparentedOrCreated(
      AXNode* node,
      const AXTreeUpdateState* update_state);

  // Notify the delegate that `node` will change its data attributes, including
  // its ignored state.
  void NotifyNodeAttributesWillChange(AXNode* node,
                                      AXTreeUpdateState& update_state,
                                      const AXTreeData* optional_old_tree_data,
                                      const AXNodeData& old_data,
                                      const AXTreeData* new_tree_data,
                                      const AXNodeData& new_data);

  // Notify the delegate that `node` will change its its ignored state.
  void NotifyNodeIgnoredStateWillChange(
      AXNode* node,
      const AXTreeData* optional_old_tree_data,
      const AXNodeData& old_data,
      const AXTreeData* new_tree_data,
      const AXNodeData& new_data);

  // Notify the delegate that `node` has changed its data attributes, including
  // its ignored state.
  void NotifyNodeAttributesHaveBeenChanged(
      AXNode* node,
      AXTreeUpdateState& update_state,
      const AXTreeData* optional_old_tree_data,
      const AXNodeData& old_data,
      const AXTreeData* new_tree_data,
      const AXNodeData& new_data);

  // Update maps that track which relations are pointing to |node|.
  void UpdateReverseRelations(AXNode* node,
                              const AXNodeData& new_data,
                              bool is_new_node = false);

  // Sets a flag indicating whether the tree is currently being updated or not.
  // If the tree is being updated, then its internal pointers might be invalid
  // and the tree should not be traversed.
  void SetTreeUpdateInProgressState(bool set_tree_update_value);

  // Returns true if all pending changes in the |update_state| have been
  // handled. If this returns false, the |error_| message will be populated.
  // It's a fatal error to have pending changes after exhausting
  // the AXTreeUpdate.
  bool ValidatePendingChangesComplete(const AXTreeUpdateState& update_state);

  // Modifies |update_state| so that it knows what subtree and nodes are
  // going to be destroyed for the subtree rooted at |node|.
  void MarkSubtreeForDestruction(AXNodeID node_id,
                                 AXTreeUpdateState* update_state);

  // Modifies |update_state| so that it knows what nodes are
  // going to be destroyed for the subtree rooted at |node|.
  void MarkNodesForDestructionRecursive(AXNodeID node_id,
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
                         const std::vector<AXNodeID>& new_child_ids,
                         AXTreeUpdateState* update_state);

  // Iterate over |new_child_ids| and populate |new_children| with
  // pointers to child nodes, reusing existing nodes already in the tree
  // if they exist, and creating otherwise. Reparenting is disallowed, so
  // if the id already exists as the child of another node, that's an
  // error. Returns true on success, false on fatal error.
  bool CreateNewChildVector(
      AXNode* node,
      const std::vector<AXNodeID>& new_child_ids,
      std::vector<raw_ptr<AXNode, VectorExperimental>>* new_children,
      AXTreeUpdateState* update_state);

  // Returns the lowest unignored ancestor of the node with the given ID. If the
  // node is not ignored, it returns the node.
  AXNode* GetUnignoredAncestorFromId(AXNodeID node_id) const;

  // Internal implementation of RelativeToTreeBounds. It calls itself
  // recursively but ensures that it can only do so exactly once!
  gfx::RectF RelativeToTreeBoundsInternal(const AXNode* node,
                                          gfx::RectF node_bounds,
                                          bool* offscreen,
                                          bool clip_bounds,
                                          bool skip_container_offset,
                                          bool allow_recursion) const;

  base::ObserverList<AXTreeObserver> observers_;
  raw_ptr<AXNode> root_ = nullptr;
  std::unordered_map<AXNodeID, std::unique_ptr<AXNode>> id_map_;
  std::string error_;
  AXTreeData data_;

  // Map from an int attribute (if IsNodeIdIntAttribute is true) to
  // a reverse mapping from target nodes to source nodes.
  IntReverseRelationMap int_reverse_relations_;
  // Map from an int list attribute (if IsNodeIdIntListAttribute is true) to
  // a reverse mapping from target nodes to source nodes.
  IntListReverseRelationMap intlist_reverse_relations_;
  // Map from child tree ID to the set of node IDs that contain that attribute.
  std::map<AXTreeID, std::set<AXNodeID>> child_tree_id_reverse_map_;

  // Map from node ID to cached table info, if the given node is a table.
  // Invalidated every time the tree is updated.
  mutable std::unordered_map<AXNodeID, std::unique_ptr<AXTableInfo>>
      table_info_map_;

  // The next negative node ID to use for internal nodes.
  AXNodeID next_negative_internal_node_id_ = -1;

  // Contains pos_in_set and set_size data for an AXNode.
  struct NodeSetSizePosInSetInfo {
    NodeSetSizePosInSetInfo();
    ~NodeSetSizePosInSetInfo();

    std::optional<int> pos_in_set;
    std::optional<int> set_size;
    std::optional<int> lowest_hierarchical_level;
  };

  // Represents the content of an ordered set which includes the ordered set
  // items and the ordered set container if it exists.
  struct OrderedSetContent;

  // Maps a particular hierarchical level to a list of OrderedSetContents.
  // Represents all ordered set items/container on a particular hierarchical
  // level.
  struct OrderedSetItemsMap;

  // Populates |items_map_to_be_populated| with all items associated with
  // |original_node| and within |ordered_set|. Only items whose roles match the
  // role of the |ordered_set| will be added.
  void PopulateOrderedSetItemsMap(
      const AXNode& original_node,
      const AXNode* ordered_set,
      OrderedSetItemsMap* items_map_to_be_populated) const;

  // Helper function for recursively populating ordered sets items map with
  // all items associated with |original_node| and |ordered_set|. |local_parent|
  // tracks the recursively passed in child nodes of |ordered_set|.
  void RecursivelyPopulateOrderedSetItemsMap(
      const AXNode& original_node,
      const AXNode* ordered_set,
      const AXNode* local_parent,
      std::optional<int> ordered_set_min_level,
      std::optional<int> prev_level,
      OrderedSetItemsMap* items_map_to_be_populated) const;

  // Computes the pos_in_set and set_size values of all items in ordered_set and
  // caches those values. Called by GetPosInSet and GetSetSize.
  void ComputeSetSizePosInSetAndCache(const AXNode& node,
                                      const AXNode* ordered_set);

  // Helper for ComputeSetSizePosInSetAndCache. Computes and caches the
  // pos_in_set and set_size values for a given OrderedSetContent.
  void ComputeSetSizePosInSetAndCacheHelper(
      const OrderedSetContent& ordered_set_content);

  // Map from node ID to OrderedSetInfo.
  // Item-like and ordered-set-like objects will map to populated OrderedSetInfo
  // objects.
  // All other objects will map to default-constructed OrderedSetInfo objects.
  // Invalidated every time the tree is updated.
  mutable std::unordered_map<AXNodeID, NodeSetSizePosInSetInfo>
      node_set_size_pos_in_set_info_map_;

  // Indicates if the tree is updating.
  bool tree_update_in_progress_ = false;

  // Indicates if the tree represents a paginated document
  bool has_pagination_support_ = false;

  std::unique_ptr<AXEvent> event_data_;
};

// Sets the flag that indicates whether the accessibility tree is currently
// being updated, and ensures that it is reset to its previous value when the
// instance is destructed. An accessibility tree that is being updated is
// unstable and should not be traversed.
class AX_EXPORT ScopedTreeUpdateInProgressStateSetter {
 public:
  explicit ScopedTreeUpdateInProgressStateSetter(AXTree& tree)
      : tree_(&tree),
        last_tree_update_in_progress_(tree.GetTreeUpdateInProgressState()) {
    tree_->SetTreeUpdateInProgressState(true);
  }

  ~ScopedTreeUpdateInProgressStateSetter() {
    tree_->SetTreeUpdateInProgressState(last_tree_update_in_progress_);
  }

  ScopedTreeUpdateInProgressStateSetter(
      const ScopedTreeUpdateInProgressStateSetter&) = delete;
  ScopedTreeUpdateInProgressStateSetter& operator=(
      const ScopedTreeUpdateInProgressStateSetter&) = delete;

 private:
  const raw_ptr<AXTree> tree_;
  bool last_tree_update_in_progress_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_H_
