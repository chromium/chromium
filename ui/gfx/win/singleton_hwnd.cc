// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/singleton_hwnd.h"

#include "base/memory/singleton.h"
#include "base/message_loop/message_loop_current.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"

namespace gfx {

// static
SingletonHwnd* SingletonHwnd::GetInstance() {
  return base::Singleton<SingletonHwnd>::get();
}

BOOL SingletonHwnd::ProcessWindowMessage(HWND window,
                                         UINT message,
                                         WPARAM wparam,
                                         LPARAM lparam,
                                         LRESULT& result,
                                         DWORD msg_map_id) {
  if (!base::MessageLoopCurrentForUI::IsSet()) {
    // If there is no MessageLoop and SingletonHwnd is receiving messages, this
    // means it is receiving messages via an external message pump such as COM
    // uninitialization.
    //
    // It is unsafe to forward these messages as observers may depend on the
    // existence of a MessageLoop to proceed.
    return false;
  }

  for (SingletonHwndObserver& observer : observer_list_)
    observer.OnWndProc(window, message, wparam, lparam);
  return false;
}

SingletonHwnd::SingletonHwnd() {
  if (!base::MessageLoopCurrentForUI::IsSet()) {
    // Creating this window in (e.g.) a renderer inhibits shutdown on
    // Windows. See http://crbug.com/230122 and http://crbug.com/236039.
    return;
  }
  WindowImpl::Init(NULL, Rect());
}

SingletonHwnd::~SingletonHwnd() {
  // WindowImpl will clean up the hwnd value on WM_NCDESTROY.
  if (hwnd())
    DestroyWindow(hwnd());

  // Tell all of our current observers to clean themselves up.
  for (SingletonHwndObserver& observer : observer_list_)
    observer.ClearWndProc();
}

void SingletonHwnd::AddObserver(SingletonHwndObserver* observer) {
  observer_list_.AddObserver(observer);
}

void SingletonHwnd::RemoveObserver(SingletonHwndObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace gfx
