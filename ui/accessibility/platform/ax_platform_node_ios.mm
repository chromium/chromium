// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/accessibility/platform/ax_platform_node_ios.h"

#import "base/strings/sys_string_conversions.h"
#import "ui/accessibility/platform/ax_platform_node_ui_kit_element.h"

namespace ui {

// static
AXPlatformNode* AXPlatformNode::Create(AXPlatformNodeDelegate* delegate) {
  AXPlatformNode* node = new AXPlatformNodeIOS();
  node->Init(delegate);
  return node;
}

// static
AXPlatformNode* AXPlatformNode::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  if ([accessible isKindOfClass:[AXPlatformNodeUIKitElement class]]) {
    return [accessible node];
  }
  return nil;
}

struct AXPlatformNodeIOS::ObjCStorage {
  AXPlatformNodeUIKitElement* __strong native_node;
};

AXPlatformNodeIOS::AXPlatformNodeIOS()
    : objc_storage_(std::make_unique<ObjCStorage>()) {}
AXPlatformNodeIOS::~AXPlatformNodeIOS() = default;

void AXPlatformNodeIOS::SetIOSDelegate(
    AXPlatformNodeIOSDelegate* ios_delegate) {
  ios_delegate_ = ios_delegate;
}

AXPlatformNodeIOSDelegate* AXPlatformNodeIOS::GetIOSDelegate() const {
  return ios_delegate_.get();
}

void AXPlatformNodeIOS::Init(AXPlatformNodeDelegate* delegate) {
  AXPlatformNodeBase::Init(delegate);
  CreateNativeWrapper();
}

void AXPlatformNodeIOS::Destroy() {
  if (objc_storage_->native_node) {
    [objc_storage_->native_node detach];
    objc_storage_->native_node = nil;
  }
  ios_delegate_ = nullptr;
  AXPlatformNodeBase::Destroy();
}

gfx::NativeViewAccessible AXPlatformNodeIOS::GetNativeViewAccessible() {
  if (!objc_storage_->native_node) {
    CreateNativeWrapper();
  }
  return objc_storage_->native_node;
}

void AXPlatformNodeIOS::CreateNativeWrapper() {
  AXPlatformNodeUIKitElement* node_ui_kit_element =
      [[AXPlatformNodeUIKitElement alloc] initWithPlatformNode:this];
  objc_storage_->native_node = node_ui_kit_element;
}

}  // namespace ui
