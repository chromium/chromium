// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_LINUX_TEXT_EDIT_COMMAND_AURALINUX_H_
#define UI_BASE_IME_LINUX_TEXT_EDIT_COMMAND_AURALINUX_H_

#include <string>

#include "base/component_export.h"

namespace ui {

enum class TextEditCommand;

// Represents a command that performs a specific operation on text.
// Copy and assignment are explicitly allowed; these objects live in vectors.
class COMPONENT_EXPORT(UI_BASE_IME_LINUX) TextEditCommandAuraLinux {
 public:
  TextEditCommandAuraLinux(TextEditCommand command, const std::string& argument)
      : command_(command), argument_(argument) {}

  TextEditCommand command() const { return command_; }
  const std::string& argument() const { return argument_; }

  // We communicate these commands back to blink with a string representation.
  std::string GetCommandString() const;

 private:
  TextEditCommand command_;

  // The text for TextEditCommand::INSERT_TEXT; otherwise empty and unused.
  std::string argument_;
};

}  // namespace ui

#endif  // UI_BASE_IME_LINUX_TEXT_EDIT_COMMAND_AURALINUX_H_
