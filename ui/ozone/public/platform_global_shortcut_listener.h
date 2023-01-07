// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_PLATFORM_GLOBAL_SHORTCUT_LISTENER_H_
#define UI_OZONE_PUBLIC_PLATFORM_GLOBAL_SHORTCUT_LISTENER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

// The platform implementation should notify the wrapper through this
// interface when the registered shortcut is activated, or when the
// implementation is destroyed.
class COMPONENT_EXPORT(OZONE_BASE) PlatformGlobalShortcutListenerDelegate {
 public:
  // Called back when the previously registered key combination is pressed.
  virtual void OnKeyPressed(KeyboardCode key_code,
                            bool is_alt_down,
                            bool is_ctrl_down,
                            bool is_shift_down) = 0;
  // Called back when the platform implementation is destroyed.
  virtual void OnPlatformListenerDestroyed() = 0;

 protected:
  virtual ~PlatformGlobalShortcutListenerDelegate();
};

// The interface to Ozone platform's functionality exposed to Chrome via
// extensions::GlobalShortcutListenerOzone.
//
// Lifetimes of extensions::GlobalShortcutListenerOzone and the platform
// implementation are independent, so these entities should decouple
// explicitly upon destruction through OnPlatformListenerDestroyed() and
// ResetDelegate(), depending on which object is destroyed first.
class COMPONENT_EXPORT(OZONE_BASE) PlatformGlobalShortcutListener {
 public:
  explicit PlatformGlobalShortcutListener(
      PlatformGlobalShortcutListenerDelegate* delegate);
  virtual ~PlatformGlobalShortcutListener();

  void ResetDelegate() { delegate_ = nullptr; }

  // The following interface serves the same purpose like the one defined in
  // extensions::GlobalShortcutListener, and does it the same way.  The only
  // difference is that the platform can not use ui::Accelerator directly so it
  // accepts it split into key code and modifiers.
  virtual void StartListening() = 0;
  virtual void StopListening() = 0;
  virtual bool RegisterAccelerator(KeyboardCode key_code,
                                   bool is_alt_down,
                                   bool is_ctrl_down,
                                   bool is_shift_down) = 0;
  virtual void UnregisterAccelerator(KeyboardCode key_code,
                                     bool is_alt_down,
                                     bool is_ctrl_down,
                                     bool is_shift_down) = 0;

 protected:
  PlatformGlobalShortcutListenerDelegate* delegate() { return delegate_; }

 private:
  raw_ptr<PlatformGlobalShortcutListenerDelegate> delegate_;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_PLATFORM_GLOBAL_SHORTCUT_LISTENER_H_
