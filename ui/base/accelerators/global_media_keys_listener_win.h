// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_GLOBAL_MEDIA_KEYS_LISTENER_WIN_H_
#define UI_BASE_ACCELERATORS_GLOBAL_MEDIA_KEYS_LISTENER_WIN_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/win/windows_types.h"
#include "ui/base/accelerators/media_keys_listener.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace gfx {

class SingletonHwndHotKeyObserver;

}  // namespace gfx

namespace ui {

// Implementation of MediaKeysListener that uses RegisterHotKey to globally
// listen for media key presses. It only allows for a single instance to be
// created in order to prevent conflicts form multiple listeners.
class COMPONENT_EXPORT(UI_BASE) GlobalMediaKeysListenerWin
    : public MediaKeysListener {
 public:
  explicit GlobalMediaKeysListenerWin(MediaKeysListener::Delegate* delegate);

  GlobalMediaKeysListenerWin(const GlobalMediaKeysListenerWin&) = delete;
  GlobalMediaKeysListenerWin& operator=(const GlobalMediaKeysListenerWin&) =
      delete;

  ~GlobalMediaKeysListenerWin() override;

  static bool has_instance() { return has_instance_; }

  // MediaKeysListener implementation.
  bool StartWatchingMediaKey(KeyboardCode key_code) override;
  void StopWatchingMediaKey(KeyboardCode key_code) override;

 private:
  // Called by SingletonHwndObserver.
  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  static bool has_instance_;

  raw_ptr<MediaKeysListener::Delegate> delegate_;
  base::flat_map<KeyboardCode,
                 std::unique_ptr<gfx::SingletonHwndHotKeyObserver>>
      key_codes_hotkey_observers_;
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_GLOBAL_MEDIA_KEYS_LISTENER_WIN_H_
