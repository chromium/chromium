// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/component_export.h"
#include "base/mac/scoped_nsobject.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"

@class AXPlatformNodeCocoa;

namespace ui {

class AXPlatformNodeMac : public AXPlatformNodeBase {
 public:
  ~AXPlatformNodeMac() override;
  AXPlatformNodeMac(const AXPlatformNodeMac&) = delete;
  AXPlatformNodeMac& operator=(const AXPlatformNodeMac&) = delete;

  // AXPlatformNode.
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void NotifyAccessibilityEvent(ax::mojom::Event event_type) override;
  void AnnounceText(const std::u16string& text) override;

  // AXPlatformNodeBase.
  void Destroy() override;
  bool IsPlatformCheckable() const override;

  AXPlatformNodeCocoa* GetNativeWrapper() const { return native_node_.get(); }

  base::scoped_nsobject<AXPlatformNodeCocoa> ReleaseNativeWrapper() {
    return std::move(native_node_);
  }

  void SetNativeWrapper(AXPlatformNodeCocoa* native_node) {
    return native_node_.reset(native_node);
  }

 protected:
  AXPlatformNodeMac();

  void AddAttributeToList(const char* name,
                          const char* value,
                          PlatformAttributeList* attributes) override;

 private:
  base::scoped_nsobject<AXPlatformNodeCocoa> native_node_;

  friend AXPlatformNode* AXPlatformNode::Create(
      AXPlatformNodeDelegate* delegate);
};

// Convenience function to determine whether an internal object role should
// expose its accessible name in AXValue (as opposed to AXTitle/AXDescription).
COMPONENT_EXPORT(AX_PLATFORM)
bool IsNameExposedInAXValueForRole(ax::mojom::Role role);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_MAC_H_
