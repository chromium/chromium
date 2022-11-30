// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/singleton_hwnd_observer.h"

#include "ui/gfx/win/singleton_hwnd.h"

namespace gfx {

SingletonHwndObserver::SingletonHwndObserver(const WndProc& wnd_proc)
    : wnd_proc_(wnd_proc) {
  DCHECK(!wnd_proc.is_null());
  SingletonHwnd::GetInstance()->AddObserver(this);
}

SingletonHwndObserver::~SingletonHwndObserver() {
  ClearWndProc();
}

void SingletonHwndObserver::ClearWndProc() {
  if (!wnd_proc_.is_null()) {
    SingletonHwnd::GetInstance()->RemoveObserver(this);
    wnd_proc_.Reset();
  }
}

void SingletonHwndObserver::OnWndProc(HWND hwnd,
                                      UINT message,
                                      WPARAM wparam,
                                      LPARAM lparam) {
  wnd_proc_.Run(hwnd, message, wparam, lparam);
}

}  // namespace gfx
