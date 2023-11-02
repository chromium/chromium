// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_ax_platform_node_delegate_mac.h"

#include <memory>

#include "ui/accessibility/platform/ax_platform_node_mac.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

// static
std::unique_ptr<ViewAccessibility> ViewAccessibility::Create(View* view) {
  auto result = std::make_unique<ViewAXPlatformNodeDelegateMac>(view);
  result->Init();
  return result;
}

ViewAXPlatformNodeDelegateMac::ViewAXPlatformNodeDelegateMac(View* view)
    : ViewAXPlatformNodeDelegate(view) {}

ViewAXPlatformNodeDelegateMac::~ViewAXPlatformNodeDelegateMac() = default;

gfx::NativeViewAccessible ViewAXPlatformNodeDelegateMac::GetNSWindow() {
  auto* widget = view()->GetWidget();
  if (!widget)
    return nil;

  auto* window_host = NativeWidgetMacNSWindowHost::GetFromNativeWindow(
      widget->GetNativeWindow());
  if (!window_host)
    return nil;

  return window_host->GetNativeViewAccessibleForNSWindow();
}

gfx::NativeViewAccessible ViewAXPlatformNodeDelegateMac::GetParent() const {
  if (view()->parent())
    return ViewAXPlatformNodeDelegate::GetParent();

  auto* widget = view()->GetWidget();
  if (!widget)
    return nil;

  auto* window_host = NativeWidgetMacNSWindowHost::GetFromNativeWindow(
      view()->GetWidget()->GetNativeWindow());
  if (!window_host)
    return nil;

  return window_host->GetNativeViewAccessibleForNSView();
}

void ViewAXPlatformNodeDelegateMac::OverrideNativeWindowTitle(
    const std::string& title) {
  if (gfx::NativeViewAccessible ax_window = GetNSWindow()) {
    [ax_window setAccessibilityLabel:base::SysUTF8ToNSString(title)];
  }
}

}  // namespace views
