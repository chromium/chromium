// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_INPUT_METHOD_DESCRIPTOR_H_
#define UI_BASE_IME_CHROMEOS_INPUT_METHOD_DESCRIPTOR_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "url/gurl.h"

namespace chromeos {
namespace input_method {

// A structure which represents an input method.
class COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) InputMethodDescriptor {
 public:
  InputMethodDescriptor();
  InputMethodDescriptor(const std::string& id,
                        const std::string& name,
                        const std::string& indicator,
                        const std::vector<std::string>& keyboard_layouts,
                        const std::vector<std::string>& language_codes,
                        bool is_login_keyboard,
                        const GURL& options_page_url,
                        const GURL& input_view_url);
  InputMethodDescriptor(const InputMethodDescriptor& other);
  ~InputMethodDescriptor();

  // Accessors
  const std::string& id() const { return id_; }
  const std::string& name() const { return name_; }
  const std::string& indicator() const { return indicator_; }
  const std::vector<std::string>& language_codes() const {
    return language_codes_;
  }
  const GURL& options_page_url() const { return options_page_url_; }
  const GURL& input_view_url() const { return input_view_url_; }
  const std::vector<std::string>& keyboard_layouts() const {
    return keyboard_layouts_;
  }

  bool is_login_keyboard() const { return is_login_keyboard_; }

  // Returns preferred keyboard layout.
  std::string GetPreferredKeyboardLayout() const;

  // Returns the indicator text of this input method.
  std::string GetIndicator() const;

 private:
  // An ID that identifies an input method engine (e.g., "t:latn-post",
  // "pinyin", "hangul").
  std::string id_;

  // A name used to specify the user-visible name of this input method.  It is
  // only used by extension IMEs, and should be blank for internal IMEs.
  std::string name_;

  // A preferred physical keyboard layout for the input method (e.g., "us",
  // "us(dvorak)", "jp"). Comma separated layout names do NOT appear.
  std::vector<std::string> keyboard_layouts_;

  // Language code like "ko", "ja", "en-US", and "zh-CN".
  std::vector<std::string> language_codes_;

  // A short indicator string that is displayed when the input method
  // is selected, like "US".
  std::string indicator_;

  // True if this input method can be used on login screen.
  bool is_login_keyboard_;

  // Options page URL e.g.
  // "chrome-extension://ceaajjmckiakobniehbjpdcidfpohlin/options.html".
  // This field is valid only for input method extension.
  GURL options_page_url_;

  // Input View URL e.g.
  // "chrome-extension://ceaajjmckiakobniehbjpdcidfpohlin/my_input_view.html".
  // This field is valid only for input method extension.
  GURL input_view_url_;
};

typedef std::vector<InputMethodDescriptor> InputMethodDescriptors;

}  // namespace input_method
}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_INPUT_METHOD_DESCRIPTOR_H_
