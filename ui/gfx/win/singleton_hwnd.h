// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_SINGLETON_HWND_H_
#define UI_GFX_WIN_SINGLETON_HWND_H_

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/win/windows_types.h"
#include "ui/gfx/win/window_impl.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace gfx {

// Singleton message-only HWND that allows interested clients to receive WM_*
// notifications.
class COMPONENT_EXPORT(GFX) SingletonHwnd : public WindowImpl {
 public:
  using CallbackList =
      base::RepeatingCallbackList<void(HWND, UINT, WPARAM, LPARAM)>;

  static SingletonHwnd* GetInstance();

  SingletonHwnd(const SingletonHwnd&) = delete;
  SingletonHwnd& operator=(const SingletonHwnd&) = delete;

  // Windows callback for WM_* notifications.
  BOOL ProcessWindowMessage(HWND window,
                            UINT message,
                            WPARAM wparam,
                            LPARAM lparam,
                            LRESULT& result,
                            DWORD msg_map_id) override;

  base::CallbackListSubscription RegisterCallback(
      CallbackList::CallbackType callback);

 private:
  friend class base::NoDestructor<SingletonHwnd>;

  SingletonHwnd();
  ~SingletonHwnd() override;

  CallbackList callback_list_;
};

}  // namespace gfx

#endif  // UI_GFX_WIN_SINGLETON_HWND_H_
