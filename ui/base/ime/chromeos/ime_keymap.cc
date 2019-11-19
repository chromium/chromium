// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/ime_keymap.h"

#include <stddef.h>

#include <map>

#include "base/lazy_instance.h"
#include "base/stl_util.h"

namespace ui {

namespace {

const struct KeyCodeTable {
  KeyboardCode keyboard_code;
  const char* dom_code;
} kKeyCodeTable[] = {{VKEY_BACK, "Backspace"},
                     {VKEY_TAB, "Tab"},
                     {VKEY_RETURN, "Enter"},
                     {VKEY_MENU, "ContextMenu"},
                     {VKEY_PAUSE, "Pause"},
                     {VKEY_CAPITAL, "CapsLock"},
                     {VKEY_KANA, "KanaMode"},
                     {VKEY_HANGUL, "HangulMode"},
                     {VKEY_HANJA, "Hanja"},
                     {VKEY_KANJI, "Kanji"},
                     {VKEY_ESCAPE, "Escape"},
                     {VKEY_CONVERT, "Convert"},
                     {VKEY_NONCONVERT, "NoConvert"},
                     {VKEY_SPACE, "Space"},
                     {VKEY_END, "End"},
                     {VKEY_HOME, "Home"},
                     {VKEY_LEFT, "ArrowLeft"},
                     {VKEY_UP, "ArrowUp"},
                     {VKEY_RIGHT, "ArrowRight"},
                     {VKEY_DOWN, "ArrowDown"},
                     {VKEY_SNAPSHOT, "PrintScreen"},
                     {VKEY_INSERT, "Insert"},
                     {VKEY_DELETE, "Delete"},
                     {VKEY_HELP, "Help"},
                     {VKEY_0, "Digit0"},
                     {VKEY_1, "Digit1"},
                     {VKEY_2, "Digit2"},
                     {VKEY_3, "Digit3"},
                     {VKEY_4, "Digit4"},
                     {VKEY_5, "Digit5"},
                     {VKEY_6, "Digit6"},
                     {VKEY_7, "Digit7"},
                     {VKEY_8, "Digit8"},
                     {VKEY_9, "Digit9"},
                     {VKEY_A, "KeyA"},
                     {VKEY_B, "KeyB"},
                     {VKEY_C, "KeyC"},
                     {VKEY_D, "KeyD"},
                     {VKEY_E, "KeyE"},
                     {VKEY_F, "KeyF"},
                     {VKEY_G, "KeyG"},
                     {VKEY_H, "KeyH"},
                     {VKEY_I, "KeyI"},
                     {VKEY_J, "KeyJ"},
                     {VKEY_K, "KeyK"},
                     {VKEY_L, "KeyL"},
                     {VKEY_M, "KeyM"},
                     {VKEY_N, "KeyN"},
                     {VKEY_O, "KeyO"},
                     {VKEY_P, "KeyP"},
                     {VKEY_Q, "KeyQ"},
                     {VKEY_R, "KeyR"},
                     {VKEY_S, "KeyS"},
                     {VKEY_T, "KeyT"},
                     {VKEY_U, "KeyU"},
                     {VKEY_V, "KeyV"},
                     {VKEY_W, "KeyW"},
                     {VKEY_X, "KeyX"},
                     {VKEY_Y, "KeyY"},
                     {VKEY_Z, "KeyZ"},
                     {VKEY_LWIN, "OSLeft"},
                     {VKEY_RWIN, "OSRight"},
                     {VKEY_NUMPAD0, "Numpad0"},
                     {VKEY_NUMPAD1, "Numpad1"},
                     {VKEY_NUMPAD2, "Numpad2"},
                     {VKEY_NUMPAD3, "Numpad3"},
                     {VKEY_NUMPAD4, "Numpad4"},
                     {VKEY_NUMPAD5, "Numpad5"},
                     {VKEY_NUMPAD6, "Numpad6"},
                     {VKEY_NUMPAD7, "Numpad7"},
                     {VKEY_NUMPAD8, "Numpad8"},
                     {VKEY_NUMPAD9, "Numpad9"},
                     {VKEY_MULTIPLY, "NumpadMultiply"},
                     {VKEY_ADD, "NumpadAdd"},
                     {VKEY_SUBTRACT, "NumpadSubtract"},
                     {VKEY_DECIMAL, "NumpadDecimal"},
                     {VKEY_DIVIDE, "NumpadDivide"},
                     {VKEY_F1, "F1"},
                     {VKEY_F2, "F2"},
                     {VKEY_F3, "F3"},
                     {VKEY_F4, "F4"},
                     {VKEY_F5, "F5"},
                     {VKEY_F6, "F6"},
                     {VKEY_F7, "F7"},
                     {VKEY_F8, "F8"},
                     {VKEY_F9, "F9"},
                     {VKEY_F10, "F10"},
                     {VKEY_F11, "F11"},
                     {VKEY_F12, "F12"},
                     {VKEY_F13, "F13"},
                     {VKEY_F14, "F14"},
                     {VKEY_F15, "F15"},
                     {VKEY_F16, "F16"},
                     {VKEY_F17, "F17"},
                     {VKEY_F18, "F18"},
                     {VKEY_F19, "F19"},
                     {VKEY_F20, "F20"},
                     {VKEY_F21, "F21"},
                     {VKEY_F22, "F22"},
                     {VKEY_F23, "F23"},
                     {VKEY_F24, "F24"},
                     {VKEY_NUMLOCK, "NumLock"},
                     {VKEY_SCROLL, "ScrollLock"},
                     {VKEY_LSHIFT, "ShiftLeft"},
                     {VKEY_RSHIFT, "ShiftRight"},
                     {VKEY_LCONTROL, "ControlLeft"},
                     {VKEY_RCONTROL, "ControlRight"},
                     {VKEY_LMENU, "AltLeft"},
                     {VKEY_RMENU, "AltRight"},
                     {VKEY_BROWSER_BACK, "BrowserBack"},
                     {VKEY_BROWSER_FORWARD, "BrowserForward"},
                     {VKEY_BROWSER_REFRESH, "BrowserRefresh"},
                     {VKEY_BROWSER_STOP, "BrowserStop"},
                     {VKEY_BROWSER_SEARCH, "BrowserSearch"},
                     {VKEY_BROWSER_HOME, "BrowserHome"},
                     {VKEY_VOLUME_MUTE, "VolumeMute"},
                     {VKEY_VOLUME_DOWN, "VolumeDown"},
                     {VKEY_VOLUME_UP, "VolumeUp"},
                     {VKEY_BRIGHTNESS_DOWN, "BrightnessDown"},
                     {VKEY_BRIGHTNESS_UP, "BrightnessUp"},
                     {VKEY_MEDIA_LAUNCH_APP1, "ChromeOSSwitchWindow"},
                     {VKEY_MEDIA_LAUNCH_APP2, "ChromeOSFullscreen"},
                     {VKEY_MEDIA_NEXT_TRACK, "MediaTrackNext"},
                     {VKEY_MEDIA_PREV_TRACK, "MediaTrackPrevious"},
                     {VKEY_MEDIA_STOP, "MediaStop"},
                     {VKEY_MEDIA_PLAY_PAUSE, "MediaPlayPause"},
                     {VKEY_MEDIA_LAUNCH_MAIL, "LaunchMail"},
                     {VKEY_OEM_1, "Semicolon"},
                     {VKEY_OEM_PLUS, "Equal"},
                     {VKEY_OEM_COMMA, "Comma"},
                     {VKEY_OEM_MINUS, "Minus"},
                     {VKEY_OEM_PERIOD, "Period"},
                     {VKEY_OEM_2, "Slash"},
                     {VKEY_OEM_3, "Backquote"},
                     {VKEY_OEM_4, "BracketLeft"},
                     {VKEY_OEM_5, "Backslash"},
                     {VKEY_OEM_6, "BracketRight"},
                     {VKEY_OEM_7, "Quote"}};

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
    return (it == map_dom_key_.end()) ? VKEY_UNKNOWN : it->second;
  }

  std::string GetDomKeycode(KeyboardCode key_code) const {
    auto it = map_key_dom_.find(key_code);
    return (it == map_key_dom_.end()) ? "" : it->second;
  }

 private:
  std::map<std::string, KeyboardCode> map_dom_key_;
  std::map<KeyboardCode, std::string> map_key_dom_;
};

base::LazyInstance<KeyCodeMap>::Leaky g_keycode_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

KeyboardCode DomKeycodeToKeyboardCode(const std::string& code) {
  return g_keycode_map.Get().GetKeyboardCode(code);
}

std::string KeyboardCodeToDomKeycode(KeyboardCode code) {
  return g_keycode_map.Get().GetDomKeycode(code);
}

}  // namespace ui
