// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/scoped_keyboard_layout_engine.h"

#include "base/check_op.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"

namespace ui {

ScopedKeyboardLayoutEngine::ScopedKeyboardLayoutEngine(
    std::unique_ptr<KeyboardLayoutEngine> keyboard_layout_engine)
    : keyboard_layout_engine_(std::move(keyboard_layout_engine)) {
  KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
      keyboard_layout_engine_.get());
}

ScopedKeyboardLayoutEngine::~ScopedKeyboardLayoutEngine() {
  DCHECK_EQ(KeyboardLayoutEngineManager::GetKeyboardLayoutEngine(),
            keyboard_layout_engine_.get());
  KeyboardLayoutEngineManager::ResetKeyboardLayoutEngine();
}

}  // namespace ui
