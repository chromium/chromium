// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_VIEW_AX_PLATFORM_NODE_DELEGATE_WIN_H_
#define UI_VIEWS_ACCESSIBILITY_VIEW_AX_PLATFORM_NODE_DELEGATE_WIN_H_

#include "ui/views/accessibility/view_ax_platform_node_delegate.h"

namespace views {

class View;

class ViewAXPlatformNodeDelegateWin : public ViewAXPlatformNodeDelegate {
 public:
  explicit ViewAXPlatformNodeDelegateWin(View* view);
  ViewAXPlatformNodeDelegateWin(const ViewAXPlatformNodeDelegateWin&) = delete;
  ViewAXPlatformNodeDelegateWin& operator=(
      const ViewAXPlatformNodeDelegateWin&) = delete;
  ~ViewAXPlatformNodeDelegateWin() override;

  // ViewAXPlatformNodeDelegate overrides.
  gfx::NativeViewAccessible GetParent() const override;
  gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() override;
  gfx::Rect GetBoundsRect(
      const ui::AXCoordinateSystem coordinate_system,
      const ui::AXClippingBehavior clipping_behavior,
      ui::AXOffscreenResult* offscreen_result) const override;
  gfx::Rect GetInnerTextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      const ui::AXCoordinateSystem coordinate_system,
      const ui::AXClippingBehavior clipping_behavior,
      ui::AXOffscreenResult* offscreen_result) const override;
  gfx::Point ScreenToDIPPoint(const gfx::Point& screen_point) const override;

  // ViewAccessibility overrides.
  void EnsureAtomicViewAXTreeManager() override;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_VIEW_AX_PLATFORM_NODE_DELEGATE_WIN_H_
