// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_LINUX_XDG_SHORTCUT_H_
#define UI_BASE_LINUX_XDG_SHORTCUT_H_

#include <string>

#include "base/component_export.h"
#include "ui/base/accelerators/accelerator.h"

namespace ui {

// Converts an Accelerator to an XDG shortcut string.  The contained
// KeyboardCode to a string according to the XDG shortcut specification [1],
// which relies on strings from xkbcommon-keysyms.h [2]. This handles only keys
// supported by the chrome.commands API [3].
// [1]
// https://gitlab.freedesktop.org/xdg/xdg-specs/-/blob/master/shortcuts/shortcuts-spec.xml
// [2]
// https://raw.githubusercontent.com/xkbcommon/libxkbcommon/master/include/xkbcommon/xkbcommon-keysyms.h
// [3] https://developer.chrome.com/docs/extensions/reference/api/commands
COMPONENT_EXPORT(UI_BASE)
std::string AcceleratorToXdgShortcut(const Accelerator& accelerator);

}  // namespace ui

#endif  // UI_BASE_LINUX_XDG_SHORTCUT_H_
