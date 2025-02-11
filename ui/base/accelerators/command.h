// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_COMMAND_H_
#define UI_BASE_ACCELERATORS_COMMAND_H_

#include <map>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "ui/base/accelerators/accelerator.h"

namespace ui {

// Denotes the reason why parsing a string to a valid global accelerator fails.
enum class AcceleratorParseError {
  // The platform key used must be for a known platform.
  kUnsupportedPlatform,
  // The string sent to the parser cannot be parsed into a valid global
  // accelerator.
  kMalformedInput,
  // Media keys may not have modifiers.
  kMediaKeyWithModifier
};

class COMPONENT_EXPORT(UI_BASE) Command {
 public:
  using AcceleratorParseErrorCallback =
      base::OnceCallback<void(ui::AcceleratorParseError)>;

  Command() = default;
  Command(std::string_view command_name,
          std::u16string_view description,
          bool global);
  Command(const Command& other) = default;
  virtual ~Command() = default;

  // The platform value for the Command.
  static std::string CommandPlatform();

  // Parse a string as an accelerator. If the accelerator is unparsable then
  // a generic ui::Accelerator object will be returns (with key_code Unknown).
  static ui::Accelerator StringToAccelerator(std::string_view accelerator);

  // Returns the string representation of an accelerator without localizing the
  // shortcut text (like accelerator::GetShortcutText() does).
  static std::string AcceleratorToString(const ui::Accelerator& accelerator);

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

 protected:
  // Parse an |accelerator| for a given platform (specified by |platform_key|)
  // and return the result as a ui::Accelerator if successful, or VKEY_UNKNOWN
  // if not. |should_parse_media_keys| specifies whether media keys are to be
  // considered for parsing. |error_callback| is called when there is an issue
  // with parsing |accelerator| and the reason why parsing failed is passed in.
  // |allow_ctrl_alt| parses the accelerator even if the accelerator contains
  // the control and alt (command and option) key combination. Usually this is
  // not preferred because that key combination produces a special key in
  // certain languages. However, this restriction may be loosened if the user
  // is aware and explicitly binds this key combination. See article for more
  // information:
  // https://devblogs.microsoft.com/oldnewthing/20040329-00/?p=40003
  // Note: If the parsing rules here are changed, make sure to
  // update the corresponding shortcut_input.ts validation, which validates the
  // user input for the CrShortcutInput WebUI component.
  static ui::Accelerator ParseImpl(std::string_view accelerator,
                                   std::string_view platform_key,
                                   bool should_parse_media_keys,
                                   AcceleratorParseErrorCallback error_callback,
                                   bool allow_ctrl_alt = false);

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
