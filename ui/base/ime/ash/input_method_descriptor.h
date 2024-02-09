// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_INPUT_METHOD_DESCRIPTOR_H_
#define UI_BASE_IME_ASH_INPUT_METHOD_DESCRIPTOR_H_

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {

// A structure which represents an input method.
class COMPONENT_EXPORT(UI_BASE_IME_ASH) InputMethodDescriptor {
 public:
  InputMethodDescriptor();
  InputMethodDescriptor(const std::string& id,
                        const std::string& name,
                        const std::string& indicator,
                        const std::string& keyboard_layout,
                        const std::vector<std::string>& language_codes,
                        bool is_login_keyboard,
                        const GURL& options_page_url,
                        const GURL& input_view_url,
                        const std::optional<std::string>& handwriting_language);
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
  const std::string& keyboard_layout() const { return keyboard_layout_; }
  bool is_login_keyboard() const { return is_login_keyboard_; }
  const std::optional<std::string>& handwriting_language() const {
    return handwriting_language_;
  }

  std::u16string GetIndicator() const;

 private:
  // An ID that identifies an input method engine (e.g., "t:latn-post",
  // "pinyin", "hangul").
  std::string id_;

  // A name used to specify the user-visible name of this input method.  It is
  // only used by extension IMEs, and should be blank for internal IMEs.
  std::string name_;

  // A physical keyboard XKB layout for the input method (e.g., "us",
  // "us(dvorak)", "jp"). Comma separated layout names do NOT appear.
  std::string keyboard_layout_;

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

  // An ID that identifies a handwriting model language ID for this input
  // method, like "en" or "ja".
  // This field is valid only for 1P Google ChromeOS input methods.
  std::optional<std::string> handwriting_language_;
};

using InputMethodDescriptors = std::vector<InputMethodDescriptor>;

}  // namespace input_method
}  // namespace ash

#endif  // UI_BASE_IME_ASH_INPUT_METHOD_DESCRIPTOR_H_
