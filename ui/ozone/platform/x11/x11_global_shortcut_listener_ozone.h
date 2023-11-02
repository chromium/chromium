// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_GLOBAL_SHORTCUT_LISTENER_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_GLOBAL_SHORTCUT_LISTENER_OZONE_H_

#include "ui/base/x/x11_global_shortcut_listener.h"
#include "ui/ozone/public/platform_global_shortcut_listener.h"

namespace ui {

class X11GlobalShortcutListenerOzone : public PlatformGlobalShortcutListener,
                                       public ui::XGlobalShortcutListener {
 public:
  explicit X11GlobalShortcutListenerOzone(
      PlatformGlobalShortcutListenerDelegate* delegate);
  X11GlobalShortcutListenerOzone(const X11GlobalShortcutListenerOzone&) =
      delete;
  X11GlobalShortcutListenerOzone& operator=(
      const X11GlobalShortcutListenerOzone&) = delete;
  ~X11GlobalShortcutListenerOzone() override;

 private:
  // GlobalShortcutListener implementation.
  void StartListening() override;
  void StopListening() override;
  bool RegisterAccelerator(KeyboardCode key_code,
                           bool is_alt_down,
                           bool is_ctrl_down,
                           bool is_shift_down) override;
  void UnregisterAccelerator(KeyboardCode key_code,
                             bool is_alt_down,
                             bool is_ctrl_down,
                             bool is_shift_down) override;

  // ui::XGlobalShortcutListener:
  void OnKeyPressed(KeyboardCode key_code,
                    bool is_alt_down,
                    bool is_ctrl_down,
                    bool is_shift_down) override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_GLOBAL_SHORTCUT_LISTENER_OZONE_H_
