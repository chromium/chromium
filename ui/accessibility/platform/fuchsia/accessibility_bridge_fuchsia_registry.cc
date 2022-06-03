// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia_registry.h"

#include "base/no_destructor.h"

namespace ui {

// static
AccessibilityBridgeFuchsiaRegistry*
AccessibilityBridgeFuchsiaRegistry::GetInstance() {
  static base::NoDestructor<AccessibilityBridgeFuchsiaRegistry> instance;
  return instance.get();
}

AccessibilityBridgeFuchsiaRegistry::AccessibilityBridgeFuchsiaRegistry() =
    default;
AccessibilityBridgeFuchsiaRegistry::~AccessibilityBridgeFuchsiaRegistry() =
    default;

AccessibilityBridgeFuchsia*
AccessibilityBridgeFuchsiaRegistry::GetAccessibilityBridge(
    AXTreeID ax_tree_id) {
  auto it = ax_tree_id_to_accessibility_bridge_map_.find(ax_tree_id);
  if (it == ax_tree_id_to_accessibility_bridge_map_.end())
    return nullptr;

  return it->second;
}

void AccessibilityBridgeFuchsiaRegistry::RegisterAccessibilityBridge(
    AXTreeID ax_tree_id,
    AccessibilityBridgeFuchsia* accessibility_bridge) {
  DCHECK(ax_tree_id_to_accessibility_bridge_map_.find(ax_tree_id) ==
         ax_tree_id_to_accessibility_bridge_map_.end());

  ax_tree_id_to_accessibility_bridge_map_[ax_tree_id] = accessibility_bridge;
}

void AccessibilityBridgeFuchsiaRegistry::UnregisterAccessibilityBridge(
    AXTreeID ax_tree_id) {
  auto it = ax_tree_id_to_accessibility_bridge_map_.find(ax_tree_id);
  DCHECK(it != ax_tree_id_to_accessibility_bridge_map_.end());

  ax_tree_id_to_accessibility_bridge_map_.erase(it);
}

}  // namespace ui
