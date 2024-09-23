// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_UPDATE_H_
#define UI_ACCESSIBILITY_AX_TREE_UPDATE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "ui/accessibility/ax_base_export.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event_intent.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_checks.h"
#include "ui/accessibility/ax_tree_data.h"

namespace ui {

// An AXTreeUpdate is a serialized representation of an atomic change
// to an AXTree. The sender and receiver must be in sync; the update
// is only meant to bring the tree from a specific previous state into
// its next state. Trying to apply it to the wrong tree should immediately
// die with a fatal assertion.
//
// An AXTreeUpdate consists of an optional node id to clear (meaning
// that all of that node's children and their descendants are deleted),
// followed by an ordered vector of zero or more AXNodeData structures to
// be applied to the tree in order. An update may also include an optional
// update to the AXTreeData structure that applies to the tree as a whole.
//
// Suppose that the next AXNodeData to be applied is |node|. The following
// invariants must hold:
// 1. Either
//   a) |node.id| is already in the tree, or
//   b) the tree is empty, and
//      |node| is the new root of the tree, and
//      |node.role| == WebAXRoleRootWebArea.
// 2. Every child id in |node.child_ids| must either be already a child
//        of this node, or a new id not previously in the tree. It is not
//        allowed to "reparent" a child to this node without first removing
//        that child from its previous parent.
// 3. When a new id appears in |node.child_ids|, the tree should create a
//        new uninitialized placeholder node for it immediately. That
//        placeholder must be updated within the same AXTreeUpdate, otherwise
//        it's a fatal error. This guarantees the tree is always complete
//        before or after an AXTreeUpdate.
struct AX_BASE_EXPORT AXTreeUpdate {
  AXTreeUpdate();

  AXTreeUpdate(AXTreeUpdate&& other);
  AXTreeUpdate& operator=(AXTreeUpdate&& other);

  // TODO(accessibility): try to = delete these or finish auditing all sites.
  AXTreeUpdate(const AXTreeUpdate& other);
  AXTreeUpdate& operator=(const AXTreeUpdate& other);

  ~AXTreeUpdate();

  void AccumulateSize(AXNodeData::AXNodeDataSize& node_data_size) const;

  // If |has_tree_data| is true, the value of |tree_data| should be used
  // to update the tree data, otherwise it should be ignored.
  bool has_tree_data = false;
  AXTreeData tree_data;

  // The id of a node to clear, before applying any updates,
  // or 0 if no nodes should be cleared. Clearing a node means deleting
  // all of its children and their descendants, but leaving that node in
  // the tree. It's an error to clear a node but not subsequently update it
  // as part of the tree update.
  AXNodeID node_id_to_clear = kInvalidAXNodeID;

  // The id of the root of the tree, if the root is changing. This is
  // required to be set if the root of the tree is changing or Unserialize
  // will fail. If the root of the tree is not changing this is optional
  // and it is allowed to pass `kInvalidAXNodeID`.
  AXNodeID root_id = kInvalidAXNodeID;

  // A vector of nodes to update, according to the rules above.
  std::vector<AXNodeData> nodes;

  // The source of the event which generated this tree update.
  ax::mojom::EventFrom event_from = ax::mojom::EventFrom::kNone;

  // The accessibility action that caused this tree update.
  ax::mojom::Action event_from_action = ax::mojom::Action::kNone;

  // The event intents associated with this tree update.
  std::vector<AXEventIntent> event_intents;

  std::optional<AXTreeChecks> tree_checks;

  // Return a multi-line indented string representation, for logging.
  std::string ToString(bool verbose = true) const;

  // Returns the approximate size in bytes.
  size_t ByteSize() const;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_UPDATE_H_
