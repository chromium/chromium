// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MAC_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/platform/browser_accessibility.h"

@class BrowserAccessibilityCocoa;

namespace ui {

class AXPlatformNodeMac;

}  // namespace ui

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

  // Manager of the native cocoa node. We own this object.
  raw_ptr<AXPlatformNodeMac> platform_node_ = nullptr;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MAC_H_
