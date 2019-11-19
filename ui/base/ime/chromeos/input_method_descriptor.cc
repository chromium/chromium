// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/input_method_descriptor.h"

#include <sstream>

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "url/gurl.h"

namespace chromeos {
namespace input_method {

InputMethodDescriptor::InputMethodDescriptor(
    const std::string& id,
    const std::string& name,
    const std::string& indicator,
    const std::vector<std::string>& keyboard_layouts,
    const std::vector<std::string>& language_codes,
    bool is_login_keyboard,
    const GURL& options_page_url,
    const GURL& input_view_url)
    : id_(id),
      name_(name),
      keyboard_layouts_(keyboard_layouts),
      language_codes_(language_codes),
      indicator_(indicator),
      is_login_keyboard_(is_login_keyboard),
      options_page_url_(options_page_url),
      input_view_url_(input_view_url) {
}

InputMethodDescriptor::InputMethodDescriptor(
    const InputMethodDescriptor& other) = default;

std::string InputMethodDescriptor::GetPreferredKeyboardLayout() const {
  // TODO(nona): Investigate better way to guess the preferred layout
  //             http://crbug.com/170601.
  return keyboard_layouts_.empty() ? "us" : keyboard_layouts_[0];
}

std::string InputMethodDescriptor::GetIndicator() const {
  // Return the empty string for ARC IMEs.
  if (extension_ime_util::IsArcIME(id_))
    return std::string();

  // If indicator is empty, use the first two character in its preferred
  // keyboard layout or language code.
  if (indicator_.empty()) {
    if (extension_ime_util::IsKeyboardLayoutExtension(id_)) {
      return base::ToUpperASCII(GetPreferredKeyboardLayout().substr(0, 2));
    }
    DCHECK(language_codes_.size() > 0);
    return base::ToUpperASCII(language_codes_[0].substr(0, 2));
  }
  return indicator_;
}

InputMethodDescriptor::InputMethodDescriptor() = default;

InputMethodDescriptor::~InputMethodDescriptor() = default;

}  // namespace input_method
}  // namespace chromeos
