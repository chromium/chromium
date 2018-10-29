// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_util_win.h"

#include <algorithm>
#include <iterator>

#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/i18n.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/win/screen_win.h"
#include "ui/strings/grit/app_locale_settings.h"

namespace {

class OverrideLocaleHolder {
 public:
  OverrideLocaleHolder() {}
  const std::vector<std::string>& value() const { return value_; }
  void swap_value(std::vector<std::string>* override_value) {
    value_.swap(*override_value);
  }
 private:
  std::vector<std::string> value_;
  DISALLOW_COPY_AND_ASSIGN(OverrideLocaleHolder);
};

base::LazyInstance<OverrideLocaleHolder>::DestructorAtExit
    override_locale_holder = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace l10n_util {

int GetExtendedStyles() {
  return !base::i18n::IsRTL() ? 0 : WS_EX_LAYOUTRTL | WS_EX_RTLREADING;
}

int GetExtendedTooltipStyles() {
  return !base::i18n::IsRTL() ? 0 : WS_EX_LAYOUTRTL;
}

void HWNDSetRTLLayout(HWND hwnd) {
  DWORD ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);

  // We don't have to do anything if the style is already set for the HWND.
  if (!(ex_style & WS_EX_LAYOUTRTL)) {
    ex_style |= WS_EX_LAYOUTRTL;
    ::SetWindowLong(hwnd, GWL_EXSTYLE, ex_style);

    // Right-to-left layout changes are not applied to the window immediately
    // so we should make sure a WM_PAINT is sent to the window by invalidating
    // the entire window rect.
    ::InvalidateRect(hwnd, NULL, true);
  }
}

bool IsLocaleSupportedByOS(const std::string& locale) {
  return true;
}

bool NeedOverrideDefaultUIFont(base::string16* override_font_family,
                               double* font_size_scaler) {
  // This is rather simple-minded to deal with the UI font size
  // issue for some Indian locales (ml, bn, hi) for which
  // the default Windows fonts are too small to be legible.  For those
  // locales, IDS_UI_FONT_FAMILY is set to an actual font family to
  // use while for other locales, it's set to 'default'.
  base::string16 ui_font_family = GetStringUTF16(IDS_UI_FONT_FAMILY);
  int scaler100;
  if (!base::StringToInt(l10n_util::GetStringUTF16(IDS_UI_FONT_SIZE_SCALER),
                         &scaler100))
    return false;

  // We use the OS default in two cases:
  // 1) The resource bundle has 'default' and '100' for font family and
  //    font scaler.
  // 2) The resource bundle is not available for some reason and
  //    ui_font_family is empty.
  if ((ui_font_family == L"default" && scaler100 == 100) ||
      ui_font_family.empty())
    return false;

  if (override_font_family && ui_font_family != L"default")
    override_font_family->swap(ui_font_family);
  if (font_size_scaler)
    *font_size_scaler = scaler100 / 100.0;
  return true;
}

void OverrideLocaleWithUILanguageList() {
  std::vector<base::string16> ui_languages;
  if (base::win::i18n::GetThreadPreferredUILanguageList(&ui_languages)) {
    std::vector<std::string> ascii_languages;
    ascii_languages.reserve(ui_languages.size());
    std::transform(ui_languages.begin(), ui_languages.end(),
                   std::back_inserter(ascii_languages), &base::UTF16ToASCII);
    override_locale_holder.Get().swap_value(&ascii_languages);
  } else {
    NOTREACHED() << "Failed to determine the UI language for locale override.";
  }
}

const std::vector<std::string>& GetLocaleOverrides() {
  return override_locale_holder.Get().value();
}

}  // namespace l10n_util
