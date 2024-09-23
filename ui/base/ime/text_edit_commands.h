// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_TEXT_EDIT_COMMANDS_H_
#define UI_BASE_IME_TEXT_EDIT_COMMANDS_H_

namespace ui {

// Text editing commands for use by ui::TextInputClient.
// Declares named values for each of the text edit commands.
enum class TextEditCommand {
#define TEXT_EDIT_COMMAND(UI, MOJOM) UI,
#include "ui/base/ime/text_edit_commands.inc"
#undef TEXT_EDIT_COMMAND
};

}  // namespace ui

#endif  // UI_BASE_IME_TEXT_EDIT_COMMANDS_H_
