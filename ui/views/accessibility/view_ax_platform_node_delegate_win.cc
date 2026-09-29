// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_ax_platform_node_delegate_win.h"

#include <memory>

#include "base/notimplemented.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/win/screen_win.h"
#include "ui/views/accessibility/atomic_view_ax_tree_manager.h"
#include "ui/views/accessibility/views_utilities_aura.h"
#include "ui/views/view.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"
#include "ui/views/widget/widget.h"
#include "ui/views/win/hwnd_message_handler_delegate.h"
#include "ui/views/win/hwnd_util.h"

namespace views {

namespace {

bool IsDesktopHWNDContentWidget(const Widget& widget) {
  aura::Window* window = widget.GetNativeView();
  HWND hwnd = HWNDForNativeView(window);
  return hwnd &&
         DesktopWindowTreeHostWin::GetContentWindowForHWND(hwnd) == window;
}

}  // namespace

// static
std::unique_ptr<ViewAccessibility>
ViewAXPlatformNodeDelegate::CreatePlatformSpecific(View* view) {
  auto result = std::make_unique<ViewAXPlatformNodeDelegateWin>(view);
  result->Init();
  return result;
}

ViewAXPlatformNodeDelegateWin::ViewAXPlatformNodeDelegateWin(View* view)
    : ViewAXPlatformNodeDelegate(view) {}

ViewAXPlatformNodeDelegateWin::~ViewAXPlatformNodeDelegateWin() = default;

gfx::NativeViewAccessible ViewAXPlatformNodeDelegateWin::GetParent() const {
  // If the View has a parent View, return that View's IAccessible.
  if (view()->parent()) {
    return ViewAXPlatformNodeDelegate::GetParent();
  }

  // Otherwise we must be the RootView, get the corresponding Widget
  // and Window.
  Widget* widget = view()->GetWidget();
  if (!widget) {
    return nullptr;
  }

  aura::Window* window = widget->GetNativeWindow();
  if (!window) {
    return nullptr;
  }

  // Look for an ancestor window with a Widget, and if found, return
  // the NativeViewAccessible for its RootView.
  aura::Window* ancestor_window = GetWindowParentIncludingTransient(window);
  while (ancestor_window) {
    Widget* ancestor_widget = Widget::GetWidgetForNativeView(ancestor_window);
    if (ancestor_widget && ancestor_widget->GetRootView()) {
      if (IsInHiddenWidget()) {
        return nullptr;
      }
      return ancestor_widget->GetRootView()->GetNativeViewAccessible();
    }
    ancestor_window = GetWindowParentIncludingTransient(ancestor_window);
  }

  // Return the IAccessible for this RootView's HWND.
  return HWNDNativeViewAccessibleForView(view());
}

gfx::AcceleratedWidget
ViewAXPlatformNodeDelegateWin::GetTargetForNativeAccessibilityEvent() {
  return HWNDForView(view());
}

gfx::Rect ViewAXPlatformNodeDelegateWin::GetBoundsRect(
    const ui::AXCoordinateSystem coordinate_system,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  switch (coordinate_system) {
    case ui::AXCoordinateSystem::kScreenPhysicalPixels:
      return display::win::GetScreenWin()->DIPToScreenRect(
          HWNDForView(view()), view()->GetBoundsInScreen());
    case ui::AXCoordinateSystem::kScreenDIPs:
      // We could optionally add clipping here if ever needed.
      return view()->GetBoundsInScreen();
    case ui::AXCoordinateSystem::kRootFrame:
    case ui::AXCoordinateSystem::kFrame:
      NOTIMPLEMENTED();
      return gfx::Rect();
  }
}

gfx::Rect ViewAXPlatformNodeDelegateWin::GetInnerTextRangeBoundsRect(
    const int start_offset,
    const int end_offset,
    const ui::AXCoordinateSystem coordinate_system,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  switch (coordinate_system) {
    case ui::AXCoordinateSystem::kScreenPhysicalPixels:
      return display::win::GetScreenWin()->DIPToScreenRect(
          HWNDForView(view()),
          ViewAXPlatformNodeDelegate::GetInnerTextRangeBoundsRect(
              start_offset, end_offset, ui::AXCoordinateSystem::kScreenDIPs,
              clipping_behavior, offscreen_result));
    case ui::AXCoordinateSystem::kScreenDIPs:
      return ViewAXPlatformNodeDelegate::GetInnerTextRangeBoundsRect(
          start_offset, end_offset, coordinate_system, clipping_behavior,
          offscreen_result);
    case ui::AXCoordinateSystem::kRootFrame:
    case ui::AXCoordinateSystem::kFrame:
      NOTIMPLEMENTED();
      return gfx::Rect();
  }
}

gfx::Point ViewAXPlatformNodeDelegateWin::ScreenToDIPPoint(
    const gfx::Point& screen_point) const {
  // On Windows, we can't directly divide the point in screen coordinates by the
  // display's scale factor to get the point in DIPs like we can on other
  // platforms. We need to go through the ScreenWin::ScreenToDIPPoint helper
  // function to perform the right set of offset transformations needed.
  //
  // This is because Chromium transforms the screen physical coordinates it
  // receives from Windows into an internal representation of screen physical
  // coordinates adjusted for multiple displays of different resolutions.
  return ToRoundedPoint(display::win::GetScreenWin()->ScreenToDIPPoint(
      gfx::PointF(screen_point)));
}

bool ViewAXPlatformNodeDelegateWin::ShouldIncludeChildWidget(
    const Widget& child_widget) const {
  // Preserve native children sharing another widget's HWND.
  if (!IsDesktopHWNDContentWidget(child_widget)) {
    return true;
  }

  // Preserve the existing hierarchy for desktop popups parented to native
  // child widgets. Desktop parents fall through to the accessible-parent
  // comparison below.
  if (const Widget* parent = child_widget.parent();
      parent && !IsDesktopHWNDContentWidget(*parent)) {
    return true;
  }

  // Ownership enumeration is transitive. Include this desktop widget only
  // under its host-provided accessible parent.
  HWNDMessageHandlerDelegate* const host =
      static_cast<DesktopWindowTreeHostWin*>(
          child_widget.GetNativeView()->GetHost());
  return host->GetParentNativeViewAccessible() ==
         view()->GetNativeViewAccessible();
}

}  // namespace views
