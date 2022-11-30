// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/menu_label_accelerator_util.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"

namespace ui {

char16_t GetMnemonic(const std::u16string& label) {
  size_t index = 0;
  do {
    index = label.find('&', index);
    if (index != std::u16string::npos) {
      if (index + 1 != label.size()) {
        if (label[index + 1] != '&') {
          char16_t char_array[] = {label[index + 1], 0};
          // TODO(jshin): What about Turkish locale? See http://crbug.com/81719.
          // If the mnemonic is capital I and the UI language is Turkish,
          // lowercasing it results in 'small dotless i', which is different
          // from a 'dotted i'. Similar issues may exist for az and lt locales.
          return base::i18n::ToLower(char_array)[0];
        } else {
          index++;
        }
      }
      index++;
    }
  } while (index != std::u16string::npos);
  return 0;
}

std::u16string EscapeMenuLabelAmpersands(const std::u16string& label) {
  std::u16string ret;
  static const char16_t kAmps[] = {'&', 0};
  static const char16_t kTwoAmps[] = {'&', '&', 0};
  base::ReplaceChars(label, kAmps, kTwoAmps, &ret);
  return ret;
}

}  // namespace ui
