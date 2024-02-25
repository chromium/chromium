// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_selection.h"

#include "ui/accessibility/ax_node_position.h"

namespace ui {

namespace {

// Helper for GetUnignoredSelection. Creates a position using |node_id|,
// |offset| and |affinity|, and if it's ignored, updates these arguments so
// that they represent a non-null non-ignored position, according to
// |adjustment_behavior|. Returns true on success, false on failure. Note that
// if the position is initially null, it's not ignored and it's a success.
bool ComputeUnignoredSelectionEndpoint(
    const AXTree* tree,
    AXPositionAdjustmentBehavior adjustment_behavior,
    AXNodeID& node_id,
    int32_t& offset,
    ax::mojom::TextAffinity& affinity) {
  AXNode* node = tree ? tree->GetFromId(node_id) : nullptr;
  if (!node) {
    node_id = kInvalidAXNodeID;
    offset = AXNodePosition::INVALID_OFFSET;
    affinity = ax::mojom::TextAffinity::kDownstream;
    return false;
  }

  AXNodePosition::AXPositionInstance position =
      AXNodePosition::CreatePosition(*node, offset, affinity);

  // Null positions are never ignored, but must be considered successful, or
  // these Android tests would fail:
  // org.chromium.content.browser.accessibility.AssistViewStructureTest#*
  // The reason is that |position| becomes null because no AXTreeManager is
  // registered for that |tree|'s AXTreeID.
  // TODO(accessibility): investigate and fix this if needed.
  if (!position->IsIgnored())
    return true;  // We assume that unignored positions are already valid.

  position =
      position->AsValidPosition()->AsUnignoredPosition(adjustment_behavior);

  // Moving to an unignored position might have placed the position on a leaf
  // node. Any selection endpoint that is inside a leaf node is expressed as a
  // text position in AXTreeData. (Note that in this context "leaf node" means
  // a node with no children or with only ignored children. This does not
  // refer to a platform leaf.)
  if (position->IsLeafTreePosition())
    position = position->AsTextPosition();

  // We do not expect the selection to have an endpoint on an inline text
  // box as this will create issues with parts of the code that don't use
  // inline text boxes.
  if (position->IsTextPosition() &&
      position->GetRole() == ax::mojom::Role::kInlineTextBox) {
    position = position->CreateParentPosition();
  }

  switch (position->kind()) {
    case AXPositionKind::NULL_POSITION:
      node_id = kInvalidAXNodeID;
      offset = AXNodePosition::INVALID_OFFSET;
      affinity = ax::mojom::TextAffinity::kDownstream;
      return false;
    case AXPositionKind::TREE_POSITION:
      node_id = position->anchor_id();
      offset = position->child_index();
      affinity = ax::mojom::TextAffinity::kDownstream;
      return true;
    case AXPositionKind::TEXT_POSITION:
      node_id = position->anchor_id();
      offset = position->text_offset();
      affinity = position->affinity();
      return true;
  }
}

}  // namespace

AXSelection::AXSelection() = default;
AXSelection::AXSelection(const AXSelection&) = default;

AXSelection::~AXSelection() = default;

AXSelection::AXSelection(const AXTree& tree)
    : is_backward(tree.data().sel_is_backward),
      anchor_object_id(tree.data().sel_anchor_object_id),
      anchor_offset(tree.data().sel_anchor_offset),
      anchor_affinity(tree.data().sel_anchor_affinity),
      focus_object_id(tree.data().sel_focus_object_id),
      focus_offset(tree.data().sel_focus_offset),
      focus_affinity(tree.data().sel_focus_affinity),
      tree_id_(tree.GetAXTreeID()) {}

AXSelection& AXSelection::ToUnignoredSelection() {
  // If the tree is not registered with an AXTreeManager, it
  // is a initial tree with no data, do not calculate selection.
  const AXTreeManager* manager = AXTreeManager::FromID(tree_id_);
  if (!manager)
    return *this;

  // If one of the selection endpoints is invalid, then the other endpoint
  // should also be unset.
  if (!ComputeUnignoredSelectionEndpoint(
          manager->ax_tree(),
          is_backward ? AXPositionAdjustmentBehavior::kMoveForward
                      : AXPositionAdjustmentBehavior::kMoveBackward,
          anchor_object_id, anchor_offset, anchor_affinity)) {
    focus_object_id = kInvalidAXNodeID;
    focus_offset = AXNodePosition::INVALID_OFFSET;
    focus_affinity = ax::mojom::TextAffinity::kDownstream;
  } else if (!ComputeUnignoredSelectionEndpoint(
                 manager->ax_tree(),
                 is_backward ? AXPositionAdjustmentBehavior::kMoveBackward
                             : AXPositionAdjustmentBehavior::kMoveForward,
                 focus_object_id, focus_offset, focus_affinity)) {
    anchor_object_id = kInvalidAXNodeID;
    anchor_offset = AXNodePosition::INVALID_OFFSET;
    anchor_affinity = ax::mojom::TextAffinity::kDownstream;
  }
  return *this;
}

}  // namespace ui
