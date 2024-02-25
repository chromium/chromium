// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_MAC_H_

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/component_export.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"

@class AXPlatformNodeCocoa;

namespace ui {

class COMPONENT_EXPORT(AX_PLATFORM) AXPlatformNodeMac
    : public AXPlatformNodeBase {
 public:
  ~AXPlatformNodeMac() override;
  AXPlatformNodeMac(const AXPlatformNodeMac&) = delete;
  AXPlatformNodeMac& operator=(const AXPlatformNodeMac&) = delete;

  // AXPlatformNode.
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void NotifyAccessibilityEvent(ax::mojom::Event event_type) override;
  void AnnounceTextAs(const std::u16string& text,
                      AnnouncementType announcement_type) override;

  // AXPlatformNodeBase.
  void Destroy() override;
  bool IsPlatformCheckable() const override;

  AXPlatformNodeCocoa* GetNativeWrapper() const;
  AXPlatformNodeCocoa* ReleaseNativeWrapper();
  void SetNativeWrapper(AXPlatformNodeCocoa* native_node);

 protected:
  AXPlatformNodeMac();

  void AddAttributeToList(const char* name,
                          const char* value,
                          PlatformAttributeList* attributes) override;

 private:
  friend AXPlatformNode* AXPlatformNode::Create(
      AXPlatformNodeDelegate* delegate);

  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;
};

// Convenience function to determine whether an internal object role should
// expose its accessible name in AXValue (as opposed to AXTitle/AXDescription).
COMPONENT_EXPORT(AX_PLATFORM)
bool IsNameExposedInAXValueForRole(ax::mojom::Role role);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_MAC_H_
