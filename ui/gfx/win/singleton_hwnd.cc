// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/singleton_hwnd.h"

#include <utility>

#include "base/callback_list.h"
#include "base/no_destructor.h"
#include "base/task/current_thread.h"
#include "base/win/windows_types.h"

namespace gfx {

// static
SingletonHwnd* SingletonHwnd::GetInstance() {
  static base::NoDestructor<SingletonHwnd> s_hwnd;
  return s_hwnd.get();
}

BOOL SingletonHwnd::ProcessWindowMessage(HWND window,
                                         UINT message,
                                         WPARAM wparam,
                                         LPARAM lparam,
                                         LRESULT& result,
                                         DWORD msg_map_id) {
  // If there is no MessageLoop and SingletonHwnd is receiving messages, this
  // means it is receiving messages via an external message pump such as COM
  // uninitialization.
  //
  // It is unsafe to forward these messages as observers may depend on the
  // existence of a MessageLoop to proceed.
  if (base::CurrentUIThread::IsSet()) {
    callback_list_.Notify(window, message, wparam, lparam);
  }
  return false;
}

base::CallbackListSubscription SingletonHwnd::RegisterCallback(
    CallbackList::CallbackType callback) {
  return callback_list_.Add(std::move(callback));
}

SingletonHwnd::SingletonHwnd() {
  // Creating this window in (e.g.) a renderer inhibits Windows shutdown. See
  // http://crbug.com/40312501 and http://crbug.com/40315446.
  if (base::CurrentUIThread::IsSet()) {
    WindowImpl::Init(NULL, Rect());
  }
}

SingletonHwnd::~SingletonHwnd() {
  NOTREACHED();  // Never destroyed.
}

}  // namespace gfx
