// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/input_method_descriptor.h"

#include <optional>
#include <sstream>

#include "base/check.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {

InputMethodDescriptor::InputMethodDescriptor(
    const std::string& id,
    const std::string& name,
    const std::string& indicator,
    const std::string& keyboard_layout,
    const std::vector<std::string>& language_codes,
    bool is_login_keyboard,
    const GURL& options_page_url,
    const GURL& input_view_url,
    const std::optional<std::string>& handwriting_language)
    : id_(id),
      name_(name),
      keyboard_layout_(keyboard_layout),
      language_codes_(language_codes),
      indicator_(indicator),
      is_login_keyboard_(is_login_keyboard),
      options_page_url_(options_page_url),
      input_view_url_(input_view_url),
      handwriting_language_(handwriting_language) {}

InputMethodDescriptor::InputMethodDescriptor(
    const InputMethodDescriptor& other) = default;

std::u16string InputMethodDescriptor::GetIndicator() const {
  // Return the empty string for ARC IMEs.
  if (extension_ime_util::IsArcIME(id_)) {
    return std::u16string();
  }

  // If indicator is empty, use the first two character in its keyboard layout
  // or language code.
  if (indicator_.empty()) {
    if (extension_ime_util::IsKeyboardLayoutExtension(id_)) {
      return base::UTF8ToUTF16(
          base::ToUpperASCII(keyboard_layout_.substr(0, 2)));
    }
    DCHECK(language_codes_.size() > 0);
    return base::UTF8ToUTF16(
        base::ToUpperASCII(language_codes_[0].substr(0, 2)));
  }
  return base::UTF8ToUTF16(indicator_);
}

InputMethodDescriptor::InputMethodDescriptor() = default;

InputMethodDescriptor::~InputMethodDescriptor() = default;

}  // namespace input_method
}  // namespace ash
