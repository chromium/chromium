// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_IOS_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_IOS_H_

#include <memory>

#include "base/component_export.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"

namespace ui {

// TODO(crbug.com/336611337): This interface exists since the long-term
// plan is to migrate functionality from //content/browser/accessibility to
// //ui/accessibility, but not all functionality needed to make this happen is
// directly available through existing //ui/accessibility classes. Once the
// functionality here is available elsewhere, this class can be removed.
class AXPlatformNodeIOSDelegate {
 public:
  virtual float GetDeviceScaleFactor() const = 0;
};

class COMPONENT_EXPORT(AX_PLATFORM) AXPlatformNodeIOS
    : public AXPlatformNodeBase {
 public:
  ~AXPlatformNodeIOS() override;
  AXPlatformNodeIOS(const AXPlatformNodeIOS&) = delete;
  AXPlatformNodeIOS& operator=(const AXPlatformNodeIOS&) = delete;

  void SetIOSDelegate(AXPlatformNodeIOSDelegate* ios_delegate);
  AXPlatformNodeIOSDelegate* GetIOSDelegate() const;

  // AXPlatformNode.
  void Init(AXPlatformNodeDelegate* delegate) override;
  void Destroy() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;

 protected:
  AXPlatformNodeIOS();

 private:
  friend AXPlatformNode* AXPlatformNode::Create(
      AXPlatformNodeDelegate* delegate);

  // Creates a new wrapper node.
  void CreateNativeWrapper();

  // An opaque pointer into the UIKit wrapper object for this node.
  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;

  // This node's iOS delegate.
  raw_ptr<AXPlatformNodeIOSDelegate> ios_delegate_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_IOS_H_
