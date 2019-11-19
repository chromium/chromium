// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/hwnd_util.h"

#include "base/i18n/rtl.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/widget/widget.h"

namespace views {

HWND HWNDForView(const View* view) {
  return view->GetWidget() ? HWNDForWidget(view->GetWidget()) : nullptr;
}

HWND HWNDForWidget(const Widget* widget) {
  return HWNDForNativeWindow(widget->GetNativeWindow());
}

HWND HWNDForNativeView(const gfx::NativeView view) {
  return view && view->GetRootWindow() ? view->GetHost()->GetAcceleratedWidget()
                                       : nullptr;
}

HWND HWNDForNativeWindow(const gfx::NativeWindow window) {
  return window && window->GetRootWindow()
             ? window->GetHost()->GetAcceleratedWidget()
             : nullptr;
}

gfx::Rect GetWindowBoundsForClientBounds(View* view,
                                         const gfx::Rect& client_bounds) {
  DCHECK(view);
  aura::WindowTreeHost* host = view->GetWidget()->GetNativeWindow()->GetHost();
  if (host) {
    HWND hwnd = host->GetAcceleratedWidget();
    RECT rect = client_bounds.ToRECT();
    DWORD style = ::GetWindowLong(hwnd, GWL_STYLE);
    DWORD ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
    AdjustWindowRectEx(&rect, style, FALSE, ex_style);
    return gfx::Rect(rect);
  }
  return client_bounds;
}

void ShowSystemMenuAtScreenPixelLocation(HWND window, const gfx::Point& point) {
  UINT flags = TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD;
  if (base::i18n::IsRTL())
    flags |= TPM_RIGHTALIGN;
  HMENU menu = GetSystemMenu(window, FALSE);

  const int command =
      TrackPopupMenu(menu, flags, point.x(), point.y(), 0, window, nullptr);

  if (command)
    SendMessage(window, WM_SYSCOMMAND, command, 0);
}

}  // namespace views
