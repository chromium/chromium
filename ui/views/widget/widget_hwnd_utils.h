// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_HWND_UTILS_H_
#define UI_VIEWS_WIDGET_WIDGET_HWND_UTILS_H_

#include <windows.h>

#include "ui/views/widget/widget.h"

// Functions shared by hwnd_message_handler.cc and
// desktop_window_tree_host_win.cc:

namespace views {
class HWNDMessageHandler;
class WidgetDelegate;
namespace internal {
class NativeWidgetDelegate;
}

// Returns true if the WINDOWPOS data provided indicates the client area of
// the window may have changed size. This can be caused by the window being
// resized or its frame changing.
bool DidClientAreaSizeChange(const WINDOWPOS* window_pos);

// Returns true if the size data provided indicates that the window
// transitioned from a minimized state to something else or vice versa.
bool DidMinimizedChange(UINT old_size_param, UINT new_size_param);

// Sets styles appropriate for |params| on |handler|.
void ConfigureWindowStyles(
    HWNDMessageHandler* handler,
    const Widget::InitParams& params,
    WidgetDelegate* widget_delegate,
    internal::NativeWidgetDelegate* native_widget_delegate);

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WIDGET_HWND_UTILS_H_
