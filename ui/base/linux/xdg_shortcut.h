// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_LINUX_XDG_SHORTCUT_H_
#define UI_BASE_LINUX_XDG_SHORTCUT_H_

#include <string>

#include "base/component_export.h"

namespace ui {

class Accelerator;

// Converts an Accelerator to an XDG shortcut string by mapping the contained
// KeyboardCode to a string according to the XDG shortcut specification [1],
// which relies on strings from xkbcommon-keysyms.h [2]. This handles only
// keys supported by the chrome.commands API [3].
// clang-format off
// [1] https://specifications.freedesktop.org/shortcuts/latest/
// [2] https://raw.githubusercontent.com/xkbcommon/libxkbcommon/master/include/xkbcommon/xkbcommon-keysyms.h
// [3] https://developer.chrome.com/docs/extensions/reference/api/commands
// clang-format on
COMPONENT_EXPORT(UI_BASE)
std::string AcceleratorToXdgShortcut(const Accelerator& accelerator);

}  // namespace ui

#endif  // UI_BASE_LINUX_XDG_SHORTCUT_H_
