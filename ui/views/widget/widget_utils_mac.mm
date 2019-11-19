// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_utils_mac.h"

#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"

namespace views {

gfx::Size GetWindowSizeForClientSize(Widget* widget, const gfx::Size& size) {
  DCHECK(widget);
  return remote_cocoa::NativeWidgetNSWindowBridge::GetWindowSizeForClientSize(
      widget->GetNativeWindow().GetNativeNSWindow(), size);
}

}  // namespace views
