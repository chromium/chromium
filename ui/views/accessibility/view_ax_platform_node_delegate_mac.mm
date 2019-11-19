// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_ax_platform_node_delegate_mac.h"

#include <memory>

#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

// static
std::unique_ptr<ViewAccessibility> ViewAccessibility::Create(View* view) {
  return std::make_unique<ViewAXPlatformNodeDelegateMac>(view);
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

gfx::NativeViewAccessible ViewAXPlatformNodeDelegateMac::GetParent() {
  if (view()->parent())
    return view()->parent()->GetNativeViewAccessible();

  auto* widget = view()->GetWidget();
  if (!widget)
    return nil;

  auto* window_host = NativeWidgetMacNSWindowHost::GetFromNativeWindow(
      view()->GetWidget()->GetNativeWindow());
  if (!window_host)
    return nil;

  return window_host->GetNativeViewAccessibleForNSView();
}

}  // namespace views
