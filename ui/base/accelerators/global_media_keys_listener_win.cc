// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/global_media_keys_listener_win.h"

#include "base/functional/bind.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/keycodes/keyboard_code_conversion_win.h"
#include "ui/gfx/win/singleton_hwnd_hot_key_observer.h"

namespace ui {

// static
bool GlobalMediaKeysListenerWin::has_instance_ = false;

GlobalMediaKeysListenerWin::GlobalMediaKeysListenerWin(
    MediaKeysListener::Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  DCHECK(!has_instance_);
  has_instance_ = true;
}

GlobalMediaKeysListenerWin::~GlobalMediaKeysListenerWin() {
  has_instance_ = false;
}

bool GlobalMediaKeysListenerWin::StartWatchingMediaKey(KeyboardCode key_code) {
  DCHECK(IsMediaKeycode(key_code));

  // If the hotkey is already registered, do nothing.
  if (key_codes_hotkey_observers_.contains(key_code))
    return true;

  // Create an observer that registers a hot key for |key_code|.
  std::unique_ptr<gfx::SingletonHwndHotKeyObserver> observer =
      gfx::SingletonHwndHotKeyObserver::Create(
          base::BindRepeating(&GlobalMediaKeysListenerWin::OnWndProc,
                              base::Unretained(this)),
          key_code, /*modifiers=*/0);

  // If observer is null, then the hot key failed to register.
  bool success = !!observer;
  if (success)
    key_codes_hotkey_observers_[key_code] = std::move(observer);

  return success;
}

void GlobalMediaKeysListenerWin::StopWatchingMediaKey(KeyboardCode key_code) {
  DCHECK(IsMediaKeycode(key_code));

  // Deleting the observer automatically unregisters the hot key.
  key_codes_hotkey_observers_.erase(key_code);
}

void GlobalMediaKeysListenerWin::OnWndProc(HWND hwnd,
                                           UINT message,
                                           WPARAM wparam,
                                           LPARAM lparam) {
  // SingletonHwndHotKeyObservers should only send us hot key messages.
  DCHECK_EQ(WM_HOTKEY, static_cast<int>(message));

  WORD win_key_code = HIWORD(lparam);
  KeyboardCode key_code = KeyboardCodeForWindowsKeyCode(win_key_code);

  // We should only receive hot key events for keys that we're observing.
  DCHECK(key_codes_hotkey_observers_.contains(key_code));

  int modifiers = 0;
  modifiers |= (LOWORD(lparam) & MOD_SHIFT) ? ui::EF_SHIFT_DOWN : 0;
  modifiers |= (LOWORD(lparam) & MOD_ALT) ? ui::EF_ALT_DOWN : 0;
  modifiers |= (LOWORD(lparam) & MOD_CONTROL) ? ui::EF_CONTROL_DOWN : 0;
  Accelerator accelerator(key_code, modifiers);

  delegate_->OnMediaKeysAccelerator(accelerator);
}

}  // namespace ui