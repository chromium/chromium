// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/hwnd_util.h"

#include <windows.h>

#include "base/i18n/rtl.h"
#include "base/task/current_thread.h"
#include "base/trace_event/base_tracing.h"
#include "ui/aura/client/aura_constants.h"
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
    auto style = static_cast<DWORD>(::GetWindowLong(hwnd, GWL_STYLE));
    auto ex_style = static_cast<DWORD>(::GetWindowLong(hwnd, GWL_EXSTYLE));
    ::AdjustWindowRectEx(&rect, style, FALSE, ex_style);
    return gfx::Rect(rect);
  }
  return client_bounds;
}

gfx::Rect GetHeadlessWindowBounds(HWND window) {
  gfx::Rect bounds;
  if (aura::WindowTreeHost* host =
          aura::WindowTreeHost::GetForAcceleratedWidget(window)) {
    if (gfx::Rect* headless_bounds =
            host->window()->GetProperty(aura::client::kHeadlessBoundsKey)) {
      bounds = *headless_bounds;
    }
  }

  return bounds;
}

void ShowSystemMenuAtScreenPixelLocation(HWND window, const gfx::Point& point) {
  TRACE_EVENT0("ui", "ShowSystemMenuAtScreenPixelLocation");

  UINT flags = TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD;
  if (base::i18n::IsRTL())
    flags |= TPM_RIGHTALIGN;
  HMENU menu = ::GetSystemMenu(window, FALSE);

  base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
  const int command =
      ::TrackPopupMenu(menu, flags, point.x(), point.y(), 0, window, nullptr);

  if (command)
    ::SendMessage(window, WM_SYSCOMMAND, static_cast<WPARAM>(command), 0);
}

}  // namespace views
