// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MAC_H_

#include "base/component_export.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/platform/ax_platform_node_mac.h"
#include "ui/accessibility/platform/browser_accessibility.h"

@class BrowserAccessibilityCocoa;

namespace ui {

class BrowserAccessibilityMac : public BrowserAccessibility {
 public:
  ~BrowserAccessibilityMac() override;
  BrowserAccessibilityMac(const BrowserAccessibilityMac&) = delete;
  BrowserAccessibilityMac& operator=(const BrowserAccessibilityMac&) = delete;

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

  // The BrowserAccessibilityCocoa associated with us.
  BrowserAccessibilityCocoa* GetNativeWrapper() const;

  // Refresh the native object associated with this.
  // Useful for re-announcing the current focus when properties have changed.
  void ReplaceNativeObject();

 protected:
  BrowserAccessibilityMac(BrowserAccessibilityManager* manager, AXNode* node);

  friend class BrowserAccessibility;  // Needs access to our constructor.

 private:
  // Creates platform and cocoa node if not yet created.
  void CreatePlatformNodes();

  // Creates a new cocoa node. Returns an old node in the swap_node.
  BrowserAccessibilityCocoa* CreateNativeWrapper();

  AXPlatformNodeMac* platform_node() {
    return static_cast<AXPlatformNodeMac*>(platform_node_.get());
  }

  const AXPlatformNodeMac* platform_node() const {
    return static_cast<const AXPlatformNodeMac*>(platform_node_.get());
  }

  // Manager of the native cocoa node. We own this object.
  AXPlatformNode::Pointer platform_node_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MAC_H_
