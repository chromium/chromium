// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_LINUX_TEXT_EDIT_COMMAND_AURALINUX_H_
#define UI_BASE_IME_LINUX_TEXT_EDIT_COMMAND_AURALINUX_H_

#include <string>

#include "base/component_export.h"

namespace ui {

enum class TextEditCommand;

COMPONENT_EXPORT(UI_BASE_IME_LINUX)
std::string TextEditCommandToString(ui::TextEditCommand command);

}  // namespace ui

#endif  // UI_BASE_IME_LINUX_TEXT_EDIT_COMMAND_AURALINUX_H_
