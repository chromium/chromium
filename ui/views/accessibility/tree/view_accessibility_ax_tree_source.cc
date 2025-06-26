// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/tree/view_accessibility_ax_tree_source.h"

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/accessibility/tree/widget_view_ax_cache.h"

namespace views {

ViewAccessibilityAXTreeSource::ViewAccessibilityAXTreeSource(
    ui::AXNodeID root_id,
    const ui::AXTreeID& tree_id,
    WidgetViewAXCache* cache)
    : root_id_(root_id), tree_id_(tree_id), cache_(cache) {
  CHECK_NE(root_id_, ui::kInvalidAXNodeID);
  CHECK_NE(tree_id_, ui::AXTreeIDUnknown());
}

ViewAccessibilityAXTreeSource::~ViewAccessibilityAXTreeSource() = default;

void ViewAccessibilityAXTreeSource::HandleAccessibleAction(
    const ui::AXActionData& action) {
  int id = action.target_node_id;

  // In Views, we only support setting the selection within a single node,
  // not across multiple nodes like on the web.
  if (action.action == ax::mojom::Action::kSetSelection) {
    CHECK_EQ(action.anchor_node_id, action.focus_node_id);
    id = action.anchor_node_id;
  }

  // TODO(crbug.com/40672441): Add a convenience virtual function
  // HandleAccessibleAction on ViewAccessibility once AXVirtualView, a subclass
  // of ViewAccessibility, doesn't need to extend the AXPlatformNodeDelegate
  // anymore -- there's currently a function with a conflicting name.
  if (ViewAccessibility* node = GetFromId(id)) {
    if (View* view = node->view()) {
      view->HandleAccessibleAction(action);
    } else if (AXVirtualView* virtual_view =
                   static_cast<AXVirtualView*>(node)) {
      virtual_view->HandleAccessibleAction(action);
    }
  }
}

bool ViewAccessibilityAXTreeSource::GetTreeData(
    ui::AXTreeData* tree_data) const {
  tree_data->tree_id = tree_id_;
  tree_data->loaded = true;
  tree_data->loading_progress = 1.0;
  // TODO(accessibility): Implement focus handling.
  return true;
}

ViewAccessibility* ViewAccessibilityAXTreeSource::GetRoot() const {
  return cache_->Get(root_id_);
}

ViewAccessibility* ViewAccessibilityAXTreeSource::GetFromId(int32_t id) const {
  return cache_->Get(id);
}

int32_t ViewAccessibilityAXTreeSource::GetId(ViewAccessibility* node) const {
  return node->GetUniqueId();
}

void ViewAccessibilityAXTreeSource::CacheChildrenIfNeeded(
    ViewAccessibility* node) {
  if (cache_->HasCachedChildren(node)) {
    return;
  }
  cache_->CacheChildrenIfNeeded(node);
}

size_t ViewAccessibilityAXTreeSource::GetChildCount(
    ViewAccessibility* node) const {
  return node->GetChildren().size();
}

ViewAccessibility* ViewAccessibilityAXTreeSource::ChildAt(
    ViewAccessibility* node,
    size_t index) const {
  auto children = node->GetChildren();
  if (index >= children.size()) {
    return nullptr;
  }
  return children[index];
}

void ViewAccessibilityAXTreeSource::ClearChildCache(ViewAccessibility* node) {
  cache_->RemoveFromChildCache(node);
}

ViewAccessibility* ViewAccessibilityAXTreeSource::GetParent(
    ViewAccessibility* node) const {
  if (node->GetUniqueId() == root_id_) {
    return nullptr;
  }

  return node->GetUnignoredParent();
}

bool ViewAccessibilityAXTreeSource::IsIgnored(ViewAccessibility* node) const {
  return false;
}

bool ViewAccessibilityAXTreeSource::IsEqual(ViewAccessibility* node1,
                                            ViewAccessibility* node2) const {
  // TODO(accessibility): Implement.
  return false;
}

ViewAccessibility* ViewAccessibilityAXTreeSource::GetNull() const {
  // TODO(accessibility): Implement.
  return nullptr;
}

std::string ViewAccessibilityAXTreeSource::GetDebugString(
    ViewAccessibility* node) const {
  // TODO(accessibility): Implement.
  return std::string();
}

void ViewAccessibilityAXTreeSource::SerializeNode(
    ViewAccessibility* node,
    ui::AXNodeData* out_data) const {
  // TODO(accessibility): Implement.
}

std::string ViewAccessibilityAXTreeSource::ToString(ViewAccessibility* root,
                                                    std::string prefix) {
  // TODO(accessibility): Implement.
  return std::string();
}

}  // namespace views
