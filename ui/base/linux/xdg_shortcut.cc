// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/linux/xdg_shortcut.h"

#include <vector>

#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"

namespace ui {

std::string KeyboardCodeToString(KeyboardCode key_code) {
  if (key_code >= VKEY_A && key_code <= VKEY_Z) {
    return std::string(1, base::checked_cast<char>('a' + (key_code - VKEY_A)));
  }
  if (key_code >= VKEY_0 && key_code <= VKEY_9) {
    return std::string(1, base::checked_cast<char>('0' + (key_code - VKEY_0)));
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
      return "UnknownKey";
  }
}

std::string AcceleratorToXdgShortcut(const Accelerator& accelerator) {
  std::vector<std::string> parts;

  // Map Chromium modifiers to XDG spec modifiers
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

  parts.push_back(KeyboardCodeToString(accelerator.key_code()));

  return base::JoinString(parts, "+");
}

}  // namespace ui
