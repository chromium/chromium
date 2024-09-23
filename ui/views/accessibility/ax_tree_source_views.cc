// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_tree_source_views.h"

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_virtual_view.h"

namespace views {

AXTreeSourceViews::AXTreeSourceViews(ui::AXNodeID root_id,
                                     const ui::AXTreeID& tree_id,
                                     views::AXAuraObjCache* cache)
    : root_id_(root_id), tree_id_(tree_id), cache_(cache) {
  DCHECK_NE(tree_id_, ui::AXTreeIDUnknown());
}

AXTreeSourceViews::~AXTreeSourceViews() = default;

void AXTreeSourceViews::HandleAccessibleAction(const ui::AXActionData& action) {
  int id = action.target_node_id;

  // In Views, we only support setting the selection within a single node,
  // not across multiple nodes like on the web.
  if (action.action == ax::mojom::Action::kSetSelection) {
    CHECK_EQ(action.anchor_node_id, action.focus_node_id);
    id = action.anchor_node_id;
  }

  AXAuraObjWrapper* obj = GetFromId(id);
  if (obj)
    obj->HandleAccessibleAction(action);
}

bool AXTreeSourceViews::GetTreeData(ui::AXTreeData* tree_data) const {
  tree_data->tree_id = tree_id_;
  tree_data->loaded = true;
  tree_data->loading_progress = 1.0;
  AXAuraObjWrapper* focus = cache_->GetFocus();
  if (focus)
    tree_data->focus_id = focus->GetUniqueId();
  return true;
}

AXAuraObjWrapper* AXTreeSourceViews::GetRoot() const {
  return cache_->Get(root_id_);
}

AXAuraObjWrapper* AXTreeSourceViews::GetFromId(int32_t id) const {
  AXAuraObjWrapper* root = GetRoot();
  // Root might not be in the cache.
  if (id == root->GetUniqueId())
    return root;
  AXAuraObjWrapper* wrapper = cache_->Get(id);

  // We must do a lookup in AXVirtualView as well if the main cache doesn't hold
  // this node.
  if (!wrapper && AXVirtualView::GetFromId(id)) {
    AXVirtualView* virtual_view = AXVirtualView::GetFromId(id);
    return virtual_view->GetOrCreateWrapper(cache_);
  }

  return wrapper;
}

int32_t AXTreeSourceViews::GetId(AXAuraObjWrapper* node) const {
  return node->GetUniqueId();
}

void AXTreeSourceViews::CacheChildrenIfNeeded(AXAuraObjWrapper* node) {
  if (node->cached_children_) {
    return;
  }

  node->cached_children_.emplace();

  node->GetChildren(&(*node->cached_children_));
}

size_t AXTreeSourceViews::GetChildCount(AXAuraObjWrapper* node) const {
  std::vector<raw_ptr<AXAuraObjWrapper, VectorExperimental>> children;
  node->GetChildren(&children);
  return children.size();
}

AXAuraObjWrapper* AXTreeSourceViews::ChildAt(AXAuraObjWrapper* node,
                                             size_t index) const {
  std::vector<raw_ptr<AXAuraObjWrapper, VectorExperimental>> children;
  node->GetChildren(&children);
  return children[index];
}

void AXTreeSourceViews::ClearChildCache(AXAuraObjWrapper* node) {
  node->cached_children_.reset();
}

AXAuraObjWrapper* AXTreeSourceViews::GetParent(AXAuraObjWrapper* node) const {
  AXAuraObjWrapper* root = GetRoot();
  // The root has no parent by definition.
  if (node->GetUniqueId() == root->GetUniqueId())
    return nullptr;
  AXAuraObjWrapper* parent = node->GetParent();
  // A top-level widget doesn't have a parent, so return the root.
  if (!parent)
    return root;
  return parent;
}

bool AXTreeSourceViews::IsIgnored(AXAuraObjWrapper* node) const {
  if (!node)
    return false;
  ui::AXNodeData out_data;
  node->Serialize(&out_data);
  return out_data.IsIgnored();
}

bool AXTreeSourceViews::IsEqual(AXAuraObjWrapper* node1,
                                AXAuraObjWrapper* node2) const {
  return node1 && node2 && node1->GetUniqueId() == node2->GetUniqueId();
}

AXAuraObjWrapper* AXTreeSourceViews::GetNull() const {
  return nullptr;
}

std::string AXTreeSourceViews::GetDebugString(AXAuraObjWrapper* node) const {
  return node ? node->ToString() : "(null)";
}

void AXTreeSourceViews::SerializeNode(AXAuraObjWrapper* node,
                                      ui::AXNodeData* out_data) const {
  node->Serialize(out_data);

  if (out_data->role == ax::mojom::Role::kWindow ||
      out_data->role == ax::mojom::Role::kDialog) {
    // Add clips children flag by default to these roles.
    out_data->AddBoolAttribute(ax::mojom::BoolAttribute::kClipsChildren, true);
  }

  // Converts the global coordinates reported by each AXAuraObjWrapper
  // into parent-relative coordinates to be used in the accessibility
  // tree. That way when any Window, Widget, or View moves (and fires
  // a location changed event), its descendants all move relative to
  // it by default.
  AXAuraObjWrapper* parent = node->GetParent();
  if (!parent)
    return;
  ui::AXNodeData parent_data;
  parent->Serialize(&parent_data);
  out_data->relative_bounds.bounds.Offset(
      -parent_data.relative_bounds.bounds.OffsetFromOrigin());
  out_data->relative_bounds.offset_container_id = parent->GetUniqueId();
}

std::string AXTreeSourceViews::ToString(AXAuraObjWrapper* root,
                                        std::string prefix) {
  ui::AXNodeData data;
  root->Serialize(&data);
  std::string output = prefix + data.ToString() + '\n';

  std::vector<raw_ptr<AXAuraObjWrapper, VectorExperimental>> children;
  root->GetChildren(&children);

  prefix += prefix[0];
  for (AXAuraObjWrapper* child : children)
    output += ToString(child, prefix);

  return output;
}

}  // namespace views
