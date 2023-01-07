// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_virtual_view.h"

#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/widget/widget.h"

namespace views {

gfx::NativeViewAccessible AXVirtualView::GetNSWindow() {
  View* owner = GetOwnerView();
  if (!owner)
    return nil;

  Widget* widget = owner->GetWidget();
  if (!widget)
    return nil;

  auto* window_host = NativeWidgetMacNSWindowHost::GetFromNativeWindow(
      widget->GetNativeWindow());
  if (!window_host)
    return nil;

  return window_host->GetNativeViewAccessibleForNSWindow();
}

}  // namespace views
