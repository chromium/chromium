// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_live_region_tracker.h"

#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"

namespace ui {

// static
bool AXLiveRegionTracker::IsLiveRegionRoot(const AXNode& node) {
  return node.HasStringAttribute(ax::mojom::StringAttribute::kLiveStatus) &&
         node.GetStringAttribute(ax::mojom::StringAttribute::kLiveStatus) !=
             "off";
}

AXLiveRegionTracker::AXLiveRegionTracker(const AXTree& tree) : tree_(tree) {
  DCHECK(tree.root());
  const AXNode& root = *tree.root();
  WalkTreeAndAssignLiveRootsToNodes(root, nullptr);
}

AXLiveRegionTracker::~AXLiveRegionTracker() = default;

void AXLiveRegionTracker::UpdateCachedLiveRootForNode(const AXNode& node) {
  const AXNode* live_root = &node;
  while (live_root && !IsLiveRegionRoot(*live_root))
    live_root = live_root->GetUnignoredParent();

  if (live_root)
    live_region_node_to_root_id_[node.id()] = live_root->id();
}

void AXLiveRegionTracker::OnNodeWillBeDeleted(const AXNode& node) {
  if (AXNode* root = GetLiveRoot(node))
    QueueChangeEventForDeletedNode(*root);

  live_region_node_to_root_id_.erase(node.id());
  deleted_node_ids_.insert(node.id());
}

void AXLiveRegionTracker::QueueChangeEventForDeletedNode(const AXNode& root) {
  live_region_roots_with_changes_.insert(root.id());
}

void AXLiveRegionTracker::OnAtomicUpdateFinished() {
  deleted_node_ids_.clear();
  live_region_roots_with_changes_.clear();
}

AXNode* AXLiveRegionTracker::GetLiveRoot(const AXNode& node) const {
  auto iter = live_region_node_to_root_id_.find(node.id());
  if (iter == live_region_node_to_root_id_.end())
    return nullptr;

  AXNodeID id = iter->second;
  if (deleted_node_ids_.find(id) != deleted_node_ids_.end())
    return nullptr;

  return tree_->GetFromId(id);
}

AXNode* AXLiveRegionTracker::GetLiveRootIfNotBusy(const AXNode& node) const {
  AXNode* live_root = GetLiveRoot(node);
  if (!live_root)
    return nullptr;

  // Don't return the live root if the live region is busy.
  if (live_root->GetBoolAttribute(ax::mojom::BoolAttribute::kBusy))
    return nullptr;

  return live_root;
}

void AXLiveRegionTracker::WalkTreeAndAssignLiveRootsToNodes(
    const AXNode& node,
    const AXNode* current_root) {
  if (IsLiveRegionRoot(node))
    current_root = &node;
  if (current_root)
    live_region_node_to_root_id_[node.id()] = current_root->id();
  for (AXNode* child : node.GetAllChildren())
    WalkTreeAndAssignLiveRootsToNodes(*child, current_root);
}

}  // namespace ui
