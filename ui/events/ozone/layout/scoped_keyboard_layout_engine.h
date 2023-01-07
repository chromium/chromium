// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_LAYOUT_SCOPED_KEYBOARD_LAYOUT_ENGINE_H_
#define UI_EVENTS_OZONE_LAYOUT_SCOPED_KEYBOARD_LAYOUT_ENGINE_H_

#include <memory>

#include "base/component_export.h"

namespace ui {

class KeyboardLayoutEngine;

// Sets a KeyboardLayoutEngine as the global layout engine used by ui::KeyEvent.
class COMPONENT_EXPORT(EVENTS_OZONE_LAYOUT) ScopedKeyboardLayoutEngine {
 public:
  explicit ScopedKeyboardLayoutEngine(
      std::unique_ptr<KeyboardLayoutEngine> engine);
  ~ScopedKeyboardLayoutEngine();

 private:
  const std::unique_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_LAYOUT_SCOPED_KEYBOARD_LAYOUT_ENGINE_H_
