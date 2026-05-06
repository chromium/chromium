// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/hwnd_util.h"

#include <windows.h>

#include <oleacc.h>
#include <wrl/client.h>

#include <utility>

#include "base/i18n/rtl.h"
#include "base/task/current_thread.h"
#include "base/trace_event/trace_event.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/class_property.h"
#include "ui/gfx/win/msg_util.h"
#include "ui/views/widget/widget.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(Microsoft::WRL::ComPtr<IAccessible>*)

namespace views {

namespace {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(Microsoft::WRL::ComPtr<IAccessible>,
                                   kParentAccessibleKey)

}  // namespace

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
  if (base::i18n::IsRTL()) {
    flags |= TPM_RIGHTALIGN;
  }
  HMENU menu = ::GetSystemMenu(window, FALSE);

  base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
  const int command =
      ::TrackPopupMenu(menu, flags, point.x(), point.y(), 0, window, nullptr);

  if (command) {
    ::SendMessage(window, WM_SYSCOMMAND, static_cast<WPARAM>(command), 0);
  }
}

gfx::NativeViewAccessible HWNDNativeViewAccessibleForView(const View* view) {
  if (!view) {
    return nullptr;
  }
  if (const Widget* widget = view->GetWidget(); widget) {
    return HWNDNativeViewAccessibleForWidget(widget);
  }
  return nullptr;
}

gfx::NativeViewAccessible HWNDNativeViewAccessibleForWidget(
    const Widget* widget) {
  if (!widget) {
    return nullptr;
  }
  aura::Window* window = widget->GetNativeWindow();
  if (!window) {
    return nullptr;
  }
  window = window->GetRootWindow();
  if (!window) {
    return nullptr;
  }

  Microsoft::WRL::ComPtr<IAccessible>* parent_accessible =
      window->GetProperty(kParentAccessibleKey);
  if (parent_accessible) {
    // Return the held reference for this root window.
    return parent_accessible->Get();
  }

  Microsoft::WRL::ComPtr<IAccessible> new_parent;
  if (FAILED(::AccessibleObjectFromWindow(
          window->GetHost()->GetAcceleratedWidget(), OBJID_WINDOW,
          IID_PPV_ARGS(&new_parent)))) {
    return nullptr;
  }

  // Hold a reference to the parent IAccessible in the root window. It will be
  // released when the Window is destroyed, which happens when destruction of
  // the HWND leads to DesktopWindowTreeHostWin::HandleDestroyed.
  parent_accessible =
      window->SetProperty(kParentAccessibleKey, std::move(new_parent));

  return parent_accessible->Get();
}

void InflateClientSizeConstraintsInPixels(HWND hwnd,
                                          gfx::Size& min,
                                          gfx::Size& max) {
  RECT client_rect;
  RECT window_rect;
  ::GetClientRect(hwnd, &client_rect);
  ::GetWindowRect(hwnd, &window_rect);
  CR_DEFLATE_RECT(&window_rect, &client_rect);
  const gfx::Size inset = gfx::Size(window_rect.right - window_rect.left,
                                    window_rect.bottom - window_rect.top);

  min.Enlarge(inset.width(), inset.height());
  // zero means "no maximum" so enlarge width/height independently.
  if (max.width()) {
    max.Enlarge(inset.width(), 0);
  }
  if (max.height()) {
    max.Enlarge(0, inset.height());
  }
}

}  // namespace views
