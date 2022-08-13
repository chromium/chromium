// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_delegate.h"

#include "base/containers/fixed_flat_set.h"

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

bool AXPlatformNodeDelegate::HasDefaultActionVerb() const {
  return GetData().GetDefaultActionVerb() !=
         ax::mojom::DefaultActionVerb::kNone;
}

std::vector<ax::mojom::Action> AXPlatformNodeDelegate::GetSupportedActions()
    const {
  static constexpr auto kActionsThatCanBeExposed =
      base::MakeFixedFlatSet<ax::mojom::Action>(
          {ax::mojom::Action::kDecrement, ax::mojom::Action::kIncrement,
           ax::mojom::Action::kScrollUp, ax::mojom::Action::kScrollDown,
           ax::mojom::Action::kScrollLeft, ax::mojom::Action::kScrollRight,
           ax::mojom::Action::kScrollForward,
           ax::mojom::Action::kScrollBackward});
  std::vector<ax::mojom::Action> supported_actions;

  // The default action, if it exists, must be listed at index 0.
  if (HasDefaultActionVerb())
    supported_actions.push_back(ax::mojom::Action::kDoDefault);

  // Users expect to be able to bring a context menu on any object via e.g.
  // right click, so we make the context menu action available to any object
  // unconditionally.
  supported_actions.push_back(ax::mojom::Action::kShowContextMenu);

  for (const auto& item : kActionsThatCanBeExposed) {
    if (HasAction(item))
      supported_actions.push_back(item);
  }

  return supported_actions;
}

}  // namespace ui
