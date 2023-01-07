// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/hidden_window.h"

#include "base/notreached.h"
#include "ui/gfx/win/window_impl.h"

namespace ui {

namespace {

// We need to have a parent window for the compositing code to work correctly.
//
// A tab will not have a parent HWND whenever it is not active in its
// host window - for example at creation time and when it's in the
// background, so we provide a default widget to host them.
//
// It may be tempting to use GetDesktopWindow() instead, but this is
// problematic as the shell sends messages to children of the desktop
// window that interact poorly with us.
//
// See: http://crbug.com/16476
class TempParent : public gfx::WindowImpl {
 public:
  static TempParent* Get() {
    static TempParent* g_temp_parent;
    if (!g_temp_parent) {
      g_temp_parent = new TempParent();

      g_temp_parent->set_window_style(WS_POPUP);
      g_temp_parent->set_window_ex_style(WS_EX_TOOLWINDOW);
      g_temp_parent->Init(GetDesktopWindow(), gfx::Rect());
      EnableWindow(g_temp_parent->hwnd(), FALSE);
    }
    return g_temp_parent;
  }

 private:
  // Explicitly do nothing in Close. We do this as some external apps may get a
  // handle to this window and attempt to close it.
  void OnClose() {
  }

  CR_BEGIN_MSG_MAP_EX(TempParent)
    CR_MSG_WM_CLOSE(OnClose)
  CR_END_MSG_MAP()

  CR_MSG_MAP_CLASS_DECLARATIONS(TempParent)
};

}  // namespace

HWND GetHiddenWindow() {
  return TempParent::Get()->hwnd();
}

}  // namespace ui
