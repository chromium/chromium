// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/fuchsia/ax_platform_node_fuchsia.h"

namespace ui {

AXPlatformNodeFuchsia::AXPlatformNodeFuchsia() = default;

// static
AXPlatformNode::Pointer AXPlatformNode::Create(
    AXPlatformNodeDelegate& delegate) {
  AXPlatformNodeFuchsia* node = new AXPlatformNodeFuchsia();
  node->Init(delegate);
  return Pointer(node);
}

gfx::NativeViewAccessible AXPlatformNodeFuchsia::GetNativeViewAccessible() {
  return nullptr;
}

void AXPlatformNodeFuchsia::PerformAction(const AXActionData& data) {
  GetDelegate()->AccessibilityPerformAction(data);
}

}  // namespace ui
