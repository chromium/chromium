// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_SERIALIZER_H_
#define UI_ACCESSIBILITY_AX_TREE_SERIALIZER_H_

#include <stddef.h>
#include <stdint.h>

#include <ctime>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <vector>

#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_error_types.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_source.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

struct ClientTreeNode;

// AXTreeSerializer is a helper class that serializes incremental
// updates to an AXTreeSource as a AXTreeUpdateType struct.
// These structs can be unserialized by a client object such as an
// AXTree. An AXTreeSerializer keeps track of the tree of node ids that its
// client is aware of so that it will never generate an AXTreeUpdateType that
// results in an invalid tree.
//
// Every node in the source tree must have an id that's a unique positive
// integer, the same node must not appear twice.
//
// Usage:
//
// You must call SerializeChanges() every time a node in the tree changes,
// and send the generated AXTreeUpdateType to the client. Changes to the
// AXTreeData, if any, are also automatically included in the AXTreeUpdateType.
//
// If a node is added, call SerializeChanges on its parent.
// If a node is removed, call SerializeChanges on its parent.
// If a whole new subtree is added, just call SerializeChanges on its root.
// If the root of the tree changes, call SerializeChanges on the new root.
//
// AXTreeSerializer will avoid re-serializing nodes that do not change.
// For example, if node 1 has children 2, 3, 4, 5 and then child 2 is
// removed and a new child 6 is added, the AXTreeSerializer will only
// update nodes 1 and 6 (and any children of node 6 recursively). It will
// assume that nodes 3, 4, and 5 are not modified unless you explicitly
// call SerializeChanges() on them.
//
// As long as the source tree has unique ids for every node and no loops,
// and as long as every update is applied to the client tree, AXTreeSerializer
// will continue to work. If the source tree makes a change but fails to
// call SerializeChanges properly, the trees may get out of sync - but
// because AXTreeSerializer always keeps track of what updates it's sent,
// it will never send an invalid update and the client tree will not break,
// it just may not contain all of the changes.
template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
class AXTreeSerializer {
 public:
  explicit AXTreeSerializer(
      AXTreeSource<AXSourceNode, AXTreeDataType, AXNodeDataType>* tree,
      bool crash_on_error = true);
  ~AXTreeSerializer();

  // Throw out the internal state that keeps track of the nodes the client
  // knows about. This has the effect that the next update will send the
  // entire tree over because it assumes the client knows nothing.
  void Reset();

  // Sets the maximum number of nodes that will be serialized, or zero
  // for no maximum. This is not a hard maximum - once it hits or
  // exceeds this maximum it stops walking the children of nodes, but
  // it may exceed this value a bit in order to create a consistent
  // tree.
  void set_max_node_count(size_t max_node_count) {
    max_node_count_ = max_node_count;
  }

  // Sets the maximum amount of time to be spend serializing, or zero for
  // no maximum. This is not a hard maximum - once it hits or
  // exceeds this timeout it stops walking the children of nodes, but
  // it may exceed this value a bit in order to create a consistent
  // tree. This is only intended to be used for one-time tree snapshots.
  void set_timeout(base::TimeDelta timeout) { timeout_ = timeout; }

  // Serialize all changes to |node| and append them to |out_update|.
  // Returns true on success. On failure, returns false and calls Reset();
  // this only happens when the source tree has a problem like duplicate
  // ids or changing during serialization.
  bool SerializeChanges(
      AXSourceNode node,
      AXTreeUpdateType out_update,
      std::set<AXSerializationErrorFlag>* out_error = nullptr);

  // Get incompletely serialized nodes. This will only be nonempty if either
  // set_max_node_count or set_timeout were used. This is only valid after a
  // call to SerializeChanges, and it's reset with each call.
  std::vector<AXNodeID> GetIncompleteNodeIds();

  // Invalidate the subtree rooted at this node, ensuring that the entire
  // subtree is re-serialized the next time any of those nodes end up
  // being serialized.
  void MarkSubtreeDirty(AXNodeID id);

  // Invalidate a single node, ensuring that it is reserialized.
  void MarkNodeDirty(AXNodeID id);

  // Return whether or not this node is in the client tree. If you call
  // this immediately after serializing, this indicates whether a given
  // node is in the set of nodes that the client (the recipient of
  // the AXTreeUpdates) is aware of.
  //
  // For example, you could use this to determine if a given node is
  // reachable. If one of its ancestors is hidden and it was pruned
  // from the accessibility tree, this would return false.
  bool IsInClientTree(AXSourceNode node);

  // Only for unit testing. Normally this class relies on getting a call
  // to SerializeChanges() every time the source tree changes. For unit
  // testing, it's convenient to create a static AXTree for the initial
  // state and then call ChangeTreeSourceForTesting and then SerializeChanges
  // to simulate the changes you'd get if a tree changed from the initial
  // state to the second tree's state.
  void ChangeTreeSourceForTesting(
      AXTreeSource<AXSourceNode, AXTreeDataType, AXNodeDataType>* new_tree);

  // Returns the number of nodes in the client tree. After a serialization
  // operation this should be an accurate representation of the tree source
  // as explored by the serializer.
  size_t ClientTreeNodeCount() const;

#if DCHECK_IS_ON()
  std::vector<AXNodeID> ClientTreeNodeIds() const;

  AXSourceNode ParentOf(AXNodeID id);
#endif

 private:
  // Return the least common ancestor of a node in the source tree
  // and a node in the client tree, or nullptr if there is no such node.
  // The least common ancestor is the closest ancestor to |node| (which
  // may be |node| itself) that's in both the source tree and client tree,
  // and for which both the source and client tree agree on their ancestor
  // chain up to the root.
  //
  // Example 1:
  //
  //    Client Tree    Source tree |
  //        1              1       |
  //       / \            / \      |
  //      2   3          2   4     |
  //
  // LCA(source node 2, client node 2) is node 2.
  // LCA(source node 3, client node 4) is node 1.
  //
  // Example 2:
  //
  //    Client Tree    Source tree |
  //        1              1       |
  //       / \            / \      |
  //      2   3          2   3     |
  //     / \            /   /      |
  //    4   7          8   4       |
  //   / \                / \      |
  //  5   6              5   6     |
  //
  // LCA(source node 8, client node 7) is node 2.
  // LCA(source node 5, client node 5) is node 1.
  // It's not node 5, because the two trees disagree on the parent of
  // node 4, so the LCA is the first ancestor both trees agree on.
  AXSourceNode LeastCommonAncestor(AXSourceNode node,
                                   ClientTreeNode* client_node);

  // Return the least common ancestor of |node| that's in the client tree.
  // This just walks up the ancestors of |node| until it finds a node that's
  // also in the client tree and not inside a dirty subtree, and then calls
  // LeastCommonAncestor on the source node and client node.
  AXSourceNode LeastCommonAncestor(AXSourceNode node);

  // Look for reparenting in the subtree, and, if found, update the LCA with
  // both the old and new parent, and clear its subtree.
  // Return true if there was reparenting.
  bool ComputeReparentingLCA(AXSourceNode* lca);

  // Walk the subtree rooted at |node| and return true if any nodes that
  // would be updated are being reparented. If so, update |out_lca| to point
  // to the least common ancestor of the previous LCA and the previous
  // parent of the node being reparented.
  bool AnyDescendantWasReparented(AXSourceNode node,
                                  AXSourceNode* out_lca);

  ClientTreeNode* ClientTreeNodeById(AXNodeID id);

  void CreateClientRoot(AXSourceNode root);

  // Mark as dirty the subtree rooted at this node.
  void MarkClientSubtreeDirty(ClientTreeNode* client_node);

  // Delete all descendants of this node.
  void DeleteDescendants(ClientTreeNode* client_node);

  void DeleteDescendants(AXSourceNode node);

  // Delete the client subtree rooted at this node.
  void DeleteClientSubtree(ClientTreeNode* client_node);

  // Helper function, called recursively with each new node to serialize.
  bool SerializeChangedNodes(
      AXSourceNode node,
      AXTreeUpdateType out_update,
      std::set<AXSerializationErrorFlag>* out_error = nullptr);

  ClientTreeNode* GetClientTreeNodeParent(ClientTreeNode* obj);

  // The tree source.
  raw_ptr<AXTreeSource<AXSourceNode, AXTreeDataType, AXNodeDataType>,
          DanglingUntriaged>
      tree_;

  // The tree data most recently sent to the client.
  AXTreeData client_tree_data_;

  // Our representation of the client tree.
  raw_ptr<ClientTreeNode, DanglingUntriaged> client_root_ = nullptr;

  // A map from IDs to nodes in the client tree.
  std::map<AXNodeID, ClientTreeNode*> client_id_map_;

  // The maximum number of nodes to serialize in a given call to
  // SerializeChanges, or 0 if there's no maximum.
  size_t max_node_count_ = 0;

  // The maximum time to spend serializing before timing out, or 0
  // if there's no maximum.
  base::TimeDelta timeout_;

  // The timer, which runs if there's a nonzero timeout and it hasn't
  // yet expired. Once the timeout elapses, the timer is deleted.
  std::unique_ptr<base::ElapsedTimer> timer_;

  // The IDs of nodes that weren't able to be completely serialized due to
  // max_node_count_ or timeout_.
  std::vector<AXNodeID> incomplete_node_ids_;

  // If Reset() Is called, stores the previous root of the entire tree. The
  // next serialization will set node_id_to_clear to this value ensure that the
  // previously serialized tree is also completely cleared on the
  // deserialization side. The id of tree_->GetRoot() cannot be changed because
  // the root may have changed since the previous serialization.
  AXNodeID client_root_id_to_clear_after_reset_ = kInvalidAXNodeID;

  // Whether to crash the process on serialization error or not.
  const bool crash_on_error_;
};

// In order to keep track of what nodes the client knows about, we keep a
// representation of the client tree - just IDs and parent/child
// relationships, and a marker indicating whether it's been dirtied.
struct AX_EXPORT ClientTreeNode {
  ClientTreeNode(AXNodeID id, ClientTreeNode* parent);
  virtual ~ClientTreeNode();
  bool IsDirty() { return in_dirty_subtree || is_dirty; }
  const AXNodeID id;
  const raw_ptr<ClientTreeNode, DanglingUntriaged> parent;
  // Not a vector<raw_ptr> due to regressions in blink_perf.accessibility tests.
  RAW_PTR_EXCLUSION std::vector<ClientTreeNode*> children;
  bool ignored : 1;
  // Additional nodes that must be serialized. When a dirty subtree is reached,
  // the entire subtree will be added to the current serialization.
  // For this to occur, the root of the dirty subtree must be reached in
  // SerializedChanges(), which occurs when one of its nodes or an ancestor is
  // passed in.
  // TODO(accessibility) It is an error if there any dirty nodes remaining
  // after serialization is complete, and this could be turned into a DCHECK.
  bool in_dirty_subtree : 1;

  // An individual node that is dirty, but its subtree may not be.
  bool is_dirty : 1;
};

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
AXTreeSerializer<AXSourceNode,
                 AXSourceNodeVectorType,
                 AXTreeUpdateType,
                 AXTreeDataType,
                 AXNodeDataType>::
    AXTreeSerializer(
        AXTreeSource<AXSourceNode, AXTreeDataType, AXNodeDataType>* tree,
        bool crash_on_error)
    : tree_(tree), crash_on_error_(crash_on_error) {}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
AXTreeSerializer<AXSourceNode,
                 AXSourceNodeVectorType,
                 AXTreeUpdateType,
                 AXTreeDataType,
                 AXNodeDataType>::~AXTreeSerializer() {
  // Clear |tree_| to prevent any additional calls to the tree source
  // during teardown.
  // TODO(accessibility) How would that happen?
  tree_ = nullptr;
  // Free up any resources allocated on the heap that are stored with raw_ptr.
  Reset();
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
void AXTreeSerializer<AXSourceNode,
                      AXSourceNodeVectorType,
                      AXTreeUpdateType,
                      AXTreeDataType,
                      AXNodeDataType>::Reset() {
  if (client_root_) {
    // This id must be set from the client tree root, and not from the tree
    // source root, so that if the root changes the correct subtree is still
    // cleared.
    client_root_id_to_clear_after_reset_ = client_root_->id;
  }

  client_tree_data_ = AXTreeData();

  // Normally we use DeleteClientSubtree to remove nodes from the tree,
  // but Reset() needs to work even if the tree is in a broken state.
  // Instead, iterate over |client_id_map_| to ensure we clear all nodes and
  // start from scratch.
  for (auto&& item : client_id_map_)
    delete item.second;
  client_id_map_.clear();
  client_root_ = nullptr;
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
void AXTreeSerializer<AXSourceNode,
                      AXSourceNodeVectorType,
                      AXTreeUpdateType,
                      AXTreeDataType,
                      AXNodeDataType>::
    ChangeTreeSourceForTesting(
        AXTreeSource<AXSourceNode, AXTreeDataType, AXNodeDataType>* new_tree) {
  tree_ = new_tree;
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
size_t AXTreeSerializer<AXSourceNode,
                        AXSourceNodeVectorType,
                        AXTreeUpdateType,
                        AXTreeDataType,
                        AXNodeDataType>::ClientTreeNodeCount() const {
  return client_id_map_.size();
}

#if DCHECK_IS_ON()
template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
std::vector<AXNodeID> AXTreeSerializer<AXSourceNode,
                                       AXSourceNodeVectorType,
                                       AXTreeUpdateType,
                                       AXTreeDataType,
                                       AXNodeDataType>::ClientTreeNodeIds()
    const {
  std::vector<AXNodeID> keys;
  std::transform(
      client_id_map_.begin(), client_id_map_.end(), std::back_inserter(keys),
      [](std::pair<AXNodeID, ClientTreeNode*> item) { return item.first; });
  return keys;
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
AXSourceNode AXTreeSerializer<AXSourceNode,
                              AXSourceNodeVectorType,
                              AXTreeUpdateType,
                              AXTreeDataType,
                              AXNodeDataType>::ParentOf(AXNodeID id) {
  ClientTreeNode* node = ClientTreeNodeById(id);
  if (!node || !node->parent) {
    return nullptr;
  }
  return tree_->GetFromId(node->parent->id);
}
#endif

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
AXSourceNode AXTreeSerializer<
    AXSourceNode,
    AXSourceNodeVectorType,
    AXTreeUpdateType,
    AXTreeDataType,
    AXNodeDataType>::LeastCommonAncestor(AXSourceNode node,
                                         ClientTreeNode* client_node) {
  if (!node || client_node == nullptr) {
    return tree_->GetNull();
  }

  AXSourceNodeVectorType ancestors;
  while (node) {
    ancestors.push_back(node);
    node = tree_->GetParent(node);
  }

  std::vector<ClientTreeNode*> client_ancestors;
  while (client_node) {
    client_ancestors.push_back(client_node);
    client_node = GetClientTreeNodeParent(client_node);
  }

  // Start at the root. Keep going until the source ancestor chain and
  // client ancestor chain disagree. The last node before they disagree
  // is the LCA.
  AXSourceNode lca = tree_->GetNull();
  for (size_t source_index = ancestors.size(),
              client_index = client_ancestors.size();
       source_index > 0 && client_index > 0; --source_index, --client_index) {
    if (tree_->GetId(ancestors[(unsigned int)(source_index - 1)]) !=
        client_ancestors[client_index - 1]->id) {
      // The passed-in |node| must be serialized. To ensure this, mark the
      // downward path from the new LCA to |node| as dirty. Use the source tree
      // as opposed to the client tree, because the serializer traverses that.
      for (unsigned int dirty_index = 0; dirty_index < source_index;
           ++dirty_index) {
        AXNodeID source_id = tree_->GetId(ancestors[dirty_index]);
        if (ClientTreeNode* node_mark_dirty = ClientTreeNodeById(source_id)) {
          node_mark_dirty->is_dirty = true;
        }
      }
      return lca;
    }
    lca = ancestors[(unsigned int)(source_index - 1)];
  }
  return lca;
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
AXSourceNode
AXTreeSerializer<AXSourceNode,
                 AXSourceNodeVectorType,
                 AXTreeUpdateType,
                 AXTreeDataType,
                 AXNodeDataType>::LeastCommonAncestor(AXSourceNode node) {
  // Walk up the tree until the source node's id also exists in the
  // client tree, whose parent is not dirty, then call LeastCommonAncestor
  // on those two nodes.
  //
  // Note that it's okay if |client_node| is dirty - the LCA can be the
  // root of a dirty subtree, since we're going to serialize the
  // LCA. But it's not okay if |client_node->parent| is dirty - that means
  // that we're inside of a dirty subtree that all needs to be re-serialized, so
  // the LCA should be higher.
  ClientTreeNode* client_node = ClientTreeNodeById(tree_->GetId(node));
  while (node) {
    if (client_node) {
      ClientTreeNode* parent = GetClientTreeNodeParent(client_node);
      if (!parent || !parent->in_dirty_subtree) {
        break;
      }
    }
    node = tree_->GetParent(node);
    if (node) {
      client_node = ClientTreeNodeById(tree_->GetId(node));
    }
  }
  return LeastCommonAncestor(node, client_node);
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
bool AXTreeSerializer<AXSourceNode,
                      AXSourceNodeVectorType,
                      AXTreeUpdateType,
                      AXTreeDataType,
                      AXNodeDataType>::ComputeReparentingLCA(AXSourceNode*
                                                                 lca) {
  // Look for reparenting under the lca subtree, and, if found, update lca using
  // both the old and new parent, and clear its subtree.
  // Returns true if there was reparenting, as an indication to clear the tree
  // rooted at the lca on deserialization.
  DCHECK(lca);
  bool had_reparenting = false;
  while (*lca && AnyDescendantWasReparented(*lca, /*out_lca*/ lca)) {
    // LCA has been moved up in the tree to the common ancestor with a
    // reparented descendant.
    DeleteDescendants(*lca);
    had_reparenting = true;
  }
  if (*lca) {
    return had_reparenting;
  }

  // There was no LCA: clear everything and treat it as if the LCA was the root
  // of the tree so that everything is cleared on the receiving end.
  Reset();
  return true;
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
bool AXTreeSerializer<
    AXSourceNode,
    AXSourceNodeVectorType,
    AXTreeUpdateType,
    AXTreeDataType,
    AXNodeDataType>::AnyDescendantWasReparented(AXSourceNode node,
                                                AXSourceNode* out_lca) {
  bool result = false;
  int id = tree_->GetId(node);
  tree_->CacheChildrenIfNeeded(node);
  auto num_children = tree_->GetChildCount(node);
  for (size_t i = 0; i < num_children; ++i) {
    AXSourceNode child = tree_->ChildAt(node, i);
    CHECK(child);
    int child_id = tree_->GetId(child);
    ClientTreeNode* client_child = ClientTreeNodeById(child_id);
    if (client_child) {
      ClientTreeNode* parent = client_child->parent;
      if (!parent) {
        // If the client child has no parent, it must have been the
        // previous root node, so there is no LCA and we can exit early.
        *out_lca = tree_->GetNull();
        tree_->ClearChildCache(node);
        return true;
      } else if (parent->id != id) {
        // If the client child's parent is not this node, update the LCA
        // and return true (reparenting was found).
        *out_lca = LeastCommonAncestor(*out_lca, client_child);
        result = true;
        continue;
      } else if (!client_child->IsDirty()) {
        // This child is already in the client tree and not dirty, we won't
        // recursively serialize it so we don't need to check this
        // subtree recursively for reparenting.
        // However, if the child is or was ignored, the children may now be
        // considered as reparented, so continue recursion in that case.
        if (!client_child->ignored && !tree_->IsIgnored(child))
          continue;
      }
    }

    // This is a new child or reparented child, check it recursively.
    if (AnyDescendantWasReparented(child, out_lca))
      result = true;
  }
  tree_->ClearChildCache(node);
  return result;
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
void AXTreeSerializer<AXSourceNode,
                      AXSourceNodeVectorType,
                      AXTreeUpdateType,
                      AXTreeDataType,
                      AXNodeDataType>::CreateClientRoot(AXSourceNode root) {
  // A new client root must be created for the first serialization after
  // construction or a Reset(), otherwise serialization cannot succeed.
  client_root_ = new ClientTreeNode(tree_->GetId(root), nullptr);
  client_id_map_[client_root_->id] = client_root_;
  DCHECK(!tree_->GetParent(root))
      << "A root should never have a parent, but "
         "the tree source thinks there is one:"
      << "\n* Root: " << tree_->GetDebugString(root) << "\n* Parent of root: "
      << tree_->GetDebugString(tree_->GetParent(root));
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
ClientTreeNode*
AXTreeSerializer<AXSourceNode,
                 AXSourceNodeVectorType,
                 AXTreeUpdateType,
                 AXTreeDataType,
                 AXNodeDataType>::ClientTreeNodeById(AXNodeID id) {
  std::map<AXNodeID, ClientTreeNode*>::iterator iter = client_id_map_.find(id);
  if (iter != client_id_map_.end())
    return iter->second;
  return nullptr;
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
ClientTreeNode*
AXTreeSerializer<AXSourceNode,
                 AXSourceNodeVectorType,
                 AXTreeUpdateType,
                 AXTreeDataType,
                 AXNodeDataType>::GetClientTreeNodeParent(ClientTreeNode* obj) {
  ClientTreeNode* parent = obj->parent;
  if (!parent)
    return nullptr;
  if (!ClientTreeNodeById(parent->id)) {
    std::ostringstream error;
    error << "Child: " << tree_->GetDebugString(tree_->EnsureGetFromId(obj->id))
          << "\nParent: "
          << tree_->GetDebugString(tree_->EnsureGetFromId(parent->id));
    static auto* missing_parent_err = base::debug::AllocateCrashKeyString(
        "ax_ts_missing_parent_err", base::debug::CrashKeySize::Size256);
    base::debug::SetCrashKeyString(missing_parent_err,
                                   error.str().substr(0, 230));
    if (crash_on_error_) {
      CHECK(false) << error.str();
    } else {
      LOG(ERROR) << error.str();
      // Different from other errors, not calling Reset() here to avoid breaking
      // the internal state of this class.
    }
  }
  return parent;
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
bool AXTreeSerializer<AXSourceNode,
                      AXSourceNodeVectorType,
                      AXTreeUpdateType,
                      AXTreeDataType,
                      AXNodeDataType>::
    SerializeChanges(AXSourceNode node,
                     AXTreeUpdateType out_update,
                     std::set<AXSerializationErrorFlag>* out_error) {
  if (!timeout_.is_zero())
    timer_ = std::make_unique<base::ElapsedTimer>();
  incomplete_node_ids_.clear();

  CHECK(tree_->GetId(node) != kInvalidAXNodeID);

  // If the root changed: reset the tree, clear the root, and start over.
  if (client_root_ && client_root_->id != tree_->GetId(tree_->GetRoot())) {
    Reset();
  }

  // If we had a reset, ensure that the old tree is cleared before the client
  // unserializes this update. If we didn't do this, there's a chance that
  // treating this update as an incremental update could result in some
  // reparenting.
  if (client_root_id_to_clear_after_reset_ != kInvalidAXNodeID) {
    out_update->node_id_to_clear = client_root_id_to_clear_after_reset_;
    client_root_id_to_clear_after_reset_ = 0;
  }

  // The LCA will be the least common ancestor for the serialization and used
  // as the root of the AXTreeUpdate.
  AXSourceNode lca = nullptr;

  // This computes the least common ancestor that includes the old
  // and new parents of any nodes that have been reparented, and clears the
  // whole client subtree of that LCA if necessary. If we do end up clearing
  // any client nodes, keep looping because we have to search for more
  // nodes that may have been reparented from this new LCA.
  if (client_root_) {
    // Not the first serialization: compute the LCA.
    // Although LeastCommonAncestor() is passed a single node, it actually uses
    // uses the node id to get an LCA from two sets of ancestors of the node:
    // 1) ancestors the client tree, which contains the last serialized tree
    // 2) ancestors in the source tree, which contains the current tree
    // Because of this, if `node` has been moved, the LCA will be an ancestor
    // of both its old and new positions in the tree.
    // In addition, the LCA may be moved to a higher ancestor in order to
    // prevent it from being on node with a dirty parent or parent that hasn't
    // yet been serialized. This ensures that the serialization is not missing
    // dirty or new nodes.
    lca = LeastCommonAncestor(node);
    // Look for reparenting in descendants of the subtree, and, if found, update
    // the LCA with both the old and new parents of that descendant.
    if (ComputeReparentingLCA(&lca)) {
      // Reparenting was found in the subtree. Inform the deserializer
      // to delete the subtree before receiving it, so that the final result
      // does not contain two subtrees with the same node. The LCA now contains
      // subtrees for both the old and new parent.
      out_update->node_id_to_clear = tree_->GetId(lca);
    }
  } else {
    // First serialization for this tree, after a changed root, or after a
    // Reset():use the root for the LCA and create the client root.
    lca = tree_->GetRoot();
    CreateClientRoot(lca);
  }

  CHECK(tree_->GetId(lca) != kInvalidAXNodeID);
  if (!SerializeChangedNodes(lca, out_update, out_error)) {
    return false;
  }

  // Send the tree data if it's changed since the last update, or if
  // out_update->has_tree_data is already set to true.
  // Do this last, so that selection retrieval will cause recomputation of
  // node inclusion before the the new tree structure has been updated in a
  // top-down matter via SerializeChangedNodes().
  AXTreeData new_tree_data;
  if (tree_->GetTreeData(&new_tree_data) &&
      (out_update->has_tree_data || new_tree_data != client_tree_data_)) {
    out_update->has_tree_data = true;
    out_update->tree_data = new_tree_data;
    client_tree_data_ = new_tree_data;
  }

  return true;
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
std::vector<AXNodeID> AXTreeSerializer<AXSourceNode,
                                       AXSourceNodeVectorType,
                                       AXTreeUpdateType,
                                       AXTreeDataType,
                                       AXNodeDataType>::GetIncompleteNodeIds() {
  DCHECK(max_node_count_ > 0 || !timeout_.is_zero());
  return incomplete_node_ids_;
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
void AXTreeSerializer<AXSourceNode,
                      AXSourceNodeVectorType,
                      AXTreeUpdateType,
                      AXTreeDataType,
                      AXNodeDataType>::MarkNodeDirty(AXNodeID id) {
  if (ClientTreeNode* client_node = ClientTreeNodeById(id)) {
    client_node->is_dirty = true;
  }
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
void AXTreeSerializer<AXSourceNode,
                      AXSourceNodeVectorType,
                      AXTreeUpdateType,
                      AXTreeDataType,
                      AXNodeDataType>::MarkSubtreeDirty(AXNodeID id) {
  if (ClientTreeNode* client_node = ClientTreeNodeById(id)) {
    MarkClientSubtreeDirty(client_node);
  }
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
bool AXTreeSerializer<AXSourceNode,
                      AXSourceNodeVectorType,
                      AXTreeUpdateType,
                      AXTreeDataType,
                      AXNodeDataType>::IsInClientTree(AXSourceNode node) {
  return ClientTreeNodeById(tree_->GetId(node));
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
void AXTreeSerializer<AXSourceNode,
                      AXSourceNodeVectorType,
                      AXTreeUpdateType,
                      AXTreeDataType,
                      AXNodeDataType>::MarkClientSubtreeDirty(ClientTreeNode*
                                                                  client_node) {
  // Return early if already marked dirty, in order to avoid duplicate work in
  // subtree, as the only method that marks nodes dirty is this one.
  if (client_node->in_dirty_subtree) {
    return;
  }
  client_node->in_dirty_subtree = true;
  for (ClientTreeNode* child : client_node->children) {
    MarkClientSubtreeDirty(child);
  }
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
void AXTreeSerializer<AXSourceNode,
                      AXSourceNodeVectorType,
                      AXTreeUpdateType,
                      AXTreeDataType,
                      AXNodeDataType>::DeleteClientSubtree(ClientTreeNode*
                                                               client_node) {
  if (client_node == client_root_) {
    // Do not try to reuse a bad root later.
    // A heuristic for this condition rather than an explicit Reset() from a
    // caller makes it difficult to debug whether extra resets / lost virtual
    // buffer positions are occurring because of this code. Therefore, a DCHECK
    // has been added in order to debug if or when this condition may occur.
#if defined(AX_FAIL_FAST_BUILD)
    CHECK(!crash_on_error_)
        << "Attempt to delete entire client subtree, including the root.";
#else
    DCHECK(!crash_on_error_)
        << "Attempt to delete entire client subtree, including the root.";
#endif
  } else {
    DeleteDescendants(client_node);
    client_id_map_.erase(client_node->id);
    delete client_node;
  }
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
void AXTreeSerializer<AXSourceNode,
                      AXSourceNodeVectorType,
                      AXTreeUpdateType,
                      AXTreeDataType,
                      AXNodeDataType>::DeleteDescendants(ClientTreeNode*
                                                             client_node) {
  for (size_t i = 0; i < client_node->children.size(); ++i)
    DeleteClientSubtree(client_node->children[i]);
  client_node->children.clear();
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
void AXTreeSerializer<AXSourceNode,
                      AXSourceNodeVectorType,
                      AXTreeUpdateType,
                      AXTreeDataType,
                      AXNodeDataType>::DeleteDescendants(AXSourceNode node) {
  ClientTreeNode* client_node = ClientTreeNodeById(tree_->GetId(node));
  CHECK(client_node);
  DeleteDescendants(client_node);
}

template <typename AXSourceNode,
          typename AXSourceNodeVectorType,
          typename AXTreeUpdateType,
          typename AXTreeDataType,
          typename AXNodeDataType>
bool AXTreeSerializer<AXSourceNode,
                      AXSourceNodeVectorType,
                      AXTreeUpdateType,
                      AXTreeDataType,
                      AXNodeDataType>::
    SerializeChangedNodes(AXSourceNode node,
                          AXTreeUpdateType out_update,
                          std::set<AXSerializationErrorFlag>* out_error) {
  // This method has three responsibilities:
  // 1. Serialize |node| into an AXNodeData, and append it to
  //    the AXTreeUpdateType to be sent to the client.
  // 2. Determine if |node| has any new children that the client doesn't
  //    know about yet, and call SerializeChangedNodes recursively on those.
  // 3. Update our internal data structure that keeps track of what nodes
  //    the client knows about.

  // First, find the ClientTreeNode for this id in our data structure where
  // we keep track of what accessibility objects the client already knows
  // about.
  // If we don't find it, then the intention may be to use it as the
  // new root of the accessibility tree. A heuristic for this condition rather
  // than an explicit Reset() from a caller makes it difficult to debug whether
  // extra resets / lost virtual buffer positions are occurring because of this
  // code. Therefore, a DCHECK has been added in order to debug if or when this
  // condition may occur.
  int id = tree_->GetId(node);
  ClientTreeNode* client_node = ClientTreeNodeById(id);
  DCHECK(client_node);

  // We're about to serialize it, so clear its dirty states.
  client_node->in_dirty_subtree = false;
  client_node->is_dirty = false;
  client_node->ignored = tree_->IsIgnored(node);

  // Terminate early if a maximum number of nodes is reached.
  // the output tree is still consistent).
  bool should_terminate_early = false;
  if (max_node_count_ > 0 && out_update->nodes.size() >= max_node_count_) {
    should_terminate_early = true;
    if (out_error) {
      (*out_error).insert(AXSerializationErrorFlag::kMaxNodesReached);
    }
  }

  // Also terminate early if a timeout is reached.
  if (!timeout_.is_zero()) {
    if (timer_ && timer_->Elapsed() >= timeout_) {
      // Terminate early and delete the timer so that we don't have to
      // keep checking if we timed out.
      should_terminate_early = true;
      if (out_error) {
        (*out_error).insert(AXSerializationErrorFlag::kTimeoutReached);
      }
      timer_.reset();
    } else if (!timer_) {
      // Already timed out; keep terminating early until the serialization
      // is done.
      should_terminate_early = true;
    }
  }

  // Iterate over the ids of the children of |node|.
  // Create a set of the child ids so we can quickly look
  // up which children are new and which ones were there before.
  // If we've hit the maximum number of serialized nodes, pretend
  // this node has no children but keep going so that we get
  // consistent results.
  std::set<AXNodeID> new_ignored_ids;
  std::set<AXNodeID> new_child_ids;
  size_t num_children = 0;
  if (should_terminate_early) {
    incomplete_node_ids_.push_back(id);
  } else {
    tree_->CacheChildrenIfNeeded(node);
    num_children = tree_->GetChildCount(node);
  }
  for (size_t i = 0; i < num_children; ++i) {
    AXSourceNode child = tree_->ChildAt(node, i);
    CHECK(child);
    int new_child_id = tree_->GetId(child);
    new_child_ids.insert(new_child_id);
    if (tree_->IsIgnored(child))
      new_ignored_ids.insert(new_child_id);

    // There shouldn't be any reparenting because we've already handled it
    // above. If this happens, reset and return an error.

    ClientTreeNode* client_child = ClientTreeNodeById(new_child_id);
    if (client_child && GetClientTreeNodeParent(client_child) != client_node) {
      // This condition leads to performance problems. It will
      // also reset virtual buffers, causing users to lose their place.
      std::ostringstream error;
      error << "Passed-in parent: "
            << tree_->GetDebugString(tree_->EnsureGetFromId(client_node->id))
            << "\nChild: " << tree_->GetDebugString(child)
            << "\nChild's parent: "
            << tree_->GetDebugString(
                   tree_->EnsureGetFromId(client_child->parent->id));
      static auto* reparent_err = base::debug::AllocateCrashKeyString(
          "ax_ts_reparent_err", base::debug::CrashKeySize::Size256);
      base::debug::SetCrashKeyString(reparent_err, error.str().substr(0, 230));
      if (crash_on_error_) {
        CHECK(false) << error.str();
      } else {
        LOG(ERROR) << error.str();
        Reset();
      }
      tree_->ClearChildCache(node);
      return false;
    }
  }

  // Go through the old children and delete subtrees for child
  // ids that are no longer present, and create a map from
  // id to ClientTreeNode for the rest. It's important to delete
  // first in a separate pass so that nodes that are reparented
  // don't end up children of two different parents in the middle
  // of an update, which can lead to a double-free.
  std::map<AXNodeID, ClientTreeNode*> client_child_id_map;
  std::vector<ClientTreeNode*> old_children;
  old_children.swap(client_node->children);
  for (size_t i = 0; i < old_children.size(); ++i) {
    ClientTreeNode* old_child = old_children[i];
    int old_child_id = old_child->id;
    if (new_child_ids.find(old_child_id) == new_child_ids.end()) {
      DeleteClientSubtree(old_child);
    } else {
      client_child_id_map[old_child_id] = old_child;
    }
  }

  // Serialize this node. This fills in all of the fields in
  // AXNodeData except child_ids, which we handle below.
  const size_t serialized_node_index = out_update->nodes.size();
  out_update->nodes.emplace_back();
  {
    // Take the address of an element in a vector only within a limited
    // scope because otherwise the pointer can become invalid if the
    // vector is resized.
    AXNodeData* serialized_node = &out_update->nodes.back();

    tree_->SerializeNode(node, serialized_node);
    if (serialized_node->id == client_root_->id) {
      out_update->root_id = serialized_node->id;
      CHECK(!client_root_->parent) << "The root cannot have a parent:";
      // << "\n* Root: "
      // << tree_->GetDebugString(tree_->GetFromId(out_update->root_id))
      // << "\n* Root's parent: "
      // << tree_->GetDebugString(tree_->GetFromId(client_root_->parent->id));

    } else {
      DCHECK(serialized_node->role != ax::mojom::Role::kRootWebArea)
          << "A kRootWebArea role was used on an object that is not the root: "
          << "\n* Actual root: " << tree_->GetDebugString(tree_->GetRoot())
          << "\n* Illegal node with root web area role: "
          << tree_->GetDebugString(tree_->EnsureGetFromId(serialized_node->id))
          << "\n* Parent of illegal node: "
          << (client_node->parent
                  ? tree_->GetDebugString(
                        tree_->EnsureGetFromId(client_node->parent->id))
                  : "");
    }
  }

  // Iterate over the children, serialize them, and update the ClientTreeNode
  // data structure to reflect the new tree.
  std::vector<AXNodeID> actual_serialized_node_child_ids;
  client_node->children.reserve(num_children);
  for (size_t i = 0; i < num_children; ++i) {
    AXSourceNode child = tree_->ChildAt(node, i);
    CHECK(child);
    int child_id = tree_->GetId(child);

    // Skip if the same child is included more than once.
    if (new_child_ids.find(child_id) == new_child_ids.end())
      continue;

    new_child_ids.erase(child_id);
    actual_serialized_node_child_ids.push_back(child_id);
    ClientTreeNode* reused_child = nullptr;
    if (client_child_id_map.find(child_id) != client_child_id_map.end())
      reused_child = ClientTreeNodeById(child_id);
    if (reused_child) {
      client_node->children.push_back(reused_child);
      const bool ignored_state_changed =
          reused_child->ignored !=
          (new_ignored_ids.find(reused_child->id) != new_ignored_ids.end());
      // Re-serialize it if the child is marked as dirty, otherwise
      // we don't have to because the client already has it.
      if (reused_child->IsDirty() || ignored_state_changed) {
        if (!SerializeChangedNodes(child, out_update, out_error)) {
          tree_->ClearChildCache(node);
          return false;
        }
      }
    } else {
      ClientTreeNode* new_child = new ClientTreeNode(child_id, client_node);
      new_child->ignored = tree_->IsIgnored(child);
      new_child->in_dirty_subtree = false;
      new_child->is_dirty = false;
      client_node->children.push_back(new_child);
      if (ClientTreeNodeById(child_id)) {
        // TODO(accessibility) Remove all cases where this occurs and re-add
        // This condition leads to performance problems. It will
        // also reset virtual buffers, causing users to lose their place.
        std::ostringstream error;
        error << "Child id " << child_id << " already in map."
              << "\nChild: "
              << tree_->GetDebugString(tree_->EnsureGetFromId(child_id))
              << "\nWanted for parent " << tree_->GetDebugString(node)
              << "\nAlready had parent "
              << tree_->GetDebugString(tree_->EnsureGetFromId(
                     ClientTreeNodeById(child_id)->parent->id));
        static auto* dupe_id_err = base::debug::AllocateCrashKeyString(
            "ax_ts_dupe_id_err", base::debug::CrashKeySize::Size256);
        base::debug::SetCrashKeyString(dupe_id_err, error.str().substr(0, 230));
        if (crash_on_error_) {
          CHECK(false) << error.str();
        } else {
          LOG(ERROR) << error.str();
          Reset();
        }
        tree_->ClearChildCache(node);
        return false;
      }
      client_id_map_[child_id] = new_child;
      if (!SerializeChangedNodes(child, out_update, out_error)) {
        tree_->ClearChildCache(node);
        return false;
      }
    }
  }
  tree_->ClearChildCache(node);

  // Finally, update the child ids of this node to reflect the actual child
  // ids that were valid during serialization.
  out_update->nodes[serialized_node_index].child_ids.swap(
      actual_serialized_node_child_ids);

  return true;
}

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_SERIALIZER_H_
