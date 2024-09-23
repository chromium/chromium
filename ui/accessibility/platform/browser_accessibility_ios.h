// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_IOS_H_
#define UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_IOS_H_

#include "base/memory/raw_ptr.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/platform/ax_platform_node_ios.h"

namespace ui {

class BrowserAccessibilityIOS : public BrowserAccessibility,
                                public AXPlatformNodeIOSDelegate {
 public:
  ~BrowserAccessibilityIOS() override;
  BrowserAccessibilityIOS(const BrowserAccessibilityIOS&) = delete;
  BrowserAccessibilityIOS& operator=(const BrowserAccessibilityIOS&) = delete;

  // BrowserAccessibility overrides.
  void OnDataChanged() override;
  size_t PlatformChildCount() const override;
  BrowserAccessibility* PlatformGetChild(size_t child_index) const override;
  BrowserAccessibility* PlatformGetFirstChild() const override;
  BrowserAccessibility* PlatformGetLastChild() const override;
  BrowserAccessibility* PlatformGetNextSibling() const override;
  BrowserAccessibility* PlatformGetPreviousSibling() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  AXPlatformNode* GetAXPlatformNode() const override;

  // AXPlatformNodeIOSDelegate overrides.
  float GetDeviceScaleFactor() const override;

 protected:
  BrowserAccessibilityIOS(BrowserAccessibilityManager* manager, AXNode* node);

  friend class BrowserAccessibility;  // Needs access to our constructor.

 private:
  // Creates platform node if not yet created.
  void CreatePlatformNode();

  // Manager of the native wrapper node. This should be a unique_ptr but
  // currently AXPlatformNodeBase manually manages deleting itself.
  raw_ptr<AXPlatformNodeIOS> platform_node_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_IOS_H_
