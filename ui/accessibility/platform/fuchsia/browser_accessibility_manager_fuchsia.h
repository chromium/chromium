// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_FUCHSIA_BROWSER_ACCESSIBILITY_MANAGER_FUCHSIA_H_
#define UI_ACCESSIBILITY_PLATFORM_FUCHSIA_BROWSER_ACCESSIBILITY_MANAGER_FUCHSIA_H_

#include <lib/inspect/cpp/vmo/types.h>

#include "base/component_export.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia.h"

namespace ui {
class AXPlatformTreeManagerDelegate;
}

namespace ui {

class BrowserAccessibilityFuchsia;

// Manages a tree of BrowserAccessibilityFuchsia objects.
class COMPONENT_EXPORT(AX_PLATFORM) BrowserAccessibilityManagerFuchsia
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerFuchsia(const AXTreeUpdate& initial_tree,
                                     AXNodeIdDelegate& node_id_delegate,
                                     AXPlatformTreeManagerDelegate* delegate);
  ~BrowserAccessibilityManagerFuchsia() override;

  BrowserAccessibilityManagerFuchsia(
      const BrowserAccessibilityManagerFuchsia&) = delete;
  BrowserAccessibilityManagerFuchsia& operator=(
      const BrowserAccessibilityManagerFuchsia&) = delete;

  static AXTreeUpdate GetEmptyDocument();

  // AXTreeManager override.
  void FireFocusEvent(AXNode* node) override;

  // BrowserAccessibilityManager overrides.
  void FireBlinkEvent(ax::mojom::Event event_type,
                      BrowserAccessibility* node,
                      int action_request_id) override;
  void UpdateDeviceScaleFactor() override;

  // Sends hit test result to fuchsia.
  void OnHitTestResult(int action_request_id, BrowserAccessibility* node);

  // Returns the accessibility bridge instance for this manager's native window.
  AccessibilityBridgeFuchsia* GetAccessibilityBridge() const;

  // Test-only method to set the return value of GetAccessibilityBridge().
  static void SetAccessibilityBridgeForTest(
      AccessibilityBridgeFuchsia* accessibility_bridge_for_test);

 private:
  // Node to hold this object fuchsia inspect data.
  inspect::Node inspect_node_;

  // Node to output a dump of this object's AXTree.
  inspect::LazyNode tree_dump_node_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_FUCHSIA_BROWSER_ACCESSIBILITY_MANAGER_FUCHSIA_H_
