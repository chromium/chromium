// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_TEST_KEYBOARD_TEST_UTIL_H_
#define UI_KEYBOARD_TEST_KEYBOARD_TEST_UTIL_H_

#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/ime/dummy_input_method.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_ui.h"

namespace gfx {
class Rect;
}

namespace keyboard {

// Waits until the keyboard is fully shown, with no pending animations.
bool WaitUntilShown();

// Waits until the keyboard starts to hide, with possible pending animations.
bool WaitUntilHidden();

// Waits until the keyboard state is changed to the given state.
void WaitControllerStateChangesTo(const KeyboardControllerState state);

// Returns true if the keyboard is about to show or already shown.
bool IsKeyboardShowing();

// Returns true if the keyboard is about to hide or already hidden.
bool IsKeyboardHiding();

// Gets the calculated keyboard bounds from |root_bounds|. The keyboard height
// is specified by |keyboard_height|.
gfx::Rect KeyboardBoundsFromRootBounds(const gfx::Rect& root_bounds,
                                       int keyboard_height);

class TestKeyboardUI : public KeyboardUI {
 public:
  TestKeyboardUI(ui::InputMethod* input_method);
  ~TestKeyboardUI() override;

  // Overridden from KeyboardUI:
  aura::Window* LoadKeyboardWindow(LoadCallback callback) override;
  aura::Window* GetKeyboardWindow() const override;
  ui::InputMethod* GetInputMethod() override;
  void ReloadKeyboardIfNeeded() override {}
  void InitInsets(const gfx::Rect& keyboard_bounds) override {}
  void ResetInsets() override {}

 private:
  std::unique_ptr<aura::Window> window_;
  aura::test::TestWindowDelegate delegate_;
  ui::InputMethod* input_method_;

  DISALLOW_COPY_AND_ASSIGN(TestKeyboardUI);
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_TEST_KEYBOARD_TEST_UTIL_H_
