// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_ax_platform_node_delegate_win.h"

#include <oleacc.h>

#include <memory>
#include <set>
#include <vector>

#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/layout.h"
#include "ui/base/win/atl_module.h"
#include "ui/display/win/screen_win.h"
#include "ui/views/accessibility/views_utilities_aura.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/win/hwnd_util.h"

namespace views {

// static
std::unique_ptr<ViewAccessibility> ViewAccessibility::Create(View* view) {
  auto result = std::make_unique<ViewAXPlatformNodeDelegateWin>(view);
  result->Init();
  return result;
}

ViewAXPlatformNodeDelegateWin::ViewAXPlatformNodeDelegateWin(View* view)
    : ViewAXPlatformNodeDelegate(view) {}

ViewAXPlatformNodeDelegateWin::~ViewAXPlatformNodeDelegateWin() = default;

gfx::NativeViewAccessible ViewAXPlatformNodeDelegateWin::GetParent() const {
  // If the View has a parent View, return that View's IAccessible.
  if (view()->parent())
    return ViewAXPlatformNodeDelegate::GetParent();

  // Otherwise we must be the RootView, get the corresponding Widget
  // and Window.
  Widget* widget = view()->GetWidget();
  if (!widget)
    return nullptr;

  aura::Window* window = widget->GetNativeWindow();
  if (!window)
    return nullptr;

  // Look for an ancestor window with a Widget, and if found, return
  // the NativeViewAccessible for its RootView.
  aura::Window* ancestor_window = GetWindowParentIncludingTransient(window);
  while (ancestor_window) {
    Widget* ancestor_widget = Widget::GetWidgetForNativeView(ancestor_window);
    if (ancestor_widget && ancestor_widget->GetRootView())
      return ancestor_widget->GetRootView()->GetNativeViewAccessible();
    ancestor_window = GetWindowParentIncludingTransient(ancestor_window);
  }

  // If that fails, return the NativeViewAccessible for our owning HWND.
  HWND hwnd = HWNDForView(view());
  if (!hwnd)
    return nullptr;

  IAccessible* parent;
  if (SUCCEEDED(
          ::AccessibleObjectFromWindow(hwnd, OBJID_WINDOW, IID_IAccessible,
                                       reinterpret_cast<void**>(&parent)))) {
    return parent;
  }

  return nullptr;
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
      return display::win::ScreenWin::DIPToScreenRect(
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

}  // namespace views
