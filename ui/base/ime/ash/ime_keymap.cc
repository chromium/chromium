// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/ime_keymap.h"

#include <stddef.h>

#include <map>

#include "base/lazy_instance.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ash {

namespace {

using ::ui::KeyboardCode;

const struct KeyCodeTable {
  KeyboardCode keyboard_code;
  const char* dom_code;
} kKeyCodeTable[] = {{ui::VKEY_BACK, "Backspace"},
                     {ui::VKEY_TAB, "Tab"},
                     {ui::VKEY_RETURN, "Enter"},
                     {ui::VKEY_MENU, "ContextMenu"},
                     {ui::VKEY_PAUSE, "Pause"},
                     {ui::VKEY_CAPITAL, "CapsLock"},
                     {ui::VKEY_KANA, "KanaMode"},
                     {ui::VKEY_HANGUL, "HangulMode"},
                     {ui::VKEY_HANJA, "Hanja"},
                     {ui::VKEY_KANJI, "Kanji"},
                     {ui::VKEY_ESCAPE, "Escape"},
                     {ui::VKEY_CONVERT, "Convert"},
                     {ui::VKEY_NONCONVERT, "NoConvert"},
                     {ui::VKEY_SPACE, "Space"},
                     {ui::VKEY_END, "End"},
                     {ui::VKEY_HOME, "Home"},
                     {ui::VKEY_LEFT, "ArrowLeft"},
                     {ui::VKEY_UP, "ArrowUp"},
                     {ui::VKEY_RIGHT, "ArrowRight"},
                     {ui::VKEY_DOWN, "ArrowDown"},
                     {ui::VKEY_SNAPSHOT, "PrintScreen"},
                     {ui::VKEY_INSERT, "Insert"},
                     {ui::VKEY_DELETE, "Delete"},
                     {ui::VKEY_HELP, "Help"},
                     {ui::VKEY_0, "Digit0"},
                     {ui::VKEY_1, "Digit1"},
                     {ui::VKEY_2, "Digit2"},
                     {ui::VKEY_3, "Digit3"},
                     {ui::VKEY_4, "Digit4"},
                     {ui::VKEY_5, "Digit5"},
                     {ui::VKEY_6, "Digit6"},
                     {ui::VKEY_7, "Digit7"},
                     {ui::VKEY_8, "Digit8"},
                     {ui::VKEY_9, "Digit9"},
                     {ui::VKEY_A, "KeyA"},
                     {ui::VKEY_B, "KeyB"},
                     {ui::VKEY_C, "KeyC"},
                     {ui::VKEY_D, "KeyD"},
                     {ui::VKEY_E, "KeyE"},
                     {ui::VKEY_F, "KeyF"},
                     {ui::VKEY_G, "KeyG"},
                     {ui::VKEY_H, "KeyH"},
                     {ui::VKEY_I, "KeyI"},
                     {ui::VKEY_J, "KeyJ"},
                     {ui::VKEY_K, "KeyK"},
                     {ui::VKEY_L, "KeyL"},
                     {ui::VKEY_M, "KeyM"},
                     {ui::VKEY_N, "KeyN"},
                     {ui::VKEY_O, "KeyO"},
                     {ui::VKEY_P, "KeyP"},
                     {ui::VKEY_Q, "KeyQ"},
                     {ui::VKEY_R, "KeyR"},
                     {ui::VKEY_S, "KeyS"},
                     {ui::VKEY_T, "KeyT"},
                     {ui::VKEY_U, "KeyU"},
                     {ui::VKEY_V, "KeyV"},
                     {ui::VKEY_W, "KeyW"},
                     {ui::VKEY_X, "KeyX"},
                     {ui::VKEY_Y, "KeyY"},
                     {ui::VKEY_Z, "KeyZ"},
                     {ui::VKEY_LWIN, "OSLeft"},
                     {ui::VKEY_RWIN, "OSRight"},
                     {ui::VKEY_NUMPAD0, "Numpad0"},
                     {ui::VKEY_NUMPAD1, "Numpad1"},
                     {ui::VKEY_NUMPAD2, "Numpad2"},
                     {ui::VKEY_NUMPAD3, "Numpad3"},
                     {ui::VKEY_NUMPAD4, "Numpad4"},
                     {ui::VKEY_NUMPAD5, "Numpad5"},
                     {ui::VKEY_NUMPAD6, "Numpad6"},
                     {ui::VKEY_NUMPAD7, "Numpad7"},
                     {ui::VKEY_NUMPAD8, "Numpad8"},
                     {ui::VKEY_NUMPAD9, "Numpad9"},
                     {ui::VKEY_MULTIPLY, "NumpadMultiply"},
                     {ui::VKEY_ADD, "NumpadAdd"},
                     {ui::VKEY_SUBTRACT, "NumpadSubtract"},
                     {ui::VKEY_DECIMAL, "NumpadDecimal"},
                     {ui::VKEY_DIVIDE, "NumpadDivide"},
                     {ui::VKEY_F1, "F1"},
                     {ui::VKEY_F2, "F2"},
                     {ui::VKEY_F3, "F3"},
                     {ui::VKEY_F4, "F4"},
                     {ui::VKEY_F5, "F5"},
                     {ui::VKEY_F6, "F6"},
                     {ui::VKEY_F7, "F7"},
                     {ui::VKEY_F8, "F8"},
                     {ui::VKEY_F9, "F9"},
                     {ui::VKEY_F10, "F10"},
                     {ui::VKEY_F11, "F11"},
                     {ui::VKEY_F12, "F12"},
                     {ui::VKEY_F13, "F13"},
                     {ui::VKEY_F14, "F14"},
                     {ui::VKEY_F15, "F15"},
                     {ui::VKEY_F16, "F16"},
                     {ui::VKEY_F17, "F17"},
                     {ui::VKEY_F18, "F18"},
                     {ui::VKEY_F19, "F19"},
                     {ui::VKEY_F20, "F20"},
                     {ui::VKEY_F21, "F21"},
                     {ui::VKEY_F22, "F22"},
                     {ui::VKEY_F23, "F23"},
                     {ui::VKEY_F24, "F24"},
                     {ui::VKEY_NUMLOCK, "NumLock"},
                     {ui::VKEY_SCROLL, "ScrollLock"},
                     {ui::VKEY_LSHIFT, "ShiftLeft"},
                     {ui::VKEY_RSHIFT, "ShiftRight"},
                     {ui::VKEY_LCONTROL, "ControlLeft"},
                     {ui::VKEY_RCONTROL, "ControlRight"},
                     {ui::VKEY_LMENU, "AltLeft"},
                     {ui::VKEY_RMENU, "AltRight"},
                     {ui::VKEY_BROWSER_BACK, "BrowserBack"},
                     {ui::VKEY_BROWSER_FORWARD, "BrowserForward"},
                     {ui::VKEY_BROWSER_REFRESH, "BrowserRefresh"},
                     {ui::VKEY_BROWSER_STOP, "BrowserStop"},
                     {ui::VKEY_BROWSER_SEARCH, "BrowserSearch"},
                     {ui::VKEY_BROWSER_HOME, "BrowserHome"},
                     {ui::VKEY_VOLUME_MUTE, "VolumeMute"},
                     {ui::VKEY_VOLUME_DOWN, "VolumeDown"},
                     {ui::VKEY_VOLUME_UP, "VolumeUp"},
                     {ui::VKEY_BRIGHTNESS_DOWN, "BrightnessDown"},
                     {ui::VKEY_BRIGHTNESS_UP, "BrightnessUp"},
                     {ui::VKEY_MEDIA_LAUNCH_APP1, "ChromeOSSwitchWindow"},
                     // LaunchApplication2 is calculator.
                     {ui::VKEY_MEDIA_LAUNCH_APP2, "LaunchApplication2"},
                     {ui::VKEY_MEDIA_NEXT_TRACK, "MediaTrackNext"},
                     {ui::VKEY_MEDIA_PREV_TRACK, "MediaTrackPrevious"},
                     {ui::VKEY_MEDIA_STOP, "MediaStop"},
                     {ui::VKEY_MEDIA_PLAY_PAUSE, "MediaPlayPause"},
                     {ui::VKEY_MEDIA_LAUNCH_MAIL, "LaunchMail"},
                     {ui::VKEY_OEM_1, "Semicolon"},
                     {ui::VKEY_OEM_PLUS, "Equal"},
                     {ui::VKEY_OEM_COMMA, "Comma"},
                     {ui::VKEY_OEM_MINUS, "Minus"},
                     {ui::VKEY_OEM_PERIOD, "Period"},
                     {ui::VKEY_OEM_2, "Slash"},
                     {ui::VKEY_OEM_3, "Backquote"},
                     {ui::VKEY_OEM_4, "BracketLeft"},
                     {ui::VKEY_OEM_5, "Backslash"},
                     {ui::VKEY_OEM_6, "BracketRight"},
                     {ui::VKEY_OEM_7, "Quote"},
                     {ui::VKEY_ZOOM, "ChromeOSFullscreen"}};

class KeyCodeMap {
 public:
  KeyCodeMap() {
    for (const auto& key_code : kKeyCodeTable) {
      map_dom_key_[key_code.dom_code] = key_code.keyboard_code;
      map_key_dom_[key_code.keyboard_code] = key_code.dom_code;
    }
  }

  KeyboardCode GetKeyboardCode(const std::string& dom_code) const {
    auto it = map_dom_key_.find(dom_code);
    return (it == map_dom_key_.end()) ? ui::VKEY_UNKNOWN : it->second;
  }

  std::string GetDomKeycode(KeyboardCode key_code) const {
    auto it = map_key_dom_.find(key_code);
    return (it == map_key_dom_.end()) ? "" : it->second;
  }

 private:
  std::map<std::string, KeyboardCode> map_dom_key_;
  std::map<KeyboardCode, std::string> map_key_dom_;
};

base::LazyInstance<KeyCodeMap>::Leaky g_keycode_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

KeyboardCode DomKeycodeToKeyboardCode(const std::string& code) {
  return g_keycode_map.Get().GetKeyboardCode(code);
}

std::string KeyboardCodeToDomKeycode(KeyboardCode code) {
  return g_keycode_map.Get().GetDomKeycode(code);
}

}  // namespace ash
