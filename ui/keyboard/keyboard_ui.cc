// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_ui.h"

#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ui_base_switches.h"
#include "ui/keyboard/keyboard_controller.h"

namespace keyboard {

KeyboardUI::KeyboardUI() = default;

KeyboardUI::~KeyboardUI() = default;

void KeyboardUI::ShowKeyboardWindow() {
  aura::Window* window = GetKeyboardWindow();
  if (window) {
    TRACE_EVENT0("vk", "ShowKeyboardWindow");
    window->Show();
  }
}

void KeyboardUI::HideKeyboardWindow() {
  aura::Window* window = GetKeyboardWindow();
  if (window)
    window->Hide();
}

void KeyboardUI::SetController(KeyboardController* controller) {
  keyboard_controller_ = controller;
}

}  // namespace keyboard
