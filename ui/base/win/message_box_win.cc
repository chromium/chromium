// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/message_box_win.h"

#include <string>

#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace ui {

// In addition to passing the RTL flags to ::MessageBox if we are running in an
// RTL locale, we need to make sure that LTR strings are rendered correctly by
// adding the appropriate Unicode directionality marks.
int MessageBox(HWND hwnd,
               const std::wstring& text,
               const std::wstring& caption,
               UINT flags) {
  UINT actual_flags = flags;
  if (base::i18n::IsRTL())
    actual_flags |= MB_RIGHT | MB_RTLREADING;

  std::u16string localized_text = base::WideToUTF16(text);
  base::i18n::AdjustStringForLocaleDirection(&localized_text);
  const wchar_t* text_ptr = base::as_wcstr(localized_text);

  std::u16string localized_caption = base::WideToUTF16(caption);
  base::i18n::AdjustStringForLocaleDirection(&localized_caption);
  const wchar_t* caption_ptr = base::as_wcstr(localized_caption);

  return ::MessageBox(hwnd, text_ptr, caption_ptr, actual_flags);
}

}  // namespace ui
