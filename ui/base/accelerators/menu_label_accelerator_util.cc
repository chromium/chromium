// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/menu_label_accelerator_util.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"

namespace ui {

base::char16 GetMnemonic(const base::string16& label) {
  size_t index = 0;
  do {
    index = label.find('&', index);
    if (index != base::string16::npos) {
      if (index + 1 != label.size()) {
        if (label[index + 1] != '&') {
          base::char16 char_array[] = {label[index + 1], 0};
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
  } while (index != base::string16::npos);
  return 0;
}

base::string16 EscapeMenuLabelAmpersands(const base::string16& label) {
  base::string16 ret;
  static const base::char16 kAmps[] = {'&', 0};
  static const base::char16 kTwoAmps[] = {'&', '&', 0};
  base::ReplaceChars(label, kAmps, kTwoAmps, &ret);
  return ret;
}

}  // namespace ui
