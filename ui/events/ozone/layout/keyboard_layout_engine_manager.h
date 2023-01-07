// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_LAYOUT_KEYBOARD_LAYOUT_ENGINE_MANAGER_H_
#define UI_EVENTS_OZONE_LAYOUT_KEYBOARD_LAYOUT_ENGINE_MANAGER_H_


#include "base/check.h"
#include "base/component_export.h"

namespace ui {

class KeyboardLayoutEngine;

class COMPONENT_EXPORT(EVENTS_OZONE_LAYOUT) KeyboardLayoutEngineManager {
 public:
  KeyboardLayoutEngineManager(const KeyboardLayoutEngineManager&) = delete;
  KeyboardLayoutEngineManager& operator=(const KeyboardLayoutEngineManager&) =
      delete;

  virtual ~KeyboardLayoutEngineManager();

  static void SetKeyboardLayoutEngine(
      KeyboardLayoutEngine* keyboard_layout_engine);

  static void ResetKeyboardLayoutEngine();

  static KeyboardLayoutEngine* GetKeyboardLayoutEngine() {
    // Must run in a context with a KeyboardLayoutEngine.
    // Hint: Tests can use ui::ScopedKeyboardLayout to create one.
    // (production code should instead call InitializeForUI).
    DCHECK(keyboard_layout_engine_);
    return keyboard_layout_engine_;
  }

 private:
  static KeyboardLayoutEngine* keyboard_layout_engine_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_LAYOUT_KEYBOARD_LAYOUT_ENGINE_MANAGER_H_
