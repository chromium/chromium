// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_SINGLETON_HWND_HOT_KEY_OBSERVER_H_
#define UI_GFX_WIN_SINGLETON_HWND_HOT_KEY_OBSERVER_H_

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/functional/bind.h"
#include "base/win/windows_types.h"
#include "ui/gfx/win/singleton_hwnd.h"

namespace gfx {

// We need to avoid duplicate hot key IDs for classes that call RegisterHotKey()
// on the SingletonHwnd. This class properly handles getting a unique hot key
// ID, registers the hotkey on construction, and unregisters the hot key on
// destruction.
//
// This class should always be used instead of directly registering hot keys on
// the SingletonHwnd with RegisterHotKey() in order to prevent duplicate hot key
// IDs.
class COMPONENT_EXPORT(GFX) SingletonHwndHotKeyObserver {
 public:
  using WndProc = SingletonHwnd::CallbackList::CallbackType;

  // Registers a hot key with the given |key_code| and |modifiers| and returns
  // a SingletonHwndHotKeyObserver if successful. Returns null if the hot key
  // fails to register, which can happen if another application has already
  // registered the hot key.
  static std::unique_ptr<SingletonHwndHotKeyObserver>
  Create(const WndProc& wnd_proc, UINT key_code, int modifiers);

  SingletonHwndHotKeyObserver(const SingletonHwndHotKeyObserver&) = delete;
  SingletonHwndHotKeyObserver& operator=(const SingletonHwndHotKeyObserver&) =
      delete;

  ~SingletonHwndHotKeyObserver();

 private:
  SingletonHwndHotKeyObserver(const WndProc& wnd_proc, int hot_key_id);

  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  base::CallbackListSubscription hwnd_subscription_ =
      SingletonHwnd::GetInstance()->RegisterCallback(
          base::BindRepeating(&SingletonHwndHotKeyObserver::OnWndProc,
                              base::Unretained(this)));
  WndProc wnd_proc_;
  const int hot_key_id_;
};

}  // namespace gfx

#endif  // UI_GFX_WIN_SINGLETON_HWND_HOT_KEY_OBSERVER_H_
