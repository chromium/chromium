// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_INPUT_METHOD_DELEGATE_H_
#define UI_BASE_IME_ASH_INPUT_METHOD_DELEGATE_H_

#include <string>

namespace ash {
namespace input_method {

// Provides access to read/persist Input Method-related properties.
class InputMethodDelegate {
 public:
  InputMethodDelegate() = default;

  InputMethodDelegate(const InputMethodDelegate&) = delete;
  InputMethodDelegate& operator=(const InputMethodDelegate&) = delete;

  virtual ~InputMethodDelegate() = default;

  // Returns original VPD value.
  virtual std::string GetHardwareKeyboardLayouts() const = 0;

  // Retrieves localized string for |resource_id|.
  virtual std::u16string GetLocalizedString(int resource_id) const = 0;

  // Set hardware layout string for testting purpose.
  virtual void SetHardwareKeyboardLayoutForTesting(
      const std::string& layout) = 0;
};

}  // namespace input_method
}  // namespace ash

#endif  // UI_BASE_IME_ASH_INPUT_METHOD_DELEGATE_H_
