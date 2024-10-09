// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree.h"

#include <stddef.h>

#include <numeric>
#include <tuple>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/timer/elapsed_timer.h"
#include "components/crash/core/common/crash_key.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_language_detection.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/ax_table_info.h"
#include "ui/accessibility/ax_tree_observer.h"
#include "ui/gfx/geometry/transform.h"

namespace ui {

namespace {

// This is the list of reverse relations that are computed.
// This purposely does not include relations such as kRadioGroupIds where
// the reverse relation is not interesting to consumers.
constexpr ax::mojom::IntListAttribute kReverseRelationIntListAttributes[] = {
    ax::mojom::IntListAttribute::kControlsIds,
    ax::mojom::IntListAttribute::kDetailsIds,
    ax::mojom::IntListAttribute::kDescribedbyIds,
    ax::mojom::IntListAttribute::kErrormessageIds,
    ax::mojom::IntListAttribute::kFlowtoIds,
    ax::mojom::IntListAttribute::kLabelledbyIds};
constexpr ax::mojom::IntAttribute kReverseRelationIntAttributes[] = {
    ax::mojom::IntAttribute::kActivedescendantId};

std::string TreeToStringHelper(const AXNode* node, int indent, bool verbose) {
  if (!node)
    return "";

  return std::accumulate(
      node->children().cbegin(), node->children().cend(),
      std::string(2 * indent, ' ') + node->data().ToString(verbose) + "\n",
      [indent, verbose](const std::string& str, const AXNode* child) {
        return str + TreeToStringHelper(child, indent + 1, verbose);
      });
}

template <typename K, typename V>
bool KeyValuePairsKeysMatch(std::vector<std::pair<K, V>> pairs1,
                            std::vector<std::pair<K, V>> pairs2) {
  if (pairs1.size() != pairs2.size())
    return false;
  for (size_t i = 0; i < pairs1.size(); ++i) {
    if (pairs1[i].first != pairs2[i].first)
      return false;
  }
  return true;
}

template <typename K, typename V>
std::map<K, V> MapFromKeyValuePairs(std::vector<std::pair<K, V>> pairs) {
  std::map<K, V> result;
  for (size_t i = 0; i < pairs.size(); ++i)
    result[pairs[i].first] = pairs[i].second;
  return result;
}

// Given two vectors of <K, V> key, value pairs representing an "old" vs "new"
// state, or "before" vs "after", calls a callback function for each key that
// changed value. Note that if an attribute is removed, that will result in
// a call to the callback with the value changing from the previous value to
// |empty_value|, and similarly when an attribute is added.
template <typename K, typename V, typename F>
void CallIfAttributeValuesChanged(const std::vector<std::pair<K, V>>& old_pairs,
                                  const std::vector<std::pair<K, V>>& new_pairs,
                                  const V& empty_value,
                                  F callback) {
  // Fast path - if they both have the same keys in the same order.
  if (KeyValuePairsKeysMatch(old_pairs, new_pairs)) {
    for (size_t i = 0; i < old_pairs.size(); ++i) {
      const auto& old_entry = old_pairs[i];
      const auto& new_entry = new_pairs[i];
      if (old_entry.second != new_entry.second) {
        callback(old_entry.first, old_entry.second, new_entry.second);
      }
    }
    return;
  }

  // Slower path - they don't have the same keys in the same order, so
  // check all keys against each other.
  using VectorOfPairs = std::vector<std::pair<K, V>>&;
  auto comp = [](const auto& lhs, const auto& rhs) {
    return lhs.first < rhs.first;
  };
  std::sort(const_cast<VectorOfPairs>(old_pairs).begin(),
            const_cast<VectorOfPairs>(old_pairs).end(), comp);
  std::sort(const_cast<VectorOfPairs>(new_pairs).begin(),
            const_cast<VectorOfPairs>(new_pairs).end(), comp);
  for (size_t old_i = 0, new_i = 0;
       old_i < old_pairs.size() || new_i < new_pairs.size();) {
    // If we reached the end of one of the vectors.
    if (old_i >= old_pairs.size()) {
      const auto& new_pair = new_pairs[new_i];
      if (new_pair.second != empty_value) {
        callback(new_pair.first, empty_value, new_pair.second);
      }
      new_i++;
      continue;
    } else if (new_i >= new_pairs.size()) {
      const auto& old_pair = old_pairs[old_i];
      if (old_pair.second != empty_value) {
        callback(old_pair.first, old_pair.second, empty_value);
      }
      old_i++;
      continue;
    }

    const auto& old_pair = old_pairs[old_i];
    const auto& new_pair = new_pairs[new_i];
    if (old_pair.first == new_pair.first) {
      if (old_pair.second != new_pair.second) {
        callback(old_pair.first, old_pair.second, new_pair.second);
      }
      old_i++;
      new_i++;
    } else if (old_pair.first < new_pair.first) {
      // This means `new_pairs` has no key for `old_pair.first`.
      if (old_pair.second != empty_value) {
        callback(old_pair.first, old_pair.second, empty_value);
      }
      old_i++;
    } else {
      // This means `old_pairs` has no key for `new_pair.first`.
      if (new_pair.second != empty_value) {
        callback(new_pair.first, empty_value, new_pair.second);
      }
      new_i++;
    }
  }
}

bool IsCollapsed(const AXNode* node) {
  return node && node->HasState(ax::mojom::State::kCollapsed);
}

}  // namespace

// static
bool AXTree::is_focused_node_always_unignored_ = false;

// This object is used to track structure changes that will occur for a specific
// AXID. This includes how many times we expect that a node with a specific AXID
// will be created and/or destroyed, and how many times a subtree rooted at AXID
// expects to be destroyed during an AXTreeUpdate.
//
// An AXTreeUpdate is a serialized representation of an atomic change to an
// AXTree. See also |AXTreeUpdate| which documents the nature and invariants
// required to atomically update the AXTree.
//
// The reason that we must track these counts, and the reason these are counts
// rather than a bool/flag is because an AXTreeUpdate may contain multiple
// AXNodeData updates for a given AXID. A common way that this occurs is when
// multiple AXTreeUpdates are merged together, combining their AXNodeData list.
// Additionally AXIDs may be reused after being removed from the tree,
// most notably when "reparenting" a node. A "reparent" occurs when an AXID is
// first destroyed from the tree then created again in the same AXTreeUpdate,
// which may also occur multiple times with merged updates.
//
// We need to accumulate these counts for 3 reasons :
//   1. To determine what structure changes *will* occur before applying
//      updates to the tree so that we can notify observers of structure changes
//      when the tree is still in a stable and unchanged state.
//   2. Capture any errors *before* applying updates to the tree structure
//      due to the order of (or lack of) AXNodeData entries in the update
//      so we can abort a bad update instead of applying it partway.
//   3. To validate that the expectations we accumulate actually match
//      updates that are applied to the tree.
//
// To reiterate the invariants that this structure is taking a dependency on
// from |AXTreeUpdate|, suppose that the next AXNodeData to be applied is
// |node|. The following invariants must hold:
// 1. Either
//   a) |node.id| is already in the tree, or
//   b) the tree is empty, and
//      |node| is the new root of the tree, and
//      |node.role| == kRootWebArea.
// 2. Every child id in |node.child_ids| must either be already a child
//        of this node, or a new id not previously in the tree. It is not
//        allowed to "reparent" a child to this node without first removing
//        that child from its previous parent.
// 3. When a new id appears in |node.child_ids|, the tree should create a
//        new uninitialized placeholder node for it immediately. That
//        placeholder must be updated within the same AXTreeUpdate, otherwise
//        it's a fatal error. This guarantees the tree is always complete
//        before or after an AXTreeUpdate.
struct PendingStructureChanges {
  explicit PendingStructureChanges(const AXNode* node)
      : destroy_subtree_count(0),
        destroy_node_count(0),
        create_node_count(0),
        node_exists(!!node),
        parent_node_id((node && node->parent())
                           ? std::make_optional<AXNodeID>(node->parent()->id())
                           : std::nullopt),
        last_known_data(node ? &node->data() : nullptr) {}

  // Returns true if this node has any changes remaining.
  // This includes pending subtree or node destruction, and node creation.
  bool DoesNodeExpectAnyStructureChanges() const {
    return DoesNodeExpectSubtreeWillBeDestroyed() ||
           DoesNodeExpectNodeWillBeDestroyed() ||
           DoesNodeExpectNodeWillBeCreated();
  }

  // Returns true if there are any pending changes that require destroying
  // this node or its subtree.
  bool DoesNodeExpectSubtreeOrNodeWillBeDestroyed() const {
    return DoesNodeExpectSubtreeWillBeDestroyed() ||
           DoesNodeExpectNodeWillBeDestroyed();
  }

  // Returns true if the subtree rooted at this node needs to be destroyed
  // during the update, but this may not be the next action that needs to be
  // performed on the node.
  bool DoesNodeExpectSubtreeWillBeDestroyed() const {
    return destroy_subtree_count;
  }

  // Returns true if this node needs to be destroyed during the update, but this
  // may not be the next action that needs to be performed on the node.
  bool DoesNodeExpectNodeWillBeDestroyed() const { return destroy_node_count; }

  // Returns true if this node needs to be created during the update, but this
  // may not be the next action that needs to be performed on the node.
  bool DoesNodeExpectNodeWillBeCreated() const { return create_node_count; }

  // Returns true if this node would exist in the tree as of the last pending
  // update that was processed, and the node has not been provided node data.
  bool DoesNodeRequireInit() const { return node_exists && !last_known_data; }

  // Keep track of the number of times the subtree rooted at this node
  // will be destroyed.
  // An example of when this count may be larger than 1 is if updates were
  // merged together. A subtree may be [created,] destroyed, created, and
  // destroyed again within the same |AXTreeUpdate|. The important takeaway here
  // is that an update may request destruction of a subtree rooted at an
  // AXID more than once, not that a specific subtree is being destroyed
  // more than once.
  int32_t destroy_subtree_count = 0;

  // Keep track of the number of times this node will be destroyed.
  // An example of when this count may be larger than 1 is if updates were
  // merged together. A node may be [created,] destroyed, created, and destroyed
  // again within the same |AXTreeUpdate|. The important takeaway here is that
  // an AXID may request destruction more than once, not that a specific node
  // is being destroyed more than once.
  int32_t destroy_node_count = 0;

  // Keep track of the number of times this node will be created.
  // An example of when this count may be larger than 1 is if updates were
  // merged together. A node may be [destroyed,] created, destroyed, and created
  // again within the same |AXTreeUpdate|. The important takeaway here is that
  // an AXID may request creation more than once, not that a specific node is
  // being created more than once.
  int32_t create_node_count = 0;

  // Keep track of whether this node exists in the tree as of the last pending
  // update that was processed.
  bool node_exists;

  // Keep track of the parent id for this node as of the last pending
  // update that was processed.
  std::optional<AXNodeID> parent_node_id;

  // Keep track of the last known node data for this node.
  // This will be null either when a node does not exist in the tree, or
  // when the node is new and has not been initialized with node data yet.
  // This is needed to determine what children have changed between pending
  // updates.
  raw_ptr<const AXNodeData> last_known_data;
};

// Represents the different states when computing PendingStructureChanges
// required for tree Unserialize.
enum class AXTreePendingStructureStatus {
  // PendingStructureChanges have not begun computation.
  kNotStarted,
  // PendingStructureChanges are currently being computed.
  kComputing,
  // All PendingStructureChanges have successfully been computed.
  kComplete,
  // An error occurred when computing pending changes.
  kFailed,
};

// Intermediate state to keep track of during a tree update.
struct AXTreeUpdateState {
  AXTreeUpdateState(const AXTree& tree, const AXTreeUpdate& pending_tree_update)
      : pending_tree_update(pending_tree_update), tree(tree) {}

  // Returns whether this update creates a node marked by |node_id|.
  bool IsCreatedNode(AXNodeID node_id) const {
    return base::Contains(new_node_ids, node_id);
  }

  // Returns whether this update creates |node|.
  bool IsCreatedNode(const AXNode* node) const {
    return IsCreatedNode(node->id());
  }

  // Returns whether this update reparents |node|.
  bool IsReparentedNode(const AXNode* node) const {
    DCHECK_EQ(AXTreePendingStructureStatus::kComplete, pending_update_status)
        << "This method should not be called before pending changes have "
           "finished computing.";
    return IsReparentedNode(node->id());
  }

  // Returns whether this update reparents the node represented by `node_data`.
  bool IsReparentedNode(const AXNodeID node_id) const {
    return reparenting_node_ids.contains(node_id);
  }

  // Returns true if the node should exist in the tree but doesn't have
  // any node data yet.
  bool DoesPendingNodeRequireInit(AXNodeID node_id) const {
    DCHECK_EQ(AXTreePendingStructureStatus::kComputing, pending_update_status)
        << "This method should only be called while computing pending changes, "
           "before updates are made to the tree.";
    PendingStructureChanges* data = GetPendingStructureChanges(node_id);
    return data && data->DoesNodeRequireInit();
  }

  // Returns the parent node id for the pending node.
  std::optional<AXNodeID> GetParentIdForPendingNode(AXNodeID node_id) {
    DCHECK_EQ(AXTreePendingStructureStatus::kComputing, pending_update_status)
        << "This method should only be called while computing pending changes, "
           "before updates are made to the tree.";
    PendingStructureChanges* data = GetOrCreatePendingStructureChanges(node_id);
    DCHECK(!data->parent_node_id ||
           ShouldPendingNodeExistInTree(*data->parent_node_id));
    return data->parent_node_id;
  }

  // Returns true if this node should exist in the tree.
  bool ShouldPendingNodeExistInTree(AXNodeID node_id) {
    DCHECK_EQ(AXTreePendingStructureStatus::kComputing, pending_update_status)
        << "This method should only be called while computing pending changes, "
           "before updates are made to the tree.";
    return GetOrCreatePendingStructureChanges(node_id)->node_exists;
  }

  // Returns the last known node data for a pending node.
  const AXNodeData& GetLastKnownPendingNodeData(AXNodeID node_id) const {
    DCHECK_EQ(AXTreePendingStructureStatus::kComputing, pending_update_status)
        << "This method should only be called while computing pending changes, "
           "before updates are made to the tree.";
    static base::NoDestructor<AXNodeData> empty_data;
    PendingStructureChanges* data = GetPendingStructureChanges(node_id);
    return (data && data->last_known_data) ? *data->last_known_data
                                           : *empty_data;
  }

  // Clear the last known pending data for |node_id|.
  void ClearLastKnownPendingNodeData(AXNodeID node_id) {
    DCHECK_EQ(AXTreePendingStructureStatus::kComputing, pending_update_status)
        << "This method should only be called while computing pending changes, "
           "before updates are made to the tree.";
    GetOrCreatePendingStructureChanges(node_id)->last_known_data = nullptr;
  }

  // Update the last known pending node data for |node_data.id|.
  void SetLastKnownPendingNodeData(const AXNodeData* node_data) {
    DCHECK_EQ(AXTreePendingStructureStatus::kComputing, pending_update_status)
        << "This method should only be called while computing pending changes, "
           "before updates are made to the tree.";
    GetOrCreatePendingStructureChanges(node_data->id)->last_known_data =
        node_data;
  }

  // Returns the number of times the update is expected to destroy a
  // subtree rooted at |node_id|.
  int32_t GetPendingDestroySubtreeCount(AXNodeID node_id) const {
    DCHECK_EQ(AXTreePendingStructureStatus::kComplete, pending_update_status)
        << "This method should not be called before pending changes have "
           "finished computing.";
    if (PendingStructureChanges* data = GetPendingStructureChanges(node_id))
      return data->destroy_subtree_count;
    return 0;
  }

  // Increments the number of times the update is expected to
  // destroy a subtree rooted at |node_id|.
  // Returns true on success, false on failure when the node will not exist.
  bool IncrementPendingDestroySubtreeCount(AXNodeID node_id) {
    DCHECK_EQ(AXTreePendingStructureStatus::kComputing, pending_update_status)
        << "This method should only be called while computing pending changes, "
           "before updates are made to the tree.";
    PendingStructureChanges* data = GetOrCreatePendingStructureChanges(node_id);
    if (!data->node_exists)
      return false;

    ++data->destroy_subtree_count;
    return true;
  }

  // Decrements the number of times the update is expected to
  // destroy a subtree rooted at |node_id|.
  void DecrementPendingDestroySubtreeCount(AXNodeID node_id) {
    DCHECK_EQ(AXTreePendingStructureStatus::kComplete, pending_update_status)
        << "This method should not be called before pending changes have "
           "finished computing.";
    if (PendingStructureChanges* data = GetPendingStructureChanges(node_id)) {
      DCHECK_GT(data->destroy_subtree_count, 0);
      --data->destroy_subtree_count;
    }
  }

  // Returns the number of times the update is expected to destroy
  // a node with |node_id|.
  int32_t GetPendingDestroyNodeCount(AXNodeID node_id) const {
    DCHECK_EQ(AXTreePendingStructureStatus::kComplete, pending_update_status)
        << "This method should not be called before pending changes have "
           "finished computing.";
    if (PendingStructureChanges* data = GetPendingStructureChanges(node_id))
      return data->destroy_node_count;
    return 0;
  }

  // Increments the number of times the update is expected to
  // destroy a node with |node_id|.
  // Returns true on success, false on failure when the node will not exist.
  bool IncrementPendingDestroyNodeCount(AXNodeID node_id) {
    DCHECK_EQ(AXTreePendingStructureStatus::kComputing, pending_update_status)
        << "This method should only be called while computing pending changes, "
           "before updates are made to the tree.";
    PendingStructureChanges* data = GetOrCreatePendingStructureChanges(node_id);
    if (!data->node_exists)
      return false;

    ++data->destroy_node_count;
    data->node_exists = false;
    data->last_known_data = nullptr;
    data->parent_node_id = std::nullopt;
    if (pending_root_id == node_id)
      pending_root_id = std::nullopt;

    // This node may have been flagged for reparenting previously. It is now
    // deleted (possibly again).
    reparenting_node_ids.erase(node_id);
    deleting_node_ids.insert(node_id);

    return true;
  }

  // Decrements the number of times the update is expected to
  // destroy a node with |node_id|.
  void DecrementPendingDestroyNodeCount(AXNodeID node_id) {
    DCHECK_EQ(AXTreePendingStructureStatus::kComplete, pending_update_status)
        << "This method should not be called before pending changes have "
           "finished computing.";
    if (PendingStructureChanges* data = GetPendingStructureChanges(node_id)) {
      DCHECK_GT(data->destroy_node_count, 0);
      --data->destroy_node_count;
    }
  }

  // Returns the number of times the update is expected to create
  // a node with |node_id|.
  int32_t GetPendingCreateNodeCount(AXNodeID node_id) const {
    DCHECK_EQ(AXTreePendingStructureStatus::kComplete, pending_update_status)
        << "This method should not be called before pending changes have "
           "finished computing.";
    if (PendingStructureChanges* data = GetPendingStructureChanges(node_id))
      return data->create_node_count;
    return 0;
  }

  // Increments the number of times the update is expected to
  // create a node with |node_id|.
  // Returns true on success, false on failure when the node will already exist.
  bool IncrementPendingCreateNodeCount(AXNodeID node_id,
                                       std::optional<AXNodeID> parent_node_id) {
    DCHECK_EQ(AXTreePendingStructureStatus::kComputing, pending_update_status)
        << "This method should only be called while computing pending changes, "
           "before updates are made to the tree.";
    PendingStructureChanges* data = GetOrCreatePendingStructureChanges(node_id);
    if (data->node_exists)
      return false;

    ++data->create_node_count;
    data->node_exists = true;
    data->parent_node_id = parent_node_id;

    if (data->destroy_node_count > 0) {
      // This node was destroyed by a previous update. This means a reparenting.
      reparenting_node_ids.insert(node_id);

      // This also means the node isn't going to be deleted after all.
      deleting_node_ids.erase(node_id);
    }

    return true;
  }

  // Decrements the number of times the update is expected to
  // create a node with |node_id|.
  void DecrementPendingCreateNodeCount(AXNodeID node_id) {
    DCHECK_EQ(AXTreePendingStructureStatus::kComplete, pending_update_status)
        << "This method should not be called before pending changes have "
           "finished computing.";
    if (PendingStructureChanges* data = GetPendingStructureChanges(node_id)) {
      DCHECK_GT(data->create_node_count, 0);
      --data->create_node_count;
    }
  }

  // Returns true if this node's updated data in conjunction with the updated
  // tree data indicate that the node will need to invalidate any of its cached
  // values, such as the number of its unignored children.
  // TODO(accessibility) Can we use ignored_state_changed_ids here?
  bool HasIgnoredChanged(const AXNodeData& new_data) const {
    DCHECK_EQ(AXTreePendingStructureStatus::kComputing, pending_update_status)
        << "This method should only be called while computing pending changes, "
           "before updates are made to the tree.";
    const AXNodeData& old_data = GetLastKnownPendingNodeData(new_data.id);
    return AXTree::ComputeNodeIsIgnored(
               old_tree_data ? &old_tree_data.value() : nullptr, old_data) !=
           AXTree::ComputeNodeIsIgnored(
               new_tree_data ? &new_tree_data.value() : nullptr, new_data);
  }

  // Returns whether this update must invalidate the unignored cached
  // values for |node_id|.
  bool InvalidatesUnignoredCachedValues(AXNodeID node_id) const {
    return base::Contains(invalidate_unignored_cached_values_ids, node_id);
  }

  // Adds the parent of |node_id| to the list of nodes to invalidate unignored
  // cached values.
  void InvalidateParentNodeUnignoredCacheValues(AXNodeID node_id) {
    DCHECK_EQ(AXTreePendingStructureStatus::kComputing, pending_update_status)
        << "This method should only be called while computing pending changes, "
           "before updates are made to the tree.";
    std::optional<AXNodeID> parent_node_id = GetParentIdForPendingNode(node_id);
    if (parent_node_id) {
      invalidate_unignored_cached_values_ids.insert(*parent_node_id);
    }
  }

  // Indicates the status for calculating what changes will occur during
  // an update before the update applies changes.
  AXTreePendingStructureStatus pending_update_status =
      AXTreePendingStructureStatus::kNotStarted;

  // Keeps track of the existing tree's root node id when calculating what
  // changes will occur during an update before the update applies changes.
  std::optional<AXNodeID> pending_root_id;

  // Keeps track of whether the root node will need to be created as a new node.
  // This may occur either when the root node does not exist before applying
  // updates to the tree (new tree), or if the root is the |node_id_to_clear|
  // and will be destroyed before applying AXNodeData updates to the tree.
  bool root_will_be_created = false;

  // During an update, this keeps track of all node IDs that have been
  // implicitly referenced as part of this update, but haven't been updated yet.
  // It's an error if there are any pending nodes at the end of Unserialize.
  std::set<AXNodeID> pending_node_ids;

  // before, During and after an update, this keeps track of the nodes' data
  // that have been provided as part of the update.
  std::vector<AXNodeData> updated_nodes;

  // Keeps track of nodes whose cached unignored child count, or unignored
  // index in parent may have changed, and must be updated.
  std::set<AXNodeID> invalidate_unignored_cached_values_ids;

  // Keeps track of nodes that have changed their node data or their ignored
  // state.
  std::set<AXNodeID> node_data_changed_ids;

  // Keeps track of any nodes that are changing their ignored state.
  std::set<AXNodeID> ignored_state_changed_ids;

  // Keeps track of new nodes created during this update.
  std::set<AXNodeID> new_node_ids;

  // Nodes expected to be deleted.
  std::set<AXNodeID> deleting_node_ids;

  // Nodes expected to be reparented.
  std::set<AXNodeID> reparenting_node_ids;

  // Maps between a node id and its pending update information.
  std::map<AXNodeID, std::unique_ptr<PendingStructureChanges>>
      node_id_to_pending_data;

  // Maps between a node id and the data it owned before being updated.
  // We need to keep this around in order to correctly fire post-update events.
  std::map<AXNodeID, AXNodeData> old_node_id_to_data;

  // Optional copy of the old tree data, only populated when the tree data will
  // need to be updated.
  std::optional<AXTreeData> old_tree_data;

  // Optional copy of the updated tree data, used when calculating what changes
  // will occur during an update before the update applies changes.
  std::optional<AXTreeData> new_tree_data;

  // Keep track of the pending tree update to help create useful error messages.
  // TODO(crbug.com/40736019) Revert this once we have the crash data we need
  // (crrev.com/c/2892259).
  const raw_ref<const AXTreeUpdate> pending_tree_update;

 private:
  PendingStructureChanges* GetPendingStructureChanges(AXNodeID node_id) const {
    auto iter = node_id_to_pending_data.find(node_id);
    return (iter != node_id_to_pending_data.cend()) ? iter->second.get()
                                                    : nullptr;
  }

  PendingStructureChanges* GetOrCreatePendingStructureChanges(
      AXNodeID node_id) {
    auto iter = node_id_to_pending_data.find(node_id);
    if (iter == node_id_to_pending_data.cend()) {
      const AXNode* node = tree->GetFromId(node_id);
      iter = node_id_to_pending_data
                 .emplace(std::make_pair(
                     node_id, std::make_unique<PendingStructureChanges>(node)))
                 .first;
    }
    return iter->second.get();
  }

  // We need to hold onto a reference to the AXTree so that we can
  // lazily initialize |PendingStructureChanges| objects.
  const raw_ref<const AXTree> tree;
};

AXTree::NodeSetSizePosInSetInfo::NodeSetSizePosInSetInfo() = default;
AXTree::NodeSetSizePosInSetInfo::~NodeSetSizePosInSetInfo() = default;

struct AXTree::OrderedSetContent {
  explicit OrderedSetContent(const AXNode* ordered_set = nullptr)
      : ordered_set_(ordered_set) {}
  ~OrderedSetContent() = default;

  std::vector<raw_ptr<const AXNode, VectorExperimental>> set_items_;

  // Some ordered set items may not be associated with an ordered set.
  raw_ptr<const AXNode> ordered_set_;
};

struct AXTree::OrderedSetItemsMap {
  OrderedSetItemsMap() = default;
  ~OrderedSetItemsMap() = default;

  // Check if a particular hierarchical level exists in this map.
  bool HierarchicalLevelExists(std::optional<int> level) {
    if (items_map_.find(level) == items_map_.end())
      return false;
    return true;
  }

  // Add the OrderedSetContent to the corresponding hierarchical level in the
  // map.
  void Add(std::optional<int> level,
           const OrderedSetContent& ordered_set_content) {
    if (!HierarchicalLevelExists(level))
      items_map_[level] = std::vector<OrderedSetContent>();

    items_map_[level].push_back(ordered_set_content);
  }

  // Add an ordered set item to the OrderedSetItemsMap given its hierarchical
  // level. We always want to append the item to the last OrderedSetContent of
  // that hierarchical level, due to the following:
  //   - The last OrderedSetContent on any level of the items map is in progress
  //     of being populated.
  //   - All other OrderedSetContent other than the last one on a level
  //     represents a complete ordered set and should not be modified.
  void AddItemToBack(std::optional<int> level, const AXNode* item) {
    if (!HierarchicalLevelExists(level))
      return;

    std::vector<OrderedSetContent>& sets_list = items_map_[level];
    if (!sets_list.empty()) {
      OrderedSetContent& ordered_set_content = sets_list.back();
      ordered_set_content.set_items_.push_back(item);
    }
  }

  // Retrieve the first OrderedSetContent of the OrderedSetItemsMap.
  OrderedSetContent* GetFirstOrderedSetContent() {
    if (items_map_.empty())
      return nullptr;

    std::vector<OrderedSetContent>& sets_list = items_map_.begin()->second;
    if (sets_list.empty())
      return nullptr;

    return &(sets_list.front());
  }

  // Clears all the content in the map.
  void Clear() { items_map_.clear(); }

  // Maps a hierarchical level to a list of OrderedSetContent.
  std::map<std::optional<int32_t>, std::vector<OrderedSetContent>> items_map_;
};

// static
void AXTree::SetFocusedNodeShouldNeverBeIgnored() {
  is_focused_node_always_unignored_ = true;
}

// static
bool AXTree::ComputeNodeIsIgnored(const AXTreeData* optional_tree_data,
                                  const AXNodeData& node_data) {
  // A node with an ARIA presentational role (role="none") should also be
  // ignored.
  bool is_ignored = node_data.HasState(ax::mojom::State::kIgnored) ||
                    node_data.role == ax::mojom::Role::kNone;

  // Exception: We should never ignore focused nodes otherwise users of
  // assistive software might be unable to interact with the webpage.
  //
  // TODO(nektar): This check is erroneous: It's missing a check of
  // focused_tree_id. Fix after updating `AXNode::IsFocusedInThisTree`.
  if (is_focused_node_always_unignored_ && is_ignored && optional_tree_data &&
      optional_tree_data->focus_id != kInvalidAXNodeID &&
      node_data.id == optional_tree_data->focus_id) {
    // If the focus has moved to or away from this node, it can also flip the
    // ignored state, provided that the node's data has the ignored state in the
    // first place. In all other cases, focus cannot affect the ignored state.
    is_ignored = false;
  }

  return is_ignored;
}

// static
bool AXTree::ComputeNodeIsIgnoredChanged(
    const AXTreeData* optional_old_tree_data,
    const AXNodeData& old_node_data,
    const AXTreeData* optional_new_tree_data,
    const AXNodeData& new_node_data) {
  // We should not notify observers of an ignored state change if the node was
  // invisible and continues to be invisible after the update. Also, we should
  // not notify observers if the node has flipped its invisible state from
  // invisible to visible or vice versa. This is because when invisibility
  // changes, the entire subtree is being inserted or removed. For example if
  // the "hidden" CSS property is deleted from a list item, its ignored state
  // will change but the change would be due to the list item becoming visible
  // and thereby adding a whole subtree of nodes, including a list marker and
  // possibly some static text. This situation arises because hidden nodes are
  // included in the internal accessibility tree, but they are marked as
  // ignored.
  //
  // TODO(nektar): This should be dealt with by fixing AXEventGenerator or
  // individual platforms.
  const bool old_node_is_ignored =
      ComputeNodeIsIgnored(optional_old_tree_data, old_node_data);
  const bool new_node_is_ignored =
      ComputeNodeIsIgnored(optional_new_tree_data, new_node_data);
  return old_node_is_ignored != new_node_is_ignored;
}

AXTree::AXTree() {
  // TODO(chrishall): should language_detection_manager be a member or pointer?
  // TODO(chrishall): do we want to initialize all the time, on demand, or only
  //                  when feature flag is set?
  DCHECK(!language_detection_manager);
  language_detection_manager =
      std::make_unique<AXLanguageDetectionManager>(this);
}

AXTree::AXTree(const AXTreeUpdate& initial_state) {
  CHECK(Unserialize(initial_state)) << error();
  DCHECK(!language_detection_manager);
  language_detection_manager =
      std::make_unique<AXLanguageDetectionManager>(this);
}

AXTree::~AXTree() {
  Destroy();

  // Language detection manager will detach from AXTree observer list in its
  // destructor. But because of variable order, when destroying AXTree, the
  // observer list will already be destroyed. To avoid that problem, free
  // language detection manager before.
  language_detection_manager.reset();
}

void AXTree::AddObserver(AXTreeObserver* observer) {
  observers_.AddObserver(observer);
}

bool AXTree::HasObserver(AXTreeObserver* observer) {
  return observers_.HasObserver(observer);
}

void AXTree::RemoveObserver(AXTreeObserver* observer) {
  observers_.RemoveObserver(observer);
}

const AXTreeID& AXTree::GetAXTreeID() const {
  return data().tree_id;
}

const AXTreeData& AXTree::data() const {
  return data_;
}

AXNode* AXTree::GetFromId(AXNodeID id) const {
  if (id == kInvalidAXNodeID) {
    return nullptr;
  }
  auto iter = id_map_.find(id);
  return iter != id_map_.end() ? iter->second.get() : nullptr;
}

void AXTree::Destroy() {
  base::ElapsedThreadTimer timer;

  table_info_map_.clear();
  if (!root_)
    return;

  std::set<AXNodeID> deleting_node_ids;
  RecursivelyNotifyNodeWillBeDeletedForTreeTeardown(*root_, deleting_node_ids);

  observers_.Notify(&AXTreeObserver::OnAtomicUpdateStarting, this,
                    deleting_node_ids, std::set<AXNodeID>{});

  {
    ScopedTreeUpdateInProgressStateSetter tree_update_in_progress(*this);
    // ExtractAsDangling clears the underlying pointer and returns another
    // raw_ptr instance that is allowed to dangle.
    DestroyNodeAndSubtree(root_.ExtractAsDangling(), nullptr);
  }  // tree_update_in_progress.

  UMA_HISTOGRAM_CUSTOM_TIMES("Accessibility.Performance.AXTree.Destroy2",
                             timer.Elapsed(), base::Microseconds(1),
                             base::Seconds(1), 50);
}

void AXTree::UpdateDataForTesting(const AXTreeData& new_data) {
  if (data_ == new_data)
    return;

  AXTreeUpdate update;
  update.has_tree_data = true;
  update.tree_data = new_data;
  Unserialize(update);
}

gfx::RectF AXTree::RelativeToTreeBoundsInternal(const AXNode* node,
                                                gfx::RectF bounds,
                                                bool* offscreen,
                                                bool clip_bounds,
                                                bool skip_container_offset,
                                                bool allow_recursion) const {
  // If |bounds| is uninitialized, which is not the same as empty,
  // start with the node bounds.
  if (bounds.width() == 0 && bounds.height() == 0) {
    bounds = node->data().relative_bounds.bounds;

    // If the node bounds is empty (either width or height is zero),
    // try to compute good bounds from the children.
    // If a tree update is in progress, skip this step as children may be in a
    // bad state.
    if (bounds.IsEmpty() && !GetTreeUpdateInProgressState() &&
        allow_recursion) {
      for (AXNode* child : node->children()) {
        gfx::RectF child_bounds = RelativeToTreeBoundsInternal(
            child, gfx::RectF(), /*offscreen=*/nullptr, clip_bounds,
            skip_container_offset,
            /*allow_recursion=*/false);
        bounds.Union(child_bounds);
      }
      if (bounds.width() > 0 && bounds.height() > 0) {
        return bounds;
      }
    }
  } else if (!skip_container_offset) {
    bounds.Offset(node->data().relative_bounds.bounds.x(),
                  node->data().relative_bounds.bounds.y());
  }

  const AXNode* original_node = node;
  while (node != nullptr) {
    if (node->data().relative_bounds.transform)
      bounds = node->data().relative_bounds.transform->MapRect(bounds);
    // Apply any transforms and offsets for each node and then walk up to
    // its offset container. If no offset container is specified, coordinates
    // are relative to the root node.
    const AXNode* container =
        GetFromId(node->data().relative_bounds.offset_container_id);
    if (!container && container != root())
      container = root();
    if (!container || container == node || skip_container_offset)
      break;

    gfx::RectF container_bounds = container->data().relative_bounds.bounds;
    bounds.Offset(container_bounds.x(), container_bounds.y());

    if (container->HasIntAttribute(ax::mojom::IntAttribute::kScrollX) &&
        container->HasIntAttribute(ax::mojom::IntAttribute::kScrollY)) {
      int scroll_x =
          container->GetIntAttribute(ax::mojom::IntAttribute::kScrollX);
      int scroll_y =
          container->GetIntAttribute(ax::mojom::IntAttribute::kScrollY);
      bounds.Offset(-scroll_x, -scroll_y);
    }

    // Get the intersection between the bounds and the container.
    gfx::RectF intersection = bounds;
    intersection.Intersect(container_bounds);

    // Calculate the clipped bounds to determine offscreen state.
    gfx::RectF clipped = bounds;
    // If this node has the kClipsChildren attribute set, clip the rect to fit.
    if (container->GetBoolAttribute(ax::mojom::BoolAttribute::kClipsChildren)) {
      if (!intersection.IsEmpty()) {
        // We can simply clip it to the container.
        clipped = intersection;
      } else {
        // Totally offscreen. Find the nearest edge or corner.
        // Make the minimum dimension 1 instead of 0.
        if (clipped.x() >= container_bounds.width()) {
          clipped.set_x(container_bounds.right() - 1);
          clipped.set_width(1);
        } else if (clipped.x() + clipped.width() <= 0) {
          clipped.set_x(container_bounds.x());
          clipped.set_width(1);
        }
        if (clipped.y() >= container_bounds.height()) {
          clipped.set_y(container_bounds.bottom() - 1);
          clipped.set_height(1);
        } else if (clipped.y() + clipped.height() <= 0) {
          clipped.set_y(container_bounds.y());
          clipped.set_height(1);
        }
      }
    }

    if (clip_bounds)
      bounds = clipped;

    if (container->GetBoolAttribute(ax::mojom::BoolAttribute::kClipsChildren) &&
        intersection.IsEmpty() && !clipped.IsEmpty()) {
      // If it is offscreen with respect to its parent, and the node itself is
      // not empty, label it offscreen.
      // Here we are extending the definition of offscreen to include elements
      // that are clipped by their parents in addition to those clipped by
      // the rootWebArea.
      // No need to update |offscreen| if |intersection| is not empty, because
      // it should be false by default.
      if (offscreen != nullptr)
        *offscreen |= true;
    }

    node = container;
  }

  // If we don't have any size yet, try to adjust the bounds to fill the
  // nearest ancestor that does have bounds.
  //
  // The rationale is that it's not useful to the user for an object to
  // have no width or height and it's probably a bug; it's better to
  // reflect the bounds of the nearest ancestor rather than a 0x0 box.
  // Tag this node as 'offscreen' because it has no true size, just a
  // size inherited from the ancestor.
  if (bounds.width() == 0 && bounds.height() == 0) {
    const AXNode* ancestor = original_node->parent();
    gfx::RectF ancestor_bounds;
    while (ancestor) {
      ancestor_bounds = ancestor->data().relative_bounds.bounds;
      if (ancestor_bounds.width() > 0 || ancestor_bounds.height() > 0)
        break;
      ancestor = ancestor->parent();
    }

    if (ancestor && allow_recursion) {
      bool ignore_offscreen = false;
      ancestor_bounds = RelativeToTreeBoundsInternal(
          ancestor, gfx::RectF(), &ignore_offscreen, clip_bounds,
          skip_container_offset,
          /* allow_recursion = */ false);

      gfx::RectF original_bounds = original_node->data().relative_bounds.bounds;
      if (original_bounds.x() == 0 && original_bounds.y() == 0) {
        bounds = ancestor_bounds;
      } else {
        bounds.set_width(std::max(0.0f, ancestor_bounds.right() - bounds.x()));
        bounds.set_height(
            std::max(0.0f, ancestor_bounds.bottom() - bounds.y()));
      }
      if (offscreen != nullptr)
        *offscreen |= true;
    }
  }

  return bounds;
}

gfx::RectF AXTree::RelativeToTreeBounds(const AXNode* node,
                                        gfx::RectF bounds,
                                        bool* offscreen,
                                        bool clip_bounds,
                                        bool skip_container_offset) const {
  bool allow_recursion = true;
  return RelativeToTreeBoundsInternal(node, bounds, offscreen, clip_bounds,
                                      skip_container_offset, allow_recursion);
}

gfx::RectF AXTree::GetTreeBounds(const AXNode* node,
                                 bool* offscreen,
                                 bool clip_bounds) const {
  return RelativeToTreeBounds(node, gfx::RectF(), offscreen, clip_bounds);
}

std::set<AXNodeID> AXTree::GetReverseRelations(ax::mojom::IntAttribute attr,
                                               AXNodeID dst_id) const {
  DCHECK(IsNodeIdIntAttribute(attr));

  // Conceptually, this is the "const" version of:
  //   return int_reverse_relations_[attr][dst_id];
  const auto& attr_relations = int_reverse_relations_.find(attr);
  if (attr_relations != int_reverse_relations_.end()) {
    const auto& result = attr_relations->second.find(dst_id);
    if (result != attr_relations->second.end())
      return result->second;
  }
  return std::set<AXNodeID>();
}

std::set<AXNodeID> AXTree::GetReverseRelations(ax::mojom::IntListAttribute attr,
                                               AXNodeID dst_id) const {
  DCHECK(IsNodeIdIntListAttribute(attr));

  // Conceptually, this is the "const" version of:
  //   return intlist_reverse_relations_[attr][dst_id];
  const auto& attr_relations = intlist_reverse_relations_.find(attr);
  if (attr_relations != intlist_reverse_relations_.end()) {
    const auto& result = attr_relations->second.find(dst_id);
    if (result != attr_relations->second.end())
      return result->second;
  }
  return std::set<AXNodeID>();
}

std::set<AXNodeID> AXTree::GetNodeIdsForChildTreeId(
    AXTreeID child_tree_id) const {
  // Conceptually, this is the "const" version of:
  //   return child_tree_id_reverse_map_[child_tree_id];
  const auto& result = child_tree_id_reverse_map_.find(child_tree_id);
  if (result != child_tree_id_reverse_map_.end())
    return result->second;
  return std::set<AXNodeID>();
}

const std::set<AXTreeID> AXTree::GetAllChildTreeIds() const {
  std::set<AXTreeID> result;
  for (auto entry : child_tree_id_reverse_map_)
    result.insert(entry.first);
  return result;
}

bool AXTree::Unserialize(const AXTreeUpdate& update) {
#if DCHECK_IS_ON()
  for (const auto& new_data : update.nodes)
    DCHECK(new_data.id != kInvalidAXNodeID)
        << "AXTreeUpdate contains invalid node: " << update.ToString();
  if (update.tree_data.tree_id != AXTreeIDUnknown() &&
      data_.tree_id != AXTreeIDUnknown()) {
    DCHECK_EQ(update.tree_data.tree_id, data_.tree_id)
        << "Tree id mismatch between tree update and this tree.";
  }
#endif

  event_data_ = std::make_unique<AXEvent>();
  event_data_->event_from = update.event_from;
  event_data_->event_from_action = update.event_from_action;
  event_data_->event_intents = update.event_intents;
  absl::Cleanup clear_event_data = [this] { event_data_.reset(); };

  AXTreeUpdateState update_state(*this, update);
  const AXNodeID old_root_id = root_ ? root_->id() : kInvalidAXNodeID;
  DCHECK(old_root_id != kInvalidAXNodeID || update.root_id != kInvalidAXNodeID)
      << "Tree must have a valid root or update must have a valid root.";

  // Accumulates the work that will be required to update the AXTree.
  // This allows us to notify observers of structure changes when the
  // tree is still in a stable and unchanged state.
  if (!ComputePendingChanges(update, &update_state))
    return false;

  // Log unserialize perf after early returns.
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "Accessibility.Performance.Tree.Unserialize2");

  // Notify observers of subtrees and nodes that are about to be destroyed or
  // reparented, this must be done before applying any updates to the tree.
  for (auto&& pair : update_state.node_id_to_pending_data) {
    const AXNodeID node_id = pair.first;
    const std::unique_ptr<PendingStructureChanges>& data = pair.second;
    if (data->DoesNodeExpectSubtreeOrNodeWillBeDestroyed()) {
      if (AXNode* node = GetFromId(node_id)) {
        if (data->DoesNodeExpectSubtreeWillBeDestroyed()) {
          NotifySubtreeWillBeReparentedOrDeleted(node, &update_state);
        }
        if (data->DoesNodeExpectNodeWillBeDestroyed()) {
          NotifyNodeWillBeReparentedOrDeleted(*node, update_state);
        }
      }
    }
  }

  // Notify observers of nodes that are about to change their ignored state or
  // their data. This must be done before applying any updates to the tree. This
  // is iterating in reverse order so that we only notify once per node id, and
  // so that we only notify the initial node data against the final node data,
  // unless the node is a new root.
  std::set<AXNodeID> notified_node_attributes_will_change;
  for (const auto& new_data : update_state.updated_nodes) {
    const bool is_new_root =
        update_state.root_will_be_created && new_data.id == update.root_id;
    if (is_new_root) {
      continue;
    }

    AXNode* node = GetFromId(new_data.id);

    // For performance, skip text deletion/insertion events on ignored nodes.
    if (node && !new_data.IsIgnored() && !node->data().IsIgnored() &&
        notified_node_attributes_will_change.insert(new_data.id).second) {
      if (new_data.HasIntListAttribute(
              ax::mojom::IntListAttribute::kTextOperationStartOffsets)) {
        DCHECK(new_data.HasIntListAttribute(
            ax::mojom::IntListAttribute::kTextOperationStartOffsets));
        DCHECK(new_data.HasIntListAttribute(
            ax::mojom::IntListAttribute::kTextOperationEndOffsets));
        DCHECK(new_data.HasIntListAttribute(
            ax::mojom::IntListAttribute::kTextOperationStartAnchorIds));
        DCHECK(new_data.HasIntListAttribute(
            ax::mojom::IntListAttribute::kTextOperationEndAnchorIds));
        DCHECK(new_data.HasIntListAttribute(
            ax::mojom::IntListAttribute::kTextOperations));
        observers_.Notify(&AXTreeObserver::OnTextDeletionOrInsertion, *node,
                          new_data);
      }
      NotifyNodeAttributesWillChange(
          node, update_state,
          update_state.old_tree_data ? &update_state.old_tree_data.value()
                                     : nullptr,
          node->data(),
          update_state.new_tree_data ? &update_state.new_tree_data.value()
                                     : nullptr,
          new_data);
    }
  }

  // Notify observers of nodes about to change their ignored state.
  for (AXNodeID id : update_state.ignored_state_changed_ids) {
    AXNode* node = GetFromId(id);
    if (node) {
      bool will_be_ignored = !node->IsIgnored();
      // Don't fire ignored state change when the parent is also changing to
      // the same ignored state.
      bool is_root_of_ignored_change =
          !node->parent() ||
          !base::Contains(update_state.ignored_state_changed_ids,
                          node->parent()->id()) ||
          node->IsIgnored() != node->parent()->IsIgnored();
      observers_.Notify(&AXTreeObserver::OnIgnoredWillChange, this, node,
                        will_be_ignored, is_root_of_ignored_change);
    }
  }

  observers_.Notify(&AXTreeObserver::OnAtomicUpdateStarting, this,
                    update_state.deleting_node_ids,
                    update_state.reparenting_node_ids);

  // Now that we have finished sending events for changes that will  happen,
  // set update state to true. |tree_update_in_progress_| gets set back to
  // false whenever this function exits.
  std::vector<AXTreeObserver::Change> changes;
  {
    ScopedTreeUpdateInProgressStateSetter tree_update_in_progress(*this);

    // Update the tree data. Do not call `UpdateDataForTesting` since this
    // method should be used only for testing, but importantly, we want to defer
    // the `OnTreeDataChanged` event until after the tree has finished updating.
    if (update_state.new_tree_data)
      data_ = update.tree_data;

    // Handle |node_id_to_clear| before applying ordinary node updates.
    // We distinguish between updating the root, e.g. changing its children or
    // some of its attributes, or replacing the root completely. If the root is
    // being updated, update.node_id_to_clear should hold the current root's ID.
    // Otherwise if the root is being replaced, update.root_id should hold the
    // ID of the new root.
    bool root_updated = false;
    if (update.node_id_to_clear != kInvalidAXNodeID) {
      // If the incoming tree was initialized with a root with an id != 1, the
      // update won't match the tree created by CreateEmptyDocument In this
      // case, the update won't be able to set the right node_id_to_clear.
      // If node_id_to_clear was set and the update's root_id doesn't match the
      // old_root_id, we assume that the update meant to replace the root.
      int node_id_to_clear = update.node_id_to_clear;
      if (!GetFromId(node_id_to_clear) && update.root_id == node_id_to_clear &&
          update.root_id != old_root_id && root_) {
        node_id_to_clear = old_root_id;
      }
      if (AXNode* cleared_node = GetFromId(node_id_to_clear)) {
        DCHECK(root_);
        if (cleared_node == root_) {
          // Only destroy the root if the root was replaced and not if it's
          // simply updated. To figure out if the root was simply updated, we
          // compare the ID of the new root with the existing root ID.
          if (update.root_id != old_root_id) {
            // Clear root_ before calling DestroySubtree so that root_ doesn't
            // ever point to an invalid node.
            AXNode* old_root = root_;
            root_ = nullptr;
            DestroySubtree(old_root, &update_state);
          } else {
            // If the root has simply been updated, we treat it like an update
            // to any other node.
            root_updated = true;
          }
        }

        // If the tree doesn't exists any more because the root has just been
        // replaced, there is nothing more to clear.
        if (root_) {
          for (AXNode* child : cleared_node->children()) {
            DestroySubtree(child, &update_state);
          }
          std::vector<raw_ptr<AXNode, VectorExperimental>> children;
          cleared_node->SwapChildren(&children);
          update_state.pending_node_ids.insert(cleared_node->id());
        }
      }
    }

    DCHECK_EQ(update.root_id != kInvalidAXNodeID && !GetFromId(update.root_id),
              update_state.root_will_be_created);

    // Update all of the nodes in the update.
    for (const AXNodeData& updated_node_data : update_state.updated_nodes) {
      const bool is_new_root = update_state.root_will_be_created &&
                               updated_node_data.id == update.root_id;
      if (!UpdateNode(updated_node_data, is_new_root, &update_state))
        return false;
    }

    if (!root_) {
      ACCESSIBILITY_TREE_UNSERIALIZE_ERROR_HISTOGRAM(
          AXTreeUnserializeError::kNoRoot);
      RecordError(update_state, "Tree has no root.", true);
      return false;
    }

    if (!ValidatePendingChangesComplete(update_state))
      return false;

    changes.reserve(update_state.updated_nodes.size());

    // Look for changes to nodes that are a descendant of a table,
    // and invalidate their table info if so.  We have to walk up the
    // ancestry of every node that was updated potentially, so keep track of
    // ids that were checked to eliminate duplicate work.
    std::set<AXNodeID> table_ids_checked;
    for (const AXNodeData& node_data : update_state.updated_nodes) {
      AXNode* node = GetFromId(node_data.id);
      while (node) {
        if (table_ids_checked.find(node->id()) != table_ids_checked.end())
          break;
        // Remove any table infos.
        const auto& table_info_entry = table_info_map_.find(node->id());
        if (table_info_entry != table_info_map_.end()) {
          table_info_entry->second->Invalidate();
#if defined(AX_EXTRA_MAC_NODES)
          // It will emit children changed notification on mac to make sure that
          // extra mac accessibles are recreated.
          changes.emplace_back(node, AXTreeObserver::NODE_CHANGED);
#endif
        }
        table_ids_checked.insert(node->id());
        node = node->parent();
      }
    }

    // Clears |node_set_size_pos_in_set_info_map_|
    node_set_size_pos_in_set_info_map_.clear();

    // A set to track which nodes have already been added to |changes|, so that
    // nodes aren't added twice.
    std::set<AXNodeID> visited_observer_changes;

    for (const AXNodeData& updated_node_data : update_state.updated_nodes) {
      AXNode* node = GetFromId(updated_node_data.id);
      if (!node ||
          !visited_observer_changes.emplace(updated_node_data.id).second)
        continue;

      bool is_new_node = update_state.IsCreatedNode(node);
      bool is_reparented_node = update_state.IsReparentedNode(node);

      AXTreeObserver::ChangeType change = AXTreeObserver::NODE_CHANGED;
      if (is_new_node) {
        if (is_reparented_node) {
          // A reparented subtree is any new node whose parent either doesn't
          // exist, or whose parent is not new.
          // Note that we also need to check for the special case when we update
          // the root without replacing it.
          bool is_subtree = !node->parent() ||
                            !update_state.IsCreatedNode(node->parent()) ||
                            (node->parent() == root_ && root_updated);
          change = is_subtree ? AXTreeObserver::SUBTREE_REPARENTED
                              : AXTreeObserver::NODE_REPARENTED;
        } else {
          // A new subtree is any new node whose parent is either not new, or
          // whose parent happens to be new only because it has been reparented.
          // Note that we also need to check for the special case when we update
          // the root without replacing it.
          bool is_subtree =
              !node->parent() || !update_state.IsCreatedNode(node->parent()) ||
              update_state.IsReparentedNode(node->parent()->id()) ||
              (node->parent() == root_ && root_updated);
          change = is_subtree ? AXTreeObserver::SUBTREE_CREATED
                              : AXTreeObserver::NODE_CREATED;
        }
      }
      changes.emplace_back(node, change);
    }

    // Clear cached information in `AXComputedNodeData` for every node that has
    // been changed in any way, including because of changes to one of its
    // descendants.
    std::set<AXNodeID> cleared_computed_node_data_ids;
    for (AXNodeID node_id : update_state.node_data_changed_ids) {
      AXNode* node = GetFromId(node_id);
      while (node) {
        if (cleared_computed_node_data_ids.insert(node->id()).second)
          node->ClearComputedNodeData();
        node = node->parent();
      }
    }

    // Update the unignored cached values as necessary, ensuring that we only
    // update once for each unignored node.
    // If the node is ignored, we must update from an unignored ancestor.
    // TODO(alexs) Look into removing this loop and adding unignored ancestors
    // at the same place we add ids to updated_unignored_cached_values_ids.
    std::set<AXNodeID> updated_unignored_cached_values_ids;
    for (AXNodeID node_id :
         update_state.invalidate_unignored_cached_values_ids) {
      AXNode* unignored_ancestor = GetUnignoredAncestorFromId(node_id);
      if (unignored_ancestor &&
          updated_unignored_cached_values_ids.insert(unignored_ancestor->id())
              .second) {
        unignored_ancestor->UpdateUnignoredCachedValues();
        // If the node was ignored, then it's unignored ancestor need to be
        // considered part of the changed node list, allowing properties such as
        // hypertext to be recomputed.
        if (unignored_ancestor->id() != node_id &&
            visited_observer_changes.emplace(unignored_ancestor->id()).second) {
          changes.emplace_back(unignored_ancestor,
                               AXTreeObserver::NODE_CHANGED);
        }
      }
    }

  }  // tree_update_in_progress.

  if (update_state.old_tree_data) {
    DCHECK(update.has_tree_data)
        << "If `UpdateState::old_tree_data` exists, then there must be a "
           "request to update the tree data.";

    // Now that the tree is stable and its nodes have been updated, notify if
    // the tree data changed. We must do this after updating nodes in case the
    // root has been replaced, so observers have the most up-to-date
    // information.
    observers_.Notify(&AXTreeObserver::OnTreeDataChanged, this,
                      *update_state.old_tree_data, data_);
  }

  // Now that the unignored cached values are up to date, notify observers of
  // new nodes in the tree. This is done before notifications of deleted nodes,
  // because deleting nodes can cause events to be fired, which will need to
  // access the root, and therefore the BrowserAccessibilityManager needs to be
  // aware of any newly created root as soon as possible.
  for (AXNodeID node_id : update_state.new_node_ids) {
    AXNode* node = GetFromId(node_id);
    if (node) {
      NotifyNodeHasBeenReparentedOrCreated(node, &update_state);
    }
  }

  // Now that the unignored cached values are up to date, notify observers of
  // the nodes that were deleted from the tree but not reparented.
  for (AXNodeID node_id : update_state.deleting_node_ids) {
    NotifyNodeHasBeenDeleted(node_id);
  }

  // Now that the unignored cached values are up to date, notify observers of
  // node changes.
  for (AXNodeID changed_id : update_state.node_data_changed_ids) {
    AXNode* node = GetFromId(changed_id);
    DCHECK(node);

    // If the node exists and is in the old data map, then the node data
    // may have changed unless this is a new root.
    const bool is_new_root =
        update_state.root_will_be_created && changed_id == update.root_id;
    if (!is_new_root) {
      auto it = update_state.old_node_id_to_data.find(changed_id);
      if (it != update_state.old_node_id_to_data.end()) {
        NotifyNodeAttributesHaveBeenChanged(
            node, update_state,
            update_state.old_tree_data ? &update_state.old_tree_data.value()
                                       : nullptr,
            it->second,
            update_state.new_tree_data ? &update_state.new_tree_data.value()
                                       : nullptr,
            node->data());
      }
    }

    // |OnNodeChanged| should be fired for all nodes that have been updated.
    observers_.Notify(&AXTreeObserver::OnNodeChanged, this, node);
  }

  observers_.Notify(&AXTreeObserver::OnAtomicUpdateFinished, this,
                    root_->id() != old_root_id, changes);

#if DCHECK_IS_ON()
  CheckTreeConsistency(update);
#endif

  return true;
}

#if DCHECK_IS_ON()
void AXTree::CheckTreeConsistency(const AXTreeUpdate& update) {
  // Return early if no expected node count was supplied.
  if (!update.tree_checks || !update.tree_checks->node_count) {
    return;
  }

  // Return early if the expected node count matches the node ids mapped.
  if (update.tree_checks->node_count == id_map_.size()) {
    return;
  }

  DCHECK(root_);
  std::ostringstream msg;
  msg << "After a tree update, there is a tree inconsistency.\n"
      << "\n* Number of ids mapped: " << id_map_.size()
      << "\n* Serializer's node count: " << update.tree_checks->node_count
      << "\n* Slow nodes count: " << root_->GetSubtreeCount()
      << "\n* AXTreeUpdate: "
      << TreeToStringHelper(root_, 0, /*verbose*/ false);

  DCHECK(false) << msg.str();
}
#endif

AXTableInfo* AXTree::GetTableInfo(const AXNode* const_table_node) const {
  DCHECK(!GetTreeUpdateInProgressState());
  // Note: the const_casts are here because we want this function to be able
  // to be called from a const virtual function on AXNode. AXTableInfo is
  // computed on demand and cached, but that's an implementation detail
  // we want to hide from users of this API.
  AXNode* table_node = const_cast<AXNode*>(const_table_node);
  AXTree* tree = const_cast<AXTree*>(this);

  DCHECK(table_node);
  const auto& cached = table_info_map_.find(table_node->id());
  if (cached != table_info_map_.end()) {
    // Get existing table info, and update if invalid because the
    // tree has changed since the last time we accessed it.
    AXTableInfo* table_info = cached->second.get();
    if (!table_info->valid()) {
      if (!table_info->Update()) {
        // If Update() returned false, this is no longer a valid table.
        // Remove it from the map.
        table_info_map_.erase(table_node->id());
        return nullptr;
      }
    }
    return table_info;
  }

  AXTableInfo* table_info = AXTableInfo::Create(tree, table_node);
  if (!table_info)
    return nullptr;

  table_info_map_[table_node->id()] = base::WrapUnique<AXTableInfo>(table_info);
  return table_info;
}

std::string AXTree::ToString(bool verbose) const {
  return "AXTree" + data_.ToString() + "\n" +
         TreeToStringHelper(root_, 0, verbose);
}

AXNode* AXTree::CreateNode(AXNode* parent,
                           AXNodeID id,
                           size_t index_in_parent,
                           AXTreeUpdateState* update_state) {
  DCHECK(GetTreeUpdateInProgressState());
  // |update_state| must already contain information about all of the expected
  // changes and invalidations to apply. If any of these are missing, observers
  // may not be notified of changes.
  SANITIZER_CHECK(id != kInvalidAXNodeID);
  DCHECK(!GetFromId(id));
  DCHECK_GT(update_state->GetPendingCreateNodeCount(id), 0);
  DCHECK(update_state->InvalidatesUnignoredCachedValues(id));
  DCHECK(!parent ||
         update_state->InvalidatesUnignoredCachedValues(parent->id()));
  update_state->DecrementPendingCreateNodeCount(id);
  update_state->new_node_ids.insert(id);
  // If this node is the root, use the given index_in_parent as the unignored
  // index in parent to provide consistency with index_in_parent.
  auto node = std::make_unique<AXNode>(this, parent, id, index_in_parent,
                                       parent ? 0 : index_in_parent);
  auto emplaced = id_map_.emplace(id, std::move(node));
  // There should not have been a node already in the map with the same id.
  DCHECK(emplaced.second);
  return emplaced.first->second.get();
}

bool AXTree::ComputePendingChanges(const AXTreeUpdate& update,
                                   AXTreeUpdateState* update_state) {
  DCHECK_EQ(AXTreePendingStructureStatus::kNotStarted,
            update_state->pending_update_status)
      << "Pending changes have already started being computed.";
  update_state->pending_update_status =
      AXTreePendingStructureStatus::kComputing;

  // The ID of the current root is temporarily stored in `update_state`, but
  // reset after all pending updates have been computed in order to avoid stale
  // data hanging around.
  base::AutoReset<std::optional<AXNodeID>> pending_root_id_resetter(
      &update_state->pending_root_id,
      root_ ? std::make_optional<AXNodeID>(root_->id()) : std::nullopt);

  if (update.has_tree_data && data_ != update.tree_data) {
    update_state->old_tree_data = data_;
    update_state->new_tree_data = update.tree_data;
  }
  update_state->updated_nodes = update.nodes;

  // We distinguish between updating the root, e.g. changing its children or
  // some of its attributes, or replacing the root completely. If the root is
  // being updated, update.node_id_to_clear should hold the current root's ID.
  // Otherwise if the root is being replaced, update.root_id should hold the ID
  // of the new root.
  if (update.node_id_to_clear != kInvalidAXNodeID) {
    if (AXNode* cleared_node = GetFromId(update.node_id_to_clear)) {
      DCHECK(root_);
      if (cleared_node == root_ &&
          update.root_id != update_state->pending_root_id) {
        // Only destroy the root if the root was replaced and not if it's simply
        // updated. To figure out if the root was simply updated, we compare
        // the ID of the new root with the existing root ID.
        MarkSubtreeForDestruction(*update_state->pending_root_id, update_state);
      }

      // If the tree has been marked for destruction because the root will be
      // replaced, there is nothing more to clear.
      if (update_state->ShouldPendingNodeExistInTree(root_->id())) {
        update_state->invalidate_unignored_cached_values_ids.insert(
            cleared_node->id());
        update_state->ClearLastKnownPendingNodeData(cleared_node->id());
        for (AXNode* child : cleared_node->children()) {
          MarkSubtreeForDestruction(child->id(), update_state);
        }
      }
    }
  }

  if (is_focused_node_always_unignored_ && update_state->old_tree_data &&
      update_state->new_tree_data) {
    // Ensure that if the focused node has changed, any unignored cached values
    // would be invalidated on both the previous as well as the new focus, in
    // cases where their ignored state will be affected. This block is necessary
    // in the rare situation when the focus node has changed but the previous or
    // new focused nodes are not in the list of updated nodes, because their
    // data has not been modified.

    // TODO(nektar): This check is erroneous: It's missing a check of
    // focused_tree_id. Fix after updating `AXNode::IsFocusedInThisTree`.

    if (update_state->old_tree_data->focus_id != kInvalidAXNodeID) {
      const AXNode* old_focus =
          GetFromId(update_state->old_tree_data->focus_id);
      if (old_focus &&
          update_state->ShouldPendingNodeExistInTree(old_focus->id()) &&
          !base::Contains(update_state->updated_nodes, old_focus->id(),
                          &AXNodeData::id)) {
        update_state->updated_nodes.push_back(old_focus->data());
      }
    }

    if (update_state->new_tree_data->focus_id != kInvalidAXNodeID) {
      const AXNode* new_focus =
          GetFromId(update_state->new_tree_data->focus_id);
      if (new_focus &&
          update_state->ShouldPendingNodeExistInTree(new_focus->id()) &&
          !base::Contains(update_state->updated_nodes, new_focus->id(),
                          &AXNodeData::id)) {
        update_state->updated_nodes.push_back(new_focus->data());
      }
    }
  }

  if (update.root_id != kInvalidAXNodeID) {
    update_state->root_will_be_created =
        !GetFromId(update.root_id) ||
        !update_state->ShouldPendingNodeExistInTree(update.root_id);
  }

  // Populate |update_state| with all of the changes that will be performed
  // on the tree during the update.
  for (const AXNodeData& new_data : update_state->updated_nodes) {
    if (new_data.id == kInvalidAXNodeID)
      continue;
    bool is_new_root =
        update_state->root_will_be_created && new_data.id == update.root_id;
    if (!ComputePendingChangesToNode(new_data, is_new_root, update_state)) {
      update_state->pending_update_status =
          AXTreePendingStructureStatus::kFailed;
      return false;
    }
  }

  update_state->pending_update_status = AXTreePendingStructureStatus::kComplete;
  return true;
}

bool AXTree::ComputePendingChangesToNode(const AXNodeData& new_data,
                                         bool is_new_root,
                                         AXTreeUpdateState* update_state) {
  // Compare every child's index in parent in the update with the existing
  // index in parent.  If the order has changed, invalidate the cached
  // unignored index in parent.
  for (size_t j = 0; j < new_data.child_ids.size(); j++) {
    const AXNode* node = GetFromId(new_data.child_ids[j]);
    if (node && node->GetIndexInParent() != j)
      update_state->InvalidateParentNodeUnignoredCacheValues(node->id());
  }

  // If the node does not exist in the tree throw an error unless this
  // is the new root and it can be created.
  if (!update_state->ShouldPendingNodeExistInTree(new_data.id)) {
    if (!is_new_root) {
      ACCESSIBILITY_TREE_UNSERIALIZE_ERROR_HISTOGRAM(
          AXTreeUnserializeError::kNotInTree);
      RecordError(*update_state,
                  base::StringPrintf(
                      "%d will not be in the tree and is not the new root",
                      new_data.id));
      return false;
    }

    // Creation is implicit for new root nodes. If |new_data.id| is already
    // pending for creation, then it must be a duplicate entry in the tree.
    if (!update_state->IncrementPendingCreateNodeCount(new_data.id,
                                                       std::nullopt)) {
      ACCESSIBILITY_TREE_UNSERIALIZE_ERROR_HISTOGRAM(
          AXTreeUnserializeError::kCreationPending);
      RecordError(
          *update_state,
          base::StringPrintf(
              "Node %d is already pending for creation, cannot be the new root",
              new_data.id));
      return false;
    }
    if (update_state->pending_root_id) {
      MarkSubtreeForDestruction(*update_state->pending_root_id, update_state);
    }
    update_state->pending_root_id = new_data.id;
  }

  // Create a set of new child ids so we can use it to find the nodes that
  // have been added and removed. Returns false if a duplicate is found.
  std::set<AXNodeID> new_child_id_set;
  for (AXNodeID new_child_id : new_data.child_ids) {
    if (base::Contains(new_child_id_set, new_child_id)) {
      ACCESSIBILITY_TREE_UNSERIALIZE_ERROR_HISTOGRAM(
          AXTreeUnserializeError::kDuplicateChild);
      RecordError(*update_state,
                  base::StringPrintf("Node %d has duplicate child id %d",
                                     new_data.id, new_child_id));
      return false;
    }
    new_child_id_set.insert(new_child_id);
  }

  // If the node has not been initialized yet then its node data has either been
  // cleared when handling |node_id_to_clear|, or it's a new node.
  // In either case, all children must be created.
  if (update_state->DoesPendingNodeRequireInit(new_data.id)) {
    update_state->invalidate_unignored_cached_values_ids.insert(new_data.id);

    // If this node has been cleared via |node_id_to_clear| or is a new node,
    // the last-known parent's unignored cache needs to be updated.
    update_state->InvalidateParentNodeUnignoredCacheValues(new_data.id);

    for (AXNodeID child_id : new_child_id_set) {
      // If a |child_id| is already pending for creation, then it must be a
      // duplicate entry in the tree.
      update_state->invalidate_unignored_cached_values_ids.insert(child_id);
      if (!update_state->IncrementPendingCreateNodeCount(child_id,
                                                         new_data.id)) {
        ACCESSIBILITY_TREE_UNSERIALIZE_ERROR_HISTOGRAM(
            AXTreeUnserializeError::kCreationPendingForChild);
        RecordError(*update_state,
                    base::StringPrintf("Node %d is already pending for "
                                       "creation, cannot be a new child",
                                       child_id));
        return false;
      }
    }

    update_state->SetLastKnownPendingNodeData(&new_data);
    return true;
  }

  const AXNodeData& old_data =
      update_state->GetLastKnownPendingNodeData(new_data.id);

  AXTreeData* old_tree_data = update_state->old_tree_data
                                  ? &update_state->old_tree_data.value()
                                  : nullptr;
  AXTreeData* new_tree_data = update_state->new_tree_data
                                  ? &update_state->new_tree_data.value()
                                  : nullptr;
  if (ComputeNodeIsIgnoredChanged(old_tree_data, old_data, new_tree_data,
                                  new_data)) {
    update_state->ignored_state_changed_ids.insert(new_data.id);
  }

  // Create a set of old child ids so we can use it to find the nodes that
  // have been added and removed.
  std::set<AXNodeID> old_child_id_set(old_data.child_ids.cbegin(),
                                      old_data.child_ids.cend());

  std::vector<AXNodeID> create_or_destroy_ids;
  std::set_symmetric_difference(
      old_child_id_set.cbegin(), old_child_id_set.cend(),
      new_child_id_set.cbegin(), new_child_id_set.cend(),
      std::back_inserter(create_or_destroy_ids));

  // If the node has changed ignored state or there are any differences in
  // its children, then its unignored cached values must be invalidated.
  if (!create_or_destroy_ids.empty() ||
      update_state->HasIgnoredChanged(new_data)) {
    update_state->invalidate_unignored_cached_values_ids.insert(new_data.id);

    // If this ignored state had changed also invalidate the parent.
    update_state->InvalidateParentNodeUnignoredCacheValues(new_data.id);
  }

  for (AXNodeID child_id : create_or_destroy_ids) {
    if (base::Contains(new_child_id_set, child_id)) {
      // This is a serious error - nodes should never be reparented without
      // first being removed from the tree. If a node exists in the tree already
      // then adding it to a new parent would mean stealing the node from its
      // old parent which hadn't been updated to reflect the change.
      if (update_state->ShouldPendingNodeExistInTree(child_id)) {
        ACCESSIBILITY_TREE_UNSERIALIZE_ERROR_HISTOGRAM(
            AXTreeUnserializeError::kReparent);
        RecordError(*update_state,
                    base::StringPrintf("Node %d is not marked for destruction, "
                                       "would be reparented to %d",
                                       child_id, new_data.id));
        return false;
      }

      // If a |child_id| is already pending for creation, then it must be a
      // duplicate entry in the tree.
      update_state->invalidate_unignored_cached_values_ids.insert(child_id);
      if (!update_state->IncrementPendingCreateNodeCount(child_id,
                                                         new_data.id)) {
        ACCESSIBILITY_TREE_UNSERIALIZE_ERROR_HISTOGRAM(
            AXTreeUnserializeError::kCreationPendingForChild);
        RecordError(*update_state,
                    base::StringPrintf("Node %d is already pending for "
                                       "creation, cannot be a new child",
                                       child_id));
        return false;
      }
    } else {
      // If |child_id| does not exist in the new set, then it has
      // been removed from |node|, and the subtree must be deleted.
      MarkSubtreeForDestruction(child_id, update_state);
    }
  }

  update_state->SetLastKnownPendingNodeData(&new_data);
  return true;
}

bool AXTree::UpdateNode(const AXNodeData& src,
                        bool is_new_root,
                        AXTreeUpdateState* update_state) {
  DCHECK(GetTreeUpdateInProgressState());
  // This method updates one node in the tree based on serialized data
  // received in an AXTreeUpdate. See AXTreeUpdate for pre and post
  // conditions.

  // Look up the node by id. If it's not found, then either the root
  // of the tree is being swapped, or we're out of sync with the source
  // and this is a serious error.
  AXNode* node = GetFromId(src.id);
  if (node) {
    // Node is changing.
    update_state->pending_node_ids.erase(node->id());
    UpdateReverseRelations(node, src);
    if (!update_state->IsCreatedNode(node) ||
        update_state->IsReparentedNode(node)) {
      update_state->old_node_id_to_data.insert(
          std::make_pair(node->id(), node->TakeData()));
    }
    node->SetData(src);
  } else {
    // Node is created.
    if (!is_new_root) {
      ACCESSIBILITY_TREE_UNSERIALIZE_ERROR_HISTOGRAM(
          AXTreeUnserializeError::kNotInTree);
      RecordError(*update_state,
                  base::StringPrintf(
                      "%d is not in the tree and not the new root", src.id));
      return false;
    }

    node = CreateNode(nullptr, src.id, 0, update_state);
    UpdateReverseRelations(node, src, /*is_new_node*/ true);
    node->SetData(src);
  }

  // If we come across a page breaking object, mark the tree as a paginated root
  if (src.GetBoolAttribute(ax::mojom::BoolAttribute::kIsPageBreakingObject))
    has_pagination_support_ = true;

  update_state->node_data_changed_ids.insert(node->id());

  // First, delete nodes that used to be children of this node but aren't
  // anymore.
  DeleteOldChildren(node, src.child_ids, update_state);

  // Now build a new children vector, reusing nodes when possible,
  // and swap it in.
  std::vector<raw_ptr<AXNode, VectorExperimental>> new_children;
  bool success = CreateNewChildVector(
      node, src.child_ids, &new_children, update_state);
  node->SwapChildren(&new_children);

  // Update the root of the tree if needed.
  if (is_new_root) {
    // Make sure root_ always points to something valid or null_, even inside
    // DestroySubtree.
    AXNode* old_root = root_;
    root_ = node;
    if (old_root && old_root != node) {
      // Example of when occurs: the contents of an iframe are replaced.
      DestroySubtree(old_root, update_state);
    }
  }

  return success;
}

void AXTree::NotifySubtreeWillBeReparentedOrDeleted(
    AXNode* node,
    const AXTreeUpdateState* update_state) {
  DCHECK(!GetTreeUpdateInProgressState());
  if (node->id() == kInvalidAXNodeID)
    return;

  bool notify_reparented = update_state->IsReparentedNode(node);
  bool notify_removed = !notify_reparented;
  // Don't fire redundant remove notification in the case where the parent
  // will become ignored at the same time.
  if (notify_removed && node->parent() &&
      base::Contains(update_state->ignored_state_changed_ids,
                     node->parent()->id()) &&
      !node->parent()->IsIgnored()) {
    notify_removed = false;
  }

  for (AXTreeObserver& observer : observers_) {
    if (notify_reparented)
      observer.OnSubtreeWillBeReparented(this, node);
    if (notify_removed)
      observer.OnSubtreeWillBeDeleted(this, node);
  }
}

void AXTree::NotifyNodeWillBeReparentedOrDeleted(
    AXNode& node,
    const AXTreeUpdateState& update_state) {
  DCHECK(!GetTreeUpdateInProgressState());

  AXNodeID id = node.id();
  if (id == kInvalidAXNodeID)
    return;

  table_info_map_.erase(id);

  bool notify_reparented = update_state.IsReparentedNode(&node);

  for (AXTreeObserver& observer : observers_) {
    if (notify_reparented) {
      observer.OnNodeWillBeReparented(this, &node);
    } else {
      observer.OnNodeWillBeDeleted(this, &node);
    }
  }

  DCHECK(table_info_map_.find(id) == table_info_map_.end())
      << "Table info should never be recreated during node deletion";
}

void AXTree::RecursivelyNotifyNodeWillBeDeletedForTreeTeardown(
    AXNode& node,
    std::set<AXNodeID>& deleted_nodes) {
  // TODO(crbug.com/366332767): Migrate tree observers to listen for tree
  // teardown by observing AXTreeManager.
  DCHECK(!GetTreeUpdateInProgressState());
  if (node.id() == kInvalidAXNodeID) {
    return;
  }

  deleted_nodes.insert(node.id());

  observers_.Notify(&AXTreeObserver::OnNodeWillBeDeleted, this, &node);
  for (ui::AXNode* child : node.children()) {
    RecursivelyNotifyNodeWillBeDeletedForTreeTeardown(CHECK_DEREF(child),
                                                      deleted_nodes);
  }
}

void AXTree::NotifyNodeHasBeenDeleted(AXNodeID node_id) {
  DCHECK(!GetTreeUpdateInProgressState());

  if (node_id == kInvalidAXNodeID)
    return;

  observers_.Notify(&AXTreeObserver::OnNodeDeleted, this, node_id);
}

void AXTree::NotifyNodeHasBeenReparentedOrCreated(
    AXNode* node,
    const AXTreeUpdateState* update_state) {
  DCHECK(!GetTreeUpdateInProgressState());
  if (node->id() == kInvalidAXNodeID)
    return;

  bool is_reparented = update_state->IsReparentedNode(node);

  if (is_reparented) {
    observers_.Notify(&AXTreeObserver::OnNodeReparented, this, node);
  } else {
    observers_.Notify(&AXTreeObserver::OnNodeCreated, this, node);
  }
}

void AXTree::NotifyChildTreeConnectionChanged(AXNode* node,
                                              AXTree* child_tree) {
  DCHECK(node->tree() == this);
  observers_.Notify(&AXTreeObserver::OnChildTreeConnectionChanged, node);
}

void AXTree::NotifyNodeAttributesWillChange(
    AXNode* node,
    AXTreeUpdateState& update_state,
    const AXTreeData* optional_old_tree_data,
    const AXNodeData& old_data,
    const AXTreeData* optional_new_tree_data,
    const AXNodeData& new_data) {
  DCHECK(!GetTreeUpdateInProgressState());
  if (new_data.id == kInvalidAXNodeID)
    return;

  observers_.Notify(&AXTreeObserver::OnNodeDataWillChange, this, old_data,
                    new_data);
}

void AXTree::NotifyNodeAttributesHaveBeenChanged(
    AXNode* node,
    AXTreeUpdateState& update_state,
    const AXTreeData* optional_old_tree_data,
    const AXNodeData& old_data,
    const AXTreeData* optional_new_tree_data,
    const AXNodeData& new_data) {
  DCHECK(!GetTreeUpdateInProgressState());
  DCHECK(node);
  DCHECK(node->id() != kInvalidAXNodeID);

  // Do not fire generated events for initial empty document:
  // The initial empty document and changes to it are uninteresting. It is a
  // bit of a hack that may not need to exist in the future
  // TODO(accessibility) Find a way to remove the initial empty document and the
  // need for this special case.
  if (node->GetRole() == ax::mojom::Role::kRootWebArea &&
      old_data.child_ids.empty() && !node->GetParentCrossingTreeBoundary()) {
    return;
  }

  observers_.Notify(&AXTreeObserver::OnNodeDataChanged, this, old_data,
                    new_data);

  if (base::Contains(update_state.ignored_state_changed_ids, new_data.id)) {
    observers_.Notify(&AXTreeObserver::OnIgnoredChanged, this, node,
                      node->IsIgnored());
  }

  // For performance reasons, it is better to skip processing and firing of
  // events related to property changes for ignored nodes.
  if (old_data.IsIgnored() || new_data.IsIgnored()) {
    return;
  }

  if (old_data.role != new_data.role) {
    observers_.Notify(&AXTreeObserver::OnRoleChanged, this, node, old_data.role,
                      new_data.role);
  }

  if (old_data.state != new_data.state) {
    for (int32_t i = static_cast<int32_t>(ax::mojom::State::kNone) + 1;
         i <= static_cast<int32_t>(ax::mojom::State::kMaxValue); ++i) {
      ax::mojom::State state = static_cast<ax::mojom::State>(i);
      // The ignored state has been already handled via `OnIgnoredChanged`.
      if (state == ax::mojom::State::kIgnored)
        continue;

      if (old_data.HasState(state) != new_data.HasState(state)) {
        observers_.Notify(&AXTreeObserver::OnStateChanged, this, node, state,
                          new_data.HasState(state));
      }
    }
  }

  auto string_callback = [this, node](ax::mojom::StringAttribute attr,
                                      const std::string& old_string,
                                      const std::string& new_string) {
    DCHECK_NE(old_string, new_string);
    observers_.Notify(&AXTreeObserver::OnStringAttributeChanged, this, node,
                      attr, old_string, new_string);
  };
  CallIfAttributeValuesChanged(old_data.string_attributes,
                               new_data.string_attributes, std::string(),
                               string_callback);

  auto bool_callback = [this, node](ax::mojom::BoolAttribute attr,
                                    const bool& old_bool,
                                    const bool& new_bool) {
    DCHECK_NE(old_bool, new_bool);
    observers_.Notify(&AXTreeObserver::OnBoolAttributeChanged, this, node, attr,
                      new_bool);
  };
  CallIfAttributeValuesChanged(old_data.bool_attributes,
                               new_data.bool_attributes, false, bool_callback);

  auto float_callback = [this, node](ax::mojom::FloatAttribute attr,
                                     const float& old_float,
                                     const float& new_float) {
    DCHECK_NE(old_float, new_float);
    observers_.Notify(&AXTreeObserver::OnFloatAttributeChanged, this, node,
                      attr, old_float, new_float);
  };
  CallIfAttributeValuesChanged(old_data.float_attributes,
                               new_data.float_attributes, 0.0f, float_callback);

  auto int_callback = [this, node](ax::mojom::IntAttribute attr,
                                   const int& old_int, const int& new_int) {
    DCHECK_NE(old_int, new_int);
    observers_.Notify(&AXTreeObserver::OnIntAttributeChanged, this, node, attr,
                      old_int, new_int);
  };
  CallIfAttributeValuesChanged(old_data.int_attributes, new_data.int_attributes,
                               0, int_callback);

  auto intlist_callback = [this, node](
                              ax::mojom::IntListAttribute attr,
                              const std::vector<int32_t>& old_intlist,
                              const std::vector<int32_t>& new_intlist) {
    observers_.Notify(&AXTreeObserver::OnIntListAttributeChanged, this, node,
                      attr, old_intlist, new_intlist);
  };
  CallIfAttributeValuesChanged(old_data.intlist_attributes,
                               new_data.intlist_attributes,
                               std::vector<int32_t>(), intlist_callback);

  auto stringlist_callback =
      [this, node](ax::mojom::StringListAttribute attr,
                   const std::vector<std::string>& old_stringlist,
                   const std::vector<std::string>& new_stringlist) {
        observers_.Notify(&AXTreeObserver::OnStringListAttributeChanged, this,
                          node, attr, old_stringlist, new_stringlist);
      };
  CallIfAttributeValuesChanged(old_data.stringlist_attributes,
                               new_data.stringlist_attributes,
                               std::vector<std::string>(), stringlist_callback);
}

void AXTree::UpdateReverseRelations(AXNode* node,
                                    const AXNodeData& new_data,
                                    bool is_new_node) {
  DCHECK(GetTreeUpdateInProgressState());
  const AXNodeData& old_data = node->data();
  // This is the id of the source node, which does not change between the old
  // and the new data.
  int id = node->id();

  for (const auto& attr : kReverseRelationIntAttributes) {
    int32_t old_relation_target_id = old_data.GetIntAttribute(attr);
    int32_t new_relation_target_id = new_data.GetIntAttribute(attr);
    if (is_new_node || old_relation_target_id != new_relation_target_id) {
      auto& map = int_reverse_relations_[attr];
      if (!is_new_node) {
        // Remove stale values from map.
        if (map.find(old_relation_target_id) != map.end()) {
          map[old_relation_target_id].erase(id);
          if (map[old_relation_target_id].empty()) {
            map.erase(old_relation_target_id);
          }
        }
      }
      int_reverse_relations_[attr][new_relation_target_id].insert(id);
    }
  }

  for (const auto& attr : kReverseRelationIntListAttributes) {
    const std::vector<int32_t>& old_idlist = old_data.GetIntListAttribute(attr);
    const std::vector<int32_t>& new_idlist = new_data.GetIntListAttribute(attr);
    if (is_new_node || old_idlist != new_idlist) {
      auto& map = intlist_reverse_relations_[attr];
      if (!is_new_node) {
        // Remove stale values from map.
        for (AXNodeID old_relation_target_id : old_idlist) {
          if (map.find(old_relation_target_id) != map.end()) {
            map[old_relation_target_id].erase(id);
            if (map[old_relation_target_id].empty()) {
              map.erase(old_relation_target_id);
            }
          }
        }
      }
      for (AXNodeID new_relation_target_id : new_idlist) {
        map[new_relation_target_id].insert(id);
      }
    }
  }

  // Update child tree id reverse map.
  std::optional<AXTreeID> old_tree_id = old_data.GetChildTreeID();
  std::optional<AXTreeID> new_tree_id = new_data.GetChildTreeID();
  if (old_tree_id == new_tree_id) {
    return;
  }

  if (old_tree_id) {
    child_tree_id_reverse_map_[*old_tree_id].erase(id);
  }
  if (new_tree_id) {
    child_tree_id_reverse_map_[*new_tree_id].insert(id);
  }
}

bool AXTree::ValidatePendingChangesComplete(
    const AXTreeUpdateState& update_state) {
  if (!update_state.pending_node_ids.empty()) {
    ACCESSIBILITY_TREE_UNSERIALIZE_ERROR_HISTOGRAM(
        AXTreeUnserializeError::kPendingNodes);
    std::string error = "Nodes left pending by the update:";
    for (const AXNodeID pending_id : update_state.pending_node_ids)
      error += base::StringPrintf(" %d", pending_id);
    RecordError(update_state, error);
    return false;
  }

  if (!update_state.node_id_to_pending_data.empty()) {
    std::string destroy_subtree_ids;
    std::string destroy_node_ids;
    std::string create_node_ids;

    bool has_pending_changes = false;
    for (auto&& pair : update_state.node_id_to_pending_data) {
      const AXNodeID pending_id = pair.first;
      const std::unique_ptr<PendingStructureChanges>& data = pair.second;
      if (data->DoesNodeExpectAnyStructureChanges()) {
        if (data->DoesNodeExpectSubtreeWillBeDestroyed())
          destroy_subtree_ids += base::StringPrintf(" %d", pending_id);
        if (data->DoesNodeExpectNodeWillBeDestroyed())
          destroy_node_ids += base::StringPrintf(" %d", pending_id);
        if (data->DoesNodeExpectNodeWillBeCreated())
          create_node_ids += base::StringPrintf(" %d", pending_id);
        has_pending_changes = true;
      }
    }
    if (has_pending_changes) {
      ACCESSIBILITY_TREE_UNSERIALIZE_ERROR_HISTOGRAM(
          AXTreeUnserializeError::kPendingChanges);
      RecordError(
          update_state,
          base::StringPrintf(
              "Changes left pending by the update; "
              "destroy subtrees: %s, destroy nodes: %s, create nodes: %s",
              destroy_subtree_ids.c_str(), destroy_node_ids.c_str(),
              create_node_ids.c_str()));
    }
    return !has_pending_changes;
  }

  return true;
}

void AXTree::MarkSubtreeForDestruction(AXNodeID node_id,
                                       AXTreeUpdateState* update_state) {
  update_state->IncrementPendingDestroySubtreeCount(node_id);
  MarkNodesForDestructionRecursive(node_id, update_state);
}

void AXTree::MarkNodesForDestructionRecursive(AXNodeID node_id,
                                              AXTreeUpdateState* update_state) {
  // If this subtree has already been marked for destruction, return so
  // we don't walk it again.
  if (!update_state->ShouldPendingNodeExistInTree(node_id))
    return;

  const AXNodeData& last_known_data =
      update_state->GetLastKnownPendingNodeData(node_id);

  update_state->IncrementPendingDestroyNodeCount(node_id);
  for (AXNodeID child_id : last_known_data.child_ids) {
    MarkNodesForDestructionRecursive(child_id, update_state);
  }
}

void AXTree::DestroySubtree(AXNode* node,
                            AXTreeUpdateState* update_state) {
  DCHECK(GetTreeUpdateInProgressState());
  // |update_state| must already contain information about all of the expected
  // changes and invalidations to apply. If any of these are missing, observers
  // may not be notified of changes.
  DCHECK(update_state);
  DCHECK_GT(update_state->GetPendingDestroySubtreeCount(node->id()), 0);
  DCHECK(!node->parent() ||
         update_state->InvalidatesUnignoredCachedValues(node->parent()->id()));
  update_state->DecrementPendingDestroySubtreeCount(node->id());
  DestroyNodeAndSubtree(node, update_state);
}

void AXTree::DestroyNodeAndSubtree(AXNode* node,
                                   AXTreeUpdateState* update_state) {
  AXNodeID id = node->id();

  DCHECK(GetTreeUpdateInProgressState());
  DCHECK(!update_state || update_state->GetPendingDestroyNodeCount(id) > 0);

  // Clear out any reverse relations.
  static base::NoDestructor<AXNodeData> empty_data;
  UpdateReverseRelations(node, *empty_data);

  auto iter = id_map_.find(id);
  CHECK(iter != id_map_.end(), base::NotFatalUntil::M130);
  std::unique_ptr<AXNode> node_to_delete = std::move(iter->second);
  id_map_.erase(iter);
  node = nullptr;

  if (!update_state) {
    // `update_state` will only be nullptr when destroying the entire tree. This
    // is then our last chance to notify that the nodes were deleted.
    observers_.Notify(&AXTreeObserver::OnNodeDeleted, this, id);
  }

  for (ui::AXNode* child : node_to_delete->children()) {
    DestroyNodeAndSubtree(child, update_state);
  }
  if (update_state) {
    update_state->pending_node_ids.erase(id);
    update_state->DecrementPendingDestroyNodeCount(id);
    update_state->new_node_ids.erase(id);
    update_state->node_data_changed_ids.erase(id);
    if (update_state->IsReparentedNode(node_to_delete.get())) {
      update_state->old_node_id_to_data.insert(
          std::make_pair(id, node_to_delete->TakeData()));
    }
  }
}

void AXTree::DeleteOldChildren(AXNode* node,
                               const std::vector<AXNodeID>& new_child_ids,
                               AXTreeUpdateState* update_state) {
  DCHECK(GetTreeUpdateInProgressState());
  // Create a set of child ids in |src| for fast lookup, we know the set does
  // not contain duplicate entries already, because that was handled when
  // populating |update_state| with information about all of the expected
  // changes to be applied.
  std::set<AXNodeID> new_child_id_set(new_child_ids.begin(),
                                      new_child_ids.end());

  // Delete the old children.
  for (AXNode* child : node->children()) {
    if (!base::Contains(new_child_id_set, child->id()))
      DestroySubtree(child, update_state);
  }
}

bool AXTree::CreateNewChildVector(
    AXNode* node,
    const std::vector<AXNodeID>& new_child_ids,
    std::vector<raw_ptr<AXNode, VectorExperimental>>* new_children,
    AXTreeUpdateState* update_state) {
  DCHECK(GetTreeUpdateInProgressState());
  bool success = true;
  for (size_t i = 0; i < new_child_ids.size(); ++i) {
    AXNodeID child_id = new_child_ids[i];
    AXNode* child = GetFromId(child_id);
    if (child) {
      if (child->parent() != node) {
        // This is a serious error - nodes should never be reparented.
        // If this case occurs, continue so this node isn't left in an
        // inconsistent state, but return failure at the end.
        if (child->parent()) {
          RecordError(*update_state,
                      base::StringPrintf("Node %d reparented from %d to %d",
                                         child->id(), child->parent()->id(),
                                         node->id()));
        } else {
          // --- Begin temporary change ---
          // TODO(crbug.com/1156601, crbug.com/1402673) Revert this once we have
          // the crash data we need (crrev.com/c/2892259) Diagnose strange
          // errors:
          // Node did not have a previous parent, but reparenting error
          // triggered:
          // * New parent = id=3 rootWebArea FOCUSABLE
          // * Child = id=1 rootWebArea (0, 0)-(0, 0) busy=true
          std::ostringstream error;
          error << "Node did not have a previous parent, but "
                   "reparenting error triggered:"
                << "\n* root_will_be_created = "
                << update_state->root_will_be_created
                << "\n* pending_root_id = "
                << (update_state->pending_root_id
                        ? *update_state->pending_root_id
                        : kInvalidAXNodeID)
                << "\n* new parent = " << *node << "\n* Old parent = "
                << (child->parent()
                        ? child->parent()->data().ToString(/*verbose*/ false)
                        : "-")
                << "\n* child = " << *child;
          RecordError(*update_state, error.str(), /* fatal */ true);
          // --- End temporary change ---
        }
        success = false;
        continue;
      }
      child->SetIndexInParent(i);
    } else {
      child = CreateNode(node, child_id, i, update_state);
      update_state->pending_node_ids.insert(child->id());
    }
    new_children->push_back(child);
  }

  return success;
}

AXNode* AXTree::GetUnignoredAncestorFromId(AXNodeID node_id) const {
  AXNode* node = GetFromId(node_id);
  // We can't simply call `AXNode::GetUnignoredParent()` because the node's
  // unignored cached values may be out-of-date.
  while (node && node->IsIgnored())
    node = node->parent();
  return node;
}

AXNodeID AXTree::GetNextNegativeInternalNodeId() {
  AXNodeID return_value = next_negative_internal_node_id_;
  next_negative_internal_node_id_--;
  if (next_negative_internal_node_id_ > 0)
    next_negative_internal_node_id_ = -1;
  return return_value;
}

void AXTree::PopulateOrderedSetItemsMap(
    const AXNode& original_node,
    const AXNode* ordered_set,
    OrderedSetItemsMap* items_map_to_be_populated) const {
  // Ignored nodes are not a part of ordered sets.
  if (original_node.IsIgnored())
    return;

  // Not all ordered set containers support hierarchical level, but their set
  // items may support hierarchical level. For example, container <tree> does
  // not support level, but <treeitem> supports level. For ordered sets like
  // this, the set container (e.g. <tree>) will take on the min of the levels
  // of its direct children(e.g. <treeitem>), if the children's levels are
  // defined.
  std::optional<int> ordered_set_min_level =
      ordered_set->GetHierarchicalLevel();

  for (AXNode::UnignoredChildIterator child =
           ordered_set->UnignoredChildrenBegin();
       child != ordered_set->UnignoredChildrenEnd(); ++child) {
    std::optional<int> child_level = child->GetHierarchicalLevel();
    if (child_level) {
      ordered_set_min_level = ordered_set_min_level
                                  ? std::min(child_level, ordered_set_min_level)
                                  : child_level;
    }
  }

  RecursivelyPopulateOrderedSetItemsMap(original_node, ordered_set, ordered_set,
                                        ordered_set_min_level, std::nullopt,
                                        items_map_to_be_populated);

  // If after RecursivelyPopulateOrderedSetItemsMap() call, the corresponding
  // level (i.e. |ordered_set_min_level|) does not exist in
  // |items_map_to_be_populated|, and |original_node| equals |ordered_set|, we
  // know |original_node| is an empty ordered set and contains no set items.
  // However, |original_node| may still have set size attribute, so we still
  // want to add this empty set (i.e. original_node/ordered_set) to
  // |items_map_to_be_populated|.
  if (&original_node == ordered_set &&
      !items_map_to_be_populated->HierarchicalLevelExists(
          ordered_set_min_level)) {
    items_map_to_be_populated->Add(ordered_set_min_level,
                                   OrderedSetContent(&original_node));
  }
}

void AXTree::RecursivelyPopulateOrderedSetItemsMap(
    const AXNode& original_node,
    const AXNode* ordered_set,
    const AXNode* local_parent,
    std::optional<int> ordered_set_min_level,
    std::optional<int> prev_level,
    OrderedSetItemsMap* items_map_to_be_populated) const {
  // For optimization purpose, we want to only populate set items that are
  // direct descendants of |ordered_set|, since we will only be calculating
  // PosInSet & SetSize of items of that level. So we skip items on deeper
  // levels by stop searching recursively on node |local_parent| that turns out
  // to be an ordered set whose role matches that of |ordered_set|. However,
  // when we encounter a flattened structure such as the following:
  // <div role="tree">
  //   <div role="treeitem" aria-level="1"></div>
  //   <div role="treeitem" aria-level="2"></div>
  //   <div role="treeitem" aria-level="3"></div>
  // </div>
  // This optimization won't apply, we will end up populating items from all
  // levels.
  if (ordered_set->GetRole() == local_parent->GetRole() &&
      ordered_set != local_parent)
    return;

  for (AXNode::UnignoredChildIterator itr =
           local_parent->UnignoredChildrenBegin();
       itr != local_parent->UnignoredChildrenEnd(); ++itr) {
    const AXNode* child = itr.get();

    // Invisible children should not be counted.
    // However, in the collapsed container case (e.g. a combobox), items can
    // still be chosen/navigated. However, the options in these collapsed
    // containers are historically marked invisible. Therefore, in that case,
    // count the invisible items. Only check 3 levels up, as combobox containers
    // are never higher.
    if (child->data().IsInvisible() && !IsCollapsed(local_parent) &&
        !IsCollapsed(local_parent->parent()) &&
        (!local_parent->parent() ||
         !IsCollapsed(local_parent->parent()->parent()))) {
      continue;
    }

    std::optional<int> curr_level = child->GetHierarchicalLevel();

    // Add child to |items_map_to_be_populated| if role matches with the role of
    // |ordered_set|. If role of node is kRadioButton, don't add items of other
    // roles, even if item role matches the role of |ordered_set|.
    if (child->GetRole() == ax::mojom::Role::kComment ||
        (original_node.GetRole() == ax::mojom::Role::kRadioButton &&
         child->GetRole() == ax::mojom::Role::kRadioButton) ||
        (original_node.GetRole() != ax::mojom::Role::kRadioButton &&
         child->SetRoleMatchesItemRole(ordered_set))) {
      // According to WAI-ARIA spec, some ordered set items do not support
      // hierarchical level while its ordered set container does. For example,
      // <tab> does not support level, while <tablist> supports level.
      // https://www.w3.org/WAI/PF/aria/roles#tab
      // https://www.w3.org/WAI/PF/aria/roles#tablist
      // For this special case, when we add set items (e.g. tab) to
      // |items_map_to_be_populated|, set item is placed at the same level as
      // its container (e.g. tablist) in |items_map_to_be_populated|.
      if (!curr_level && child->GetUnignoredParent() == ordered_set)
        curr_level = ordered_set_min_level;

      // We only add child to |items_map_to_be_populated| if the child set item
      // is at the same hierarchical level as |ordered_set|'s level.
      if (!items_map_to_be_populated->HierarchicalLevelExists(curr_level)) {
        bool use_ordered_set = child->SetRoleMatchesItemRole(ordered_set) &&
                               ordered_set_min_level == curr_level;
        const AXNode* child_ordered_set =
            use_ordered_set ? ordered_set : nullptr;
        items_map_to_be_populated->Add(curr_level,
                                       OrderedSetContent(child_ordered_set));
      }

      items_map_to_be_populated->AddItemToBack(curr_level, child);
    }

    // If |child| is an ignored container for ordered set and should not be used
    // to contribute to |items_map_to_be_populated|, we recurse into |child|'s
    // descendants to populate |items_map_to_be_populated|.
    if (child->IsIgnoredContainerForOrderedSet()) {
      RecursivelyPopulateOrderedSetItemsMap(original_node, ordered_set, child,
                                            ordered_set_min_level, curr_level,
                                            items_map_to_be_populated);
    }

    // If |curr_level| goes up one level from |prev_level|, which indicates
    // the ordered set of |prev_level| is closed, we add a new OrderedSetContent
    // on the previous level of |items_map_to_be_populated| to signify this.
    // Consider the example below:
    // <div role="tree">
    //   <div role="treeitem" aria-level="1"></div>
    //   <!--- set1-level2 -->
    //   <div role="treeitem" aria-level="2"></div>
    //   <div role="treeitem" aria-level="2"></div>  <--|prev_level|
    //   <div role="treeitem" aria-level="1" id="item2-level1">  <--|curr_level|
    //   </div>
    //   <!--- set2-level2 -->
    //   <div role="treeitem" aria-level="2"></div>
    //   <div role="treeitem" aria-level="2"></div>
    // </div>
    // |prev_level| is on the last item of "set1-level2" and |curr_level| is on
    // "item2-level1". Since |curr_level| is up one level from |prev_level|, we
    // already completed adding all items from "set1-level2" to
    // |items_map_to_be_populated|. So we close up "set1-level2" by adding a new
    // OrderedSetContent to level 2. When |curr_level| ends up on the items of
    // "set2-level2" next, it has a fresh new set to be populated.
    if (child->SetRoleMatchesItemRole(ordered_set) && curr_level < prev_level)
      items_map_to_be_populated->Add(prev_level, OrderedSetContent());

    prev_level = curr_level;
  }
}

// Given an ordered_set, compute pos_in_set and set_size for all of its items
// and store values in cache.
// Ordered_set should never be nullptr.
void AXTree::ComputeSetSizePosInSetAndCache(const AXNode& node,
                                            const AXNode* ordered_set) {
  DCHECK(ordered_set);

  // Set items role::kComment and role::kDisclosureTriangleGrouped and
  // role::kRadioButton are special cases and do not necessarily need to be
  // contained in an ordered set.
  if (node.GetRole() != ax::mojom::Role::kComment &&
      node.GetRole() != ax::mojom::Role::kDisclosureTriangle &&
      node.GetRole() != ax::mojom::Role::kDisclosureTriangleGrouped &&
      node.GetRole() != ax::mojom::Role::kRadioButton &&
      !node.SetRoleMatchesItemRole(ordered_set) && !node.IsOrderedSet()) {
    return;
  }

  // Find all items within ordered_set and add to |items_map_to_be_populated|.
  OrderedSetItemsMap items_map_to_be_populated;
  PopulateOrderedSetItemsMap(node, ordered_set, &items_map_to_be_populated);

  // If ordered_set role is kComboBoxSelect and it wraps a kMenuListPopUp, then
  // we would like it to inherit the SetSize from the kMenuListPopUp it wraps.
  // To do this, we treat the kMenuListPopUp as the ordered_set and eventually
  // assign its SetSize value to the kComboBoxSelect.
  if (node.GetRole() == ax::mojom::Role::kComboBoxSelect &&
      node.GetUnignoredChildCount() > 0) {
    // kPopUpButtons are only allowed to contain one kMenuListPopUp.
    // The single element is guaranteed to be a kMenuListPopUp because that is
    // the only item role that matches the ordered set role of kPopUpButton.
    // Please see AXNode::SetRoleMatchesItemRole for more details.
    OrderedSetContent* set_content =
        items_map_to_be_populated.GetFirstOrderedSetContent();
    if (set_content && set_content->set_items_.size() == 1) {
      const AXNode* menu_list_popup = set_content->set_items_.front();
      if (menu_list_popup->GetRole() == ax::mojom::Role::kMenuListPopup) {
        items_map_to_be_populated.Clear();
        PopulateOrderedSetItemsMap(node, menu_list_popup,
                                   &items_map_to_be_populated);
        set_content = items_map_to_be_populated.GetFirstOrderedSetContent();
        // Replace |set_content|'s ordered set container with |node|
        // (Role::kPopUpButton), which acts as the set container for nodes with
        // Role::kMenuListOptions (children of |menu_list_popup|).
        if (set_content)
          set_content->ordered_set_ = &node;
      }
    }
  }

  // Iterate over all items from OrderedSetItemsMap to compute and cache each
  // ordered set item's PosInSet and SetSize and corresponding ordered set
  // container's SetSize.
  for (auto element : items_map_to_be_populated.items_map_) {
    for (const OrderedSetContent& ordered_set_content : element.second) {
      ComputeSetSizePosInSetAndCacheHelper(ordered_set_content);
    }
  }
}

void AXTree::ComputeSetSizePosInSetAndCacheHelper(
    const OrderedSetContent& ordered_set_content) {
  // Keep track of number of items in the set.
  int32_t num_elements = 0;
  // Keep track of largest ordered set item's |aria-setsize| attribute value.
  int32_t max_item_set_size_from_attribute = 0;

  for (const AXNode* item : ordered_set_content.set_items_) {
    // |item|'s PosInSet value is the maximum of accumulated number of
    // elements count and the value from its |aria-posinset| attribute.
    int32_t pos_in_set_value =
        std::max(num_elements + 1,
                 item->GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));

    // For |item| that has defined hierarchical level and |aria-posinset|
    // attribute, the attribute value takes precedence.
    // Note: According to WAI-ARIA spec, items that support
    // |aria-posinset| do not necessarily support hierarchical level.
    if (item->GetHierarchicalLevel() &&
        item->HasIntAttribute(ax::mojom::IntAttribute::kPosInSet))
      pos_in_set_value =
          item->GetIntAttribute(ax::mojom::IntAttribute::kPosInSet);

    num_elements = pos_in_set_value;

    // Cache computed PosInSet value for |item|.
    node_set_size_pos_in_set_info_map_[item->id()] = NodeSetSizePosInSetInfo();
    node_set_size_pos_in_set_info_map_[item->id()].pos_in_set =
        pos_in_set_value;

    // Track the largest set size for this OrderedSetContent.
    max_item_set_size_from_attribute =
        std::max(max_item_set_size_from_attribute,
                 item->GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  }  // End of iterating over each item in |ordered_set_content|.

  // The SetSize of an ordered set (and all of its items) is the maximum of
  // the following values:
  // 1. The number of elements in the ordered set.
  // 2. The largest item set size from |aria-setsize| attribute.
  // 3. The ordered set container's |aria-setsize| attribute value.
  int32_t set_size_value =
      std::max(num_elements, max_item_set_size_from_attribute);

  // Cache the hierarchical level and set size of |ordered_set_content|'s set
  // container, if the container exists.
  if (const AXNode* ordered_set = ordered_set_content.ordered_set_) {
    set_size_value = std::max(
        set_size_value,
        ordered_set->GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

    // Cache |ordered_set|'s hierarchical level.
    std::optional<int> ordered_set_level = ordered_set->GetHierarchicalLevel();
    if (node_set_size_pos_in_set_info_map_.find(ordered_set->id()) ==
        node_set_size_pos_in_set_info_map_.end()) {
      node_set_size_pos_in_set_info_map_[ordered_set->id()] =
          NodeSetSizePosInSetInfo();
      node_set_size_pos_in_set_info_map_[ordered_set->id()]
          .lowest_hierarchical_level = ordered_set_level;
    } else if (node_set_size_pos_in_set_info_map_[ordered_set->id()]
                   .lowest_hierarchical_level > ordered_set_level) {
      node_set_size_pos_in_set_info_map_[ordered_set->id()]
          .lowest_hierarchical_level = ordered_set_level;
    }
    // Cache |ordered_set|'s set size.
    node_set_size_pos_in_set_info_map_[ordered_set->id()].set_size =
        set_size_value;
  }

  // Cache the set size of |ordered_set_content|'s set items.
  for (const AXNode* item : ordered_set_content.set_items_) {
    // If item's hierarchical level and |aria-setsize| attribute are specified,
    // the item's |aria-setsize| value takes precedence.
    if (item->GetHierarchicalLevel() &&
        item->HasIntAttribute(ax::mojom::IntAttribute::kSetSize))
      node_set_size_pos_in_set_info_map_[item->id()].set_size =
          item->GetIntAttribute(ax::mojom::IntAttribute::kSetSize);
    else
      node_set_size_pos_in_set_info_map_[item->id()].set_size = set_size_value;
  }  // End of iterating over each item in |ordered_set_content|.
}

std::optional<int> AXTree::GetPosInSet(const AXNode& node) {
  if (node.IsIgnored()) {
    return std::nullopt;
  }

  if ((node.GetRole() == ax::mojom::Role::kComboBoxSelect ||
       node.GetRole() == ax::mojom::Role::kPopUpButton) &&
      node.GetUnignoredChildCount() == 0 &&
      node.HasIntAttribute(ax::mojom::IntAttribute::kPosInSet)) {
    return node.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet);
  }

  if (node_set_size_pos_in_set_info_map_.find(node.id()) !=
      node_set_size_pos_in_set_info_map_.end()) {
    // If item's id is in the cache, return stored PosInSet value.
    return node_set_size_pos_in_set_info_map_[node.id()].pos_in_set;
  }

  if (GetTreeUpdateInProgressState())
    return std::nullopt;

  // Only allow this to be called on nodes that can hold PosInSet values,
  // which are defined in the ARIA spec.
  if (!node.IsOrderedSetItem()) {
    return std::nullopt;
  }

  const AXNode* ordered_set = node.GetOrderedSet();
  if (!ordered_set)
    return std::nullopt;

  ComputeSetSizePosInSetAndCache(node, ordered_set);
  std::optional<int> pos_in_set =
      node_set_size_pos_in_set_info_map_[node.id()].pos_in_set;
  if (pos_in_set.has_value() && pos_in_set.value() < 1)
    return std::nullopt;

  return pos_in_set;
}

std::optional<int> AXTree::GetSetSize(const AXNode& node) {
  if (node.IsIgnored()) {
    return std::nullopt;
  };

  if ((node.GetRole() == ax::mojom::Role::kComboBoxSelect ||
       node.GetRole() == ax::mojom::Role::kPopUpButton) &&
      node.GetUnignoredChildCount() == 0 &&
      node.HasIntAttribute(ax::mojom::IntAttribute::kSetSize)) {
    return node.GetIntAttribute(ax::mojom::IntAttribute::kSetSize);
  }

  if (node_set_size_pos_in_set_info_map_.find(node.id()) !=
      node_set_size_pos_in_set_info_map_.end()) {
    // If item's id is in the cache, return stored SetSize value.
    return node_set_size_pos_in_set_info_map_[node.id()].set_size;
  }

  if (GetTreeUpdateInProgressState())
    return std::nullopt;

  // Only allow this to be called on nodes that can hold SetSize values, which
  // are defined in the ARIA spec. However, we allow set-like items to receive
  // SetSize values for internal purposes.
  if ((!node.IsOrderedSetItem() && !node.IsOrderedSet()) ||
      node.IsEmbeddedGroup()) {
    return std::nullopt;
  }

  // If |node| is an ordered set item-like, find its outerlying ordered set.
  // Otherwise, |node| is the ordered set.
  const AXNode* ordered_set = &node;
  if (node.IsOrderedSetItem())
    ordered_set = node.GetOrderedSet();

  if (!ordered_set)
    return std::nullopt;

  // For popup buttons that control a single element, inherit the controlled
  // item's SetSize. Skip this block if the popup button controls itself.
  if (node.GetRole() == ax::mojom::Role::kPopUpButton ||
      node.GetRole() == ax::mojom::Role::kComboBoxSelect) {
    const auto& controls_ids =
        node.GetIntListAttribute(ax::mojom::IntListAttribute::kControlsIds);
    if (controls_ids.size() == 1 && GetFromId(controls_ids[0]) &&
        controls_ids[0] != node.id()) {
      const AXNode& controlled_item = *GetFromId(controls_ids[0]);

      std::optional<int> controlled_item_set_size = GetSetSize(controlled_item);
      node_set_size_pos_in_set_info_map_[node.id()].set_size =
          controlled_item_set_size;
      return controlled_item_set_size;
    }
  }

  // Compute, cache, then return.
  ComputeSetSizePosInSetAndCache(node, ordered_set);
  std::optional<int> set_size =
      node_set_size_pos_in_set_info_map_[node.id()].set_size;
  if (set_size.has_value() && set_size.value() < 0)
    return std::nullopt;

  return set_size;
}

AXSelection AXTree::GetSelection() const {
  // TODO(accessibility): do not create a selection object every time it's
  // requested. Either switch AXSelection to getters that computes selection
  // data upon request or provide an invalidation mechanism.
  return AXSelection(*this);
}

AXSelection AXTree::GetUnignoredSelection() const {
  return GetSelection().ToUnignoredSelection();
}

bool AXTree::GetTreeUpdateInProgressState() const {
  return tree_update_in_progress_;
}

void AXTree::SetTreeUpdateInProgressState(bool set_tree_update_value) {
  tree_update_in_progress_ = set_tree_update_value;
}

bool AXTree::HasPaginationSupport() const {
  return has_pagination_support_;
}

void AXTree::NotifyTreeManagerWillBeRemoved(AXTreeID previous_tree_id) {
  if (previous_tree_id == AXTreeIDUnknown())
    return;

  observers_.Notify(&AXTreeObserver::OnTreeManagerWillBeRemoved,
                    previous_tree_id);
}

void AXTree::RecordError(const AXTreeUpdateState& update_state,
                         std::string new_error,
                         bool is_fatal) {
  // Aggregate error with previous errors.
  if (!error_.empty())
    error_ = error_ + "\n";  // Add visual separation between errors.
  error_ = error_ + new_error;

#if defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
  // Suppress fatal error logging in builds that target fuzzing, as fuzzers
  // generate invalid trees by design to shake out bugs.
  is_fatal = false;
#elif defined(AX_FAIL_FAST_BUILD)
  // In fast-failing-builds, crash immediately with a full message, otherwise
  // rely on UnrecoverableAccessibilityError(), which will not crash until
  // multiple errors occur.
  // TODO(accessibility) Make AXTree errors fatal in Canary and Dev builds, as
  // they indicate fundamental problems in part of the engine. They are much
  // less frequent than in the past -- it should not be highimpact on users.
  is_fatal = true;
#endif

  std::ostringstream verbose_error;
  verbose_error << new_error << "\n** Pending tree update **\n"
                << update_state.pending_tree_update->ToString(
                       /*verbose*/ false)
                << "** Root **\n"
                << root() << "\n** AXTreeData **\n"
                << data_.ToString() + "\n** AXTree **\n"
                << TreeToStringHelper(root_, 0, false).substr(0, 1000);

  LOG_IF(FATAL, is_fatal) << verbose_error.str();

  // If this is the first error, will dump without crashing in
  // RenderFrameHostImpl::AccessibilityFatalError().
  static auto* const ax_tree_error_key = base::debug::AllocateCrashKeyString(
      "ax_tree_error", base::debug::CrashKeySize::Size256);
  static auto* const ax_tree_update_key = base::debug::AllocateCrashKeyString(
      "ax_tree_update", base::debug::CrashKeySize::Size256);
  static auto* const ax_tree_key = base::debug::AllocateCrashKeyString(
      "ax_tree", base::debug::CrashKeySize::Size256);
  static auto* const ax_tree_data_key = base::debug::AllocateCrashKeyString(
      "ax_tree_data", base::debug::CrashKeySize::Size256);

  // Log additional crash keys so we can debug bad tree updates.
  base::debug::SetCrashKeyString(ax_tree_error_key, new_error);
  base::debug::SetCrashKeyString(ax_tree_update_key,
                                 update_state.pending_tree_update->ToString());
  base::debug::SetCrashKeyString(ax_tree_key,
                                 TreeToStringHelper(root_, 0, false));
  base::debug::SetCrashKeyString(ax_tree_data_key, data_.ToString());
  LOG(ERROR) << verbose_error.str();
}

}  // namespace ui
