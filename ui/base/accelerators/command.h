// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_COMMAND_H_
#define UI_BASE_ACCELERATORS_COMMAND_H_

#include <map>
#include <string>
#include <string_view>

#include "base/values.h"
#include "ui/base/accelerators/accelerator.h"

namespace ui {

class COMPONENT_EXPORT(UI_BASE) Command {
 public:
  Command() = default;
  Command(std::string_view command_name,
          std::u16string_view description,
          bool global);
  Command(const Command& other) = default;
  virtual ~Command() = default;

  // Accessors:
  const std::string& command_name() const { return command_name_; }
  const ui::Accelerator& accelerator() const { return accelerator_; }
  const std::u16string& description() const { return description_; }
  bool global() const { return global_; }

  // Setter:
  void set_command_name(std::string_view command_name) {
    command_name_ = command_name;
  }
  void set_accelerator(const ui::Accelerator& accelerator) {
    accelerator_ = accelerator;
  }
  void set_description(std::u16string_view description) {
    description_ = description;
  }
  void set_global(bool global) { global_ = global; }

 private:
  std::string command_name_;
  ui::Accelerator accelerator_;
  std::u16string description_;
  bool global_ = false;
};

// A mapping of command name (std::string) to a command object.
using CommandMap = std::map<std::string, Command>;

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_COMMAND_H_
