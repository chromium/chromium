// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"

#include "base/check.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"

namespace ui {

// static
KeyboardLayoutEngine* KeyboardLayoutEngineManager::keyboard_layout_engine_;

// static
void KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
    KeyboardLayoutEngine* keyboard_layout_engine) {
  DCHECK(!keyboard_layout_engine_);
  DCHECK(keyboard_layout_engine);
  keyboard_layout_engine_ = keyboard_layout_engine;
}

// static
void KeyboardLayoutEngineManager::ResetKeyboardLayoutEngine() {
  DCHECK(keyboard_layout_engine_);
  keyboard_layout_engine_ = nullptr;
}

}  // namespace ui
