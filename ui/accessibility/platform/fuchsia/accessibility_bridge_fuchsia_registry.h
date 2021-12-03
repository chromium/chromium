// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_FUCHSIA_ACCESSIBILITY_BRIDGE_FUCHSIA_REGISTRY_H_
#define UI_ACCESSIBILITY_PLATFORM_FUCHSIA_ACCESSIBILITY_BRIDGE_FUCHSIA_REGISTRY_H_

#include "base/containers/flat_map.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia.h"

namespace ui {

// This class manages a mapping between AXTrees and their respective
// accessibility bridge instances. The accessibility bridge instances
// are owned by the FrameImpl for the WebContents to which the AXTrees
// belong.
class AX_EXPORT AccessibilityBridgeFuchsiaRegistry {
 public:
  // Get the global instance of this class.
  static AccessibilityBridgeFuchsiaRegistry* GetInstance();

  AccessibilityBridgeFuchsiaRegistry();
  ~AccessibilityBridgeFuchsiaRegistry();

  AccessibilityBridgeFuchsiaRegistry(
      const AccessibilityBridgeFuchsiaRegistry&) = delete;
  AccessibilityBridgeFuchsiaRegistry& operator=(
      const AccessibilityBridgeFuchsiaRegistry&) = delete;

  // Retrieve an |AccessibilityBridgeFuchsia| by AXTreeID.
  //
  // Returns nullptr if no accessibility bridge is registered for
  // |ax_tree_id|.
  AccessibilityBridgeFuchsia* GetAccessibilityBridge(AXTreeID ax_tree_id);

  // Registers the accessibility bridge for the specified AXTreeID.
  // There must NOT be an accessibility bridge registered for |ax_tree_id| when
  // this method is called.
  void RegisterAccessibilityBridge(
      AXTreeID ax_tree_id,
      AccessibilityBridgeFuchsia* accessibility_bridge);

  // Removes the accessibility bridge for the specified AXTreeID.
  // There must be an accessibility bridge registered for |ax_tree_id| when this
  // method is called.
  void UnregisterAccessibilityBridge(AXTreeID ax_tree_id);

 private:
  base::flat_map<AXTreeID, AccessibilityBridgeFuchsia*>
      ax_tree_id_to_accessibility_bridge_map_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_FUCHSIA_ACCESSIBILITY_BRIDGE_FUCHSIA_REGISTRY_H_
