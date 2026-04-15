// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/linux/xdg_shortcut.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ui {

namespace {

std::string KeyboardCodeToString(KeyboardCode key_code) {
  if (key_code >= VKEY_A && key_code <= VKEY_Z) {
    return std::string(1, static_cast<char>('a' + (key_code - VKEY_A)));
  }
  if (key_code >= VKEY_0 && key_code <= VKEY_9) {
    return std::string(1, static_cast<char>('0' + (key_code - VKEY_0)));
  }
  if (key_code >= VKEY_F1 && key_code <= VKEY_F12) {
    return "F" + base::NumberToString(key_code - VKEY_F1 + 1);
  }

  switch (key_code) {
    // General keys
    case VKEY_OEM_COMMA:
      return "comma";
    case VKEY_OEM_PERIOD:
      return "period";
    case VKEY_HOME:
      return "Home";
    case VKEY_END:
      return "End";
    case VKEY_PRIOR:
      return "Prior";
    case VKEY_NEXT:
      return "Next";
    case VKEY_SPACE:
      return "space";
    case VKEY_INSERT:
      return "Insert";
    case VKEY_DELETE:
      return "Delete";

    // Arrow keys
    case VKEY_LEFT:
      return "Left";
    case VKEY_UP:
      return "Up";
    case VKEY_RIGHT:
      return "Right";
    case VKEY_DOWN:
      return "Down";

    // Media keys
    case VKEY_MEDIA_NEXT_TRACK:
      return "XF86AudioNext";
    case VKEY_MEDIA_PREV_TRACK:
      return "XF86AudioPrev";
    case VKEY_MEDIA_STOP:
      return "XF86AudioStop";
    case VKEY_MEDIA_PLAY_PAUSE:
      return "XF86AudioPlay";

    default:
      return "";
  }
}

}  // namespace

std::string AcceleratorToXdgShortcut(const Accelerator& accelerator) {
  std::vector<std::string> parts;

  if (accelerator.IsCtrlDown()) {
    parts.push_back("CTRL");
  }
  if (accelerator.IsAltDown()) {
    parts.push_back("ALT");
  }
  if (accelerator.IsShiftDown()) {
    parts.push_back("SHIFT");
  }
  if (accelerator.IsCmdDown()) {
    parts.push_back("LOGO");
  }

  std::string key = KeyboardCodeToString(accelerator.key_code());
  if (key.empty()) {
    return "";
  }
  parts.push_back(std::move(key));
  return base::JoinString(parts, "+");
}

}  // namespace ui
