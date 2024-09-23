// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia_registry.h"

#include "base/no_destructor.h"
#include "base/not_fatal_until.h"

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
    aura::Window* window) {
  auto it = window_to_bridge_map_.find(window);
  if (it == window_to_bridge_map_.end())
    return nullptr;

  return it->second;
}

void AccessibilityBridgeFuchsiaRegistry::RegisterAccessibilityBridge(
    aura::Window* window,
    AccessibilityBridgeFuchsia* accessibility_bridge) {
  DCHECK(window);
  DCHECK(!window_to_bridge_map_.count(window));

  window_to_bridge_map_.emplace(window, accessibility_bridge);
}

void AccessibilityBridgeFuchsiaRegistry::UnregisterAccessibilityBridge(
    aura::Window* window) {
  auto it = window_to_bridge_map_.find(window);
  CHECK(it != window_to_bridge_map_.end(), base::NotFatalUntil::M130);

  window_to_bridge_map_.erase(it);
}

}  // namespace ui
