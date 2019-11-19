// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/input_method_whitelist.h"

#include <stddef.h>

#include <vector>

#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chromeos/ime/input_methods.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"

namespace chromeos {
namespace input_method {

const char kLanguageDelimiter[] = ",";

InputMethodWhitelist::InputMethodWhitelist() {
  for (const auto& input_method : kInputMethods) {
    supported_input_methods_.insert(input_method.input_method_id);
  }
}

InputMethodWhitelist::~InputMethodWhitelist() = default;

bool InputMethodWhitelist::InputMethodIdIsWhitelisted(
    const std::string& input_method_id) const {
  return supported_input_methods_.count(input_method_id) > 0;
}

std::unique_ptr<InputMethodDescriptors>
InputMethodWhitelist::GetSupportedInputMethods() const {
  std::unique_ptr<InputMethodDescriptors> input_methods(
      new InputMethodDescriptors);
  input_methods->reserve(base::size(kInputMethods));
  for (const auto& input_method : kInputMethods) {
    std::vector<std::string> layouts;
    layouts.emplace_back(input_method.xkb_layout_id);

    std::vector<std::string> languages =
        base::SplitString(input_method.language_code, kLanguageDelimiter,
                          base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    DCHECK(!languages.empty());

    input_methods->push_back(InputMethodDescriptor(
        extension_ime_util::GetInputMethodIDByEngineID(
            input_method.input_method_id),
        "", input_method.indicator, layouts, languages,
        input_method.is_login_keyboard,
        GURL(),  // options page url.
        GURL()   // input view page url.
        ));
  }
  return input_methods;
}

}  // namespace input_method
}  // namespace chromeos
