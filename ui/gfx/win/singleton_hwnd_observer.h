// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_SINGLETON_HWND_OBSERVER_H_
#define UI_GFX_WIN_SINGLETON_HWND_OBSERVER_H_

#include <windows.h>

#include "base/functional/callback.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

class SingletonHwnd;

// Singleton lifetime management is tricky. This observer handles the correct
// cleanup if either the SingletonHwnd or forwarded object is destroyed first.
// Note that if you want to register a hot key on the SingletonHwnd, you need to
// use a SingletonHwndHotKeyObserver instead for each hot key.
class GFX_EXPORT SingletonHwndObserver {
 public:
  using WndProc = base::RepeatingCallback<void(HWND, UINT, WPARAM, LPARAM)>;

  explicit SingletonHwndObserver(const WndProc& wnd_proc);

  SingletonHwndObserver(const SingletonHwndObserver&) = delete;
  SingletonHwndObserver& operator=(const SingletonHwndObserver&) = delete;

  ~SingletonHwndObserver();

 private:
  friend class SingletonHwnd;

  void ClearWndProc();
  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  WndProc wnd_proc_;
};

}  // namespace gfx

#endif  // UI_GFX_WIN_SINGLETON_HWND_OBSERVER_H_
