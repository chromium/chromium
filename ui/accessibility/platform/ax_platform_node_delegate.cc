// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

gfx::Rect AXPlatformNodeDelegate::GetClippedScreenBoundsRect(
    AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(AXCoordinateSystem::kScreenDIPs,
                       AXClippingBehavior::kClipped, offscreen_result);
}

gfx::Rect AXPlatformNodeDelegate::GetUnclippedScreenBoundsRect(
    AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(AXCoordinateSystem::kScreenDIPs,
                       AXClippingBehavior::kUnclipped, offscreen_result);
}

gfx::Rect AXPlatformNodeDelegate::GetClippedRootFrameBoundsRect(
    AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(AXCoordinateSystem::kRootFrame,
                       AXClippingBehavior::kClipped, offscreen_result);
}

gfx::Rect AXPlatformNodeDelegate::GetUnclippedRootFrameBoundsRect(
    AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(AXCoordinateSystem::kRootFrame,
                       AXClippingBehavior::kUnclipped, offscreen_result);
}

gfx::Rect AXPlatformNodeDelegate::GetClippedFrameBoundsRect(
    AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(AXCoordinateSystem::kFrame, AXClippingBehavior::kClipped,
                       offscreen_result);
}

gfx::Rect AXPlatformNodeDelegate::GetUnclippedFrameBoundsRect(
    AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(AXCoordinateSystem::kFrame,
                       AXClippingBehavior::kUnclipped, offscreen_result);
}

}  // namespace ui
