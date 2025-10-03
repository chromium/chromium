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
  if (!node) {
    return ui::kInvalidAXNodeID;
  }
  return node->GetUniqueId();
}

void ViewAccessibilityAXTreeSource::CacheChildrenIfNeeded(
    ViewAccessibility* node) {
  if (!node) {
    return;
  }
  cache_->CacheChildrenIfNeeded(node);
}

size_t ViewAccessibilityAXTreeSource::GetChildCount(
    ViewAccessibility* node) const {
  if (!node) {
    return 0;
  }
  return cache_->CachedChildCount(node);
}

ViewAccessibility* ViewAccessibilityAXTreeSource::ChildAt(
    ViewAccessibility* node,
    size_t index) const {
  if (!node) {
    return nullptr;
  }

  return cache_->CachedChildAt(node, index);
}

void ViewAccessibilityAXTreeSource::ClearChildCache(ViewAccessibility* node) {
  if (!node) {
    return;
  }
  cache_->RemoveFromChildCache(node);
}

ViewAccessibility* ViewAccessibilityAXTreeSource::GetParent(
    ViewAccessibility* node) const {
  if (!node || node->GetUniqueId() == root_id_) {
    return nullptr;
  }
  return node->GetUnignoredParent();
}

bool ViewAccessibilityAXTreeSource::IsIgnored(ViewAccessibility* node) const {
  if (!node) {
    return false;
  }
  return node->GetIsIgnored();
}

bool ViewAccessibilityAXTreeSource::IsEqual(ViewAccessibility* node1,
                                            ViewAccessibility* node2) const {
  if (!node1 || !node2) {
    return false;
  }
  return node1->GetUniqueId() == node2->GetUniqueId();
}

ViewAccessibility* ViewAccessibilityAXTreeSource::GetNull() const {
  return nullptr;
}

std::string ViewAccessibilityAXTreeSource::GetDebugString(
    ViewAccessibility* node) const {
  if (!node) {
    return "null";
  }
  return node->GetDebugString();
}

void ViewAccessibilityAXTreeSource::SerializeNode(
    ViewAccessibility* node,
    ui::AXNodeData* out_data) const {
  if (!node || !out_data) {
    return;
  }
  node->GetAccessibleNodeData(out_data);
}

std::string ViewAccessibilityAXTreeSource::ToString(ViewAccessibility* root,
                                                    std::string prefix) {
  if (!root) {
    return prefix + "null\n";
  }

  ui::AXNodeData data;
  SerializeNode(root, &data);
  std::string output = prefix + data.ToString() + '\n';

  auto children = root->GetChildren();

  prefix += prefix[0];
  for (auto child : children) {
    output += ToString(child, prefix);
  }

  return output;
}

}  // namespace views
