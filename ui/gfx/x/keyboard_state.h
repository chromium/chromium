// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_KEYBOARD_STATE_H_
#define UI_GFX_X_KEYBOARD_STATE_H_

#include <memory>

#include "ui/gfx/x/xproto.h"

namespace x11 {

class Connection;

// This is an interface used by Connection to manage conversion between keycodes
// (8 bit values) and keysyms (32 bit values).
class KeyboardState {
 public:
  KeyboardState();
  virtual ~KeyboardState();

  virtual KeyCode KeysymToKeycode(uint32_t keysym) const = 0;
  virtual uint32_t KeycodeToKeysym(KeyCode keycode,
                                   uint32_t modifiers) const = 0;

 private:
  friend class Connection;

  virtual void UpdateMapping() = 0;
};

std::unique_ptr<KeyboardState> CreateKeyboardState(Connection* connection);

}  // namespace x11

#endif  // UI_GFX_X_KEYBOARD_STATE_H_
