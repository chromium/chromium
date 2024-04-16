// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_FUCHSIA_ACCESSIBILITY_BRIDGE_FUCHSIA_REGISTRY_H_
#define UI_ACCESSIBILITY_PLATFORM_FUCHSIA_ACCESSIBILITY_BRIDGE_FUCHSIA_REGISTRY_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia.h"
#include "ui/aura/window.h"

namespace ui {

// TODO(crbug.com/40212066): Investigate using window ID instead of
// Window*.
// This class manages a mapping between aura root windows and their respective
// accessibility bridge instances. This class does NOT own the accessibility
// bridge instances themselves.
class COMPONENT_EXPORT(AX_PLATFORM) AccessibilityBridgeFuchsiaRegistry {
 public:
  // Get the global instance of this class.
  static AccessibilityBridgeFuchsiaRegistry* GetInstance();

  AccessibilityBridgeFuchsiaRegistry();
  ~AccessibilityBridgeFuchsiaRegistry();

  AccessibilityBridgeFuchsiaRegistry(
      const AccessibilityBridgeFuchsiaRegistry&) = delete;
  AccessibilityBridgeFuchsiaRegistry& operator=(
      const AccessibilityBridgeFuchsiaRegistry&) = delete;

  // Retrieve an |AccessibilityBridgeFuchsia| by aura::Window*.
  //
  // Returns nullptr if no accessibility bridge is registered for
  // |window|.
  AccessibilityBridgeFuchsia* GetAccessibilityBridge(aura::Window* window);

  // Registers the accessibility bridge for the specified window.
  // There must NOT be an accessibility bridge registered for |window| when
  // this method is called, and |window| must NOT be null.
  void RegisterAccessibilityBridge(
      aura::Window* window,
      AccessibilityBridgeFuchsia* accessibility_bridge);

  // Removes the accessibility bridge for the specified widow.
  // There must be an accessibility bridge registered for |window| when this
  // method is called.
  void UnregisterAccessibilityBridge(aura::Window* window);

 private:
  base::flat_map<aura::Window*, AccessibilityBridgeFuchsia*>
      window_to_bridge_map_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_FUCHSIA_ACCESSIBILITY_BRIDGE_FUCHSIA_REGISTRY_H_
