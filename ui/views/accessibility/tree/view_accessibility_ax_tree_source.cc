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

namespace views {

ViewAccessibilityAXTreeSource::ViewAccessibilityAXTreeSource(
    ui::AXNodeID root_id,
    const ui::AXTreeID& tree_id)
    : root_id_(root_id), tree_id_(tree_id) {
  CHECK_NE(root_id_, ui::kInvalidAXNodeID);
  CHECK_NE(tree_id_, ui::AXTreeIDUnknown());
}

ViewAccessibilityAXTreeSource::~ViewAccessibilityAXTreeSource() = default;

void ViewAccessibilityAXTreeSource::HandleAccessibleAction(
    const ui::AXActionData& action) {
  // TODO(accessibility): Implement.
}

bool ViewAccessibilityAXTreeSource::GetTreeData(
    ui::AXTreeData* tree_data) const {
  // TODO(accessibility): Implement.
  return false;
}

ViewAccessibility* ViewAccessibilityAXTreeSource::GetRoot() const {
  // TODO(accessibility): Implement.
  return nullptr;
}

ViewAccessibility* ViewAccessibilityAXTreeSource::GetFromId(int32_t id) const {
  // TODO(accessibility): Implement.
  return nullptr;
}

int32_t ViewAccessibilityAXTreeSource::GetId(ViewAccessibility* node) const {
  return node->GetUniqueId();
}

void ViewAccessibilityAXTreeSource::CacheChildrenIfNeeded(
    ViewAccessibility* node) {
  // TODO(accessibility): Implement.
}

size_t ViewAccessibilityAXTreeSource::GetChildCount(
    ViewAccessibility* node) const {
  // TODO(accessibility): Implement.
  return 0;
}

ViewAccessibility* ViewAccessibilityAXTreeSource::ChildAt(
    ViewAccessibility* node,
    size_t index) const {
  // TODO(accessibility): Implement.
  return nullptr;
}

void ViewAccessibilityAXTreeSource::ClearChildCache(ViewAccessibility* node) {
  // TODO(accessibility): Implement.
}

ViewAccessibility* ViewAccessibilityAXTreeSource::GetParent(
    ViewAccessibility* node) const {
  // TODO(accessibility): Implement.
  return nullptr;
}

bool ViewAccessibilityAXTreeSource::IsIgnored(ViewAccessibility* node) const {
  // TODO(accessibility): Implement.
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
