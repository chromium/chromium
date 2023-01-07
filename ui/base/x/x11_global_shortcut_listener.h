// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_GLOBAL_SHORTCUT_LISTENER_H_
#define UI_BASE_X_X11_GLOBAL_SHORTCUT_LISTENER_H_

#include <stdint.h>

#include <set>

#include "base/memory/raw_ptr.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {
class Connection;
}

namespace ui {

class KeyEvent;

// X11-specific implementation of the class that listens for global shortcuts.
class COMPONENT_EXPORT(UI_BASE_X) XGlobalShortcutListener
    : public PlatformEventDispatcher {
 public:
  XGlobalShortcutListener();
  XGlobalShortcutListener(const XGlobalShortcutListener&) = delete;
  XGlobalShortcutListener& operator=(const XGlobalShortcutListener&) = delete;
  ~XGlobalShortcutListener() override;

  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

 protected:
  // Called when the previously registered key combination is pressed.
  // The implementation should forward the output to the owner.
  virtual void OnKeyPressed(KeyboardCode key_code,
                            bool is_alt_down,
                            bool is_ctrl_down,
                            bool is_shift_down) = 0;

  void StartListening();
  void StopListening();
  bool RegisterAccelerator(KeyboardCode key_code,
                           bool is_alt_down,
                           bool is_ctrl_down,
                           bool is_shift_down);
  void UnregisterAccelerator(KeyboardCode key_code,
                             bool is_alt_down,
                             bool is_ctrl_down,
                             bool is_shift_down);

 private:
  // Due to how system key grabbing works on X11, we have to be a bit greedy and
  // register combinations that we will later reject (see the comment for
  // kModifiersMasks in the cc file).  For that we store registered combinations
  // and filter the incoming events against that registry before notifying the
  // observer.  This tuple describes the meaningful parts of the event; booleans
  // 1, 2, and 3 hold states of Alt, Control, and Shift keys, respectively.
  using Accelerator = std::tuple<KeyboardCode, bool, bool, bool>;

  // Invoked when a global shortcut is pressed.
  void OnKeyPressEvent(const KeyEvent& event);

  // Whether this object is listening for global shortcuts.
  bool is_listening_ = false;

  // Key combinations that we are interested in.
  std::set<Accelerator> registered_combinations_;

  // The x11 default display and the native root window.
  raw_ptr<x11::Connection> connection_;
  x11::Window x_root_window_;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_GLOBAL_SHORTCUT_LISTENER_H_
