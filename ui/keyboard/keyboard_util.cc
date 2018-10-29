// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_util.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/constants.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"

namespace keyboard {

namespace {

const char kKeyDown[] = "keydown";
const char kKeyUp[] = "keyup";

void SendProcessKeyEvent(ui::EventType type, aura::WindowTreeHost* host) {
  ui::KeyEvent event(type, ui::VKEY_PROCESSKEY, ui::DomCode::NONE,
                     ui::EF_IS_SYNTHESIZED, ui::DomKey::PROCESS,
                     ui::EventTimeForNow());
  ui::EventDispatchDetails details =
      host->event_sink()->OnEventFromSource(&event);
  CHECK(!details.dispatcher_destroyed);
}

// Until src/chrome is fully transitioned to use ChromeKeyboardControllerClient
// we need to test whether KeyboardController exists; it is null in OopMash.
// TODO(stevenjb): Remove remaining calls from src/chrome.
// https://crbug.com/84332.

bool GetFlag(mojom::KeyboardEnableFlag flag) {
  auto* controller = KeyboardController::Get();
  return controller ? controller->IsEnableFlagSet(flag) : false;
}

void SetOrClearEnableFlag(mojom::KeyboardEnableFlag flag, bool enabled) {
  auto* controller = KeyboardController::Get();
  if (!controller)
    return;
  if (enabled)
    controller->SetEnableFlag(flag);
  else
    controller->ClearEnableFlag(flag);
}

}  // namespace

void SetAccessibilityKeyboardEnabled(bool enabled) {
  SetOrClearEnableFlag(mojom::KeyboardEnableFlag::kAccessibilityEnabled,
                       enabled);
}

bool GetAccessibilityKeyboardEnabled() {
  return GetFlag(mojom::KeyboardEnableFlag::kAccessibilityEnabled);
}

void SetKeyboardEnabledFromShelf(bool enabled) {
  SetOrClearEnableFlag(mojom::KeyboardEnableFlag::kShelfEnabled, enabled);
}

bool GetKeyboardEnabledFromShelf() {
  return GetFlag(mojom::KeyboardEnableFlag::kShelfEnabled);
}

void SetTouchKeyboardEnabled(bool enabled) {
  SetOrClearEnableFlag(mojom::KeyboardEnableFlag::kTouchEnabled, enabled);
}

bool GetTouchKeyboardEnabled() {
  return GetFlag(mojom::KeyboardEnableFlag::kTouchEnabled);
}

std::string GetKeyboardLayout() {
  // TODO(bshe): layout string is currently hard coded. We should use more
  // standard keyboard layouts.
  return GetAccessibilityKeyboardEnabled() ? "system-qwerty" : "qwerty";
}

bool IsKeyboardEnabled() {
  return KeyboardController::Get()->IsKeyboardEnableRequested();
}

bool SendKeyEvent(const std::string type,
                  int key_value,
                  int key_code,
                  std::string key_name,
                  int modifiers,
                  aura::WindowTreeHost* host) {
  ui::EventType event_type = ui::ET_UNKNOWN;
  if (type == kKeyDown)
    event_type = ui::ET_KEY_PRESSED;
  else if (type == kKeyUp)
    event_type = ui::ET_KEY_RELEASED;
  if (event_type == ui::ET_UNKNOWN)
    return false;

  ui::KeyboardCode code = static_cast<ui::KeyboardCode>(key_code);

  ui::InputMethod* input_method = host->GetInputMethod();
  if (code == ui::VKEY_UNKNOWN) {
    // Handling of special printable characters (e.g. accented characters) for
    // which there is no key code.
    if (event_type == ui::ET_KEY_RELEASED) {
      if (!input_method)
        return false;

      // This can be null if no text input field is not focused.
      ui::TextInputClient* tic = input_method->GetTextInputClient();

      SendProcessKeyEvent(ui::ET_KEY_PRESSED, host);

      ui::KeyEvent char_event(key_value, code, ui::DomCode::NONE, ui::EF_NONE);
      if (tic)
        tic->InsertChar(char_event);
      SendProcessKeyEvent(ui::ET_KEY_RELEASED, host);
    }
  } else {
    if (event_type == ui::ET_KEY_RELEASED) {
      // The number of key press events seen since the last backspace.
      static int keys_seen = 0;
      if (code == ui::VKEY_BACK) {
        // Log the rough lengths of characters typed between backspaces. This
        // metric will be used to determine the error rate for the keyboard.
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "VirtualKeyboard.KeystrokesBetweenBackspaces", keys_seen, 1, 1000,
            50);
        keys_seen = 0;
      } else {
        ++keys_seen;
      }
    }

    ui::DomCode dom_code = ui::KeycodeConverter::CodeStringToDomCode(key_name);
    if (dom_code == ui::DomCode::NONE)
      dom_code = ui::UsLayoutKeyboardCodeToDomCode(code);
    CHECK(dom_code != ui::DomCode::NONE);
    ui::KeyEvent event(event_type, code, dom_code, modifiers);

    // Marks the simulated key event is from the Virtual Keyboard.
    ui::Event::Properties properties;
    properties[ui::kPropertyFromVK] = std::vector<uint8_t>();
    event.SetProperties(properties);

    ui::EventDispatchDetails details =
        host->event_sink()->OnEventFromSource(&event);
    CHECK(!details.dispatcher_destroyed);
  }
  return true;
}

}  // namespace keyboard
