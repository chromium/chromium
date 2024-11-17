// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_util_win.h"

#include <iterator>

#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/i18n.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/win/dpi.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/system_fonts_win.h"
#include "ui/strings/grit/app_locale_settings.h"

namespace {

class OverrideLocaleHolder {
 public:
  OverrideLocaleHolder() {}

  OverrideLocaleHolder(const OverrideLocaleHolder&) = delete;
  OverrideLocaleHolder& operator=(const OverrideLocaleHolder&) = delete;

  const std::vector<std::string>& value() const { return value_; }
  void swap_value(std::vector<std::string>* override_value) {
    value_.swap(*override_value);
  }
 private:
  std::vector<std::string> value_;
};

base::LazyInstance<OverrideLocaleHolder>::DestructorAtExit
    override_locale_holder = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace l10n_util {

void HWNDSetRTLLayout(HWND hwnd) {
  LONG ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);

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

void AdjustUiFont(gfx::win::FontAdjustment& font_adjustment) {
  // This is rather simple-minded to deal with the UI font size issue for some
  // Indian locales (ml, bn, hi) for which/ the default Windows fonts are too
  // small to be legible.  For those locales, IDS_UI_FONT_FAMILY is set to an
  // actual font family to use, while for other locales, it's set to "default".
  std::wstring ui_font_family = GetWideString(IDS_UI_FONT_FAMILY);
  if (!ui_font_family.empty()) {
    int scaler;
    if (base::StringToInt(l10n_util::GetStringUTF16(IDS_UI_FONT_SIZE_SCALER),
                          &scaler)) {
      if (const bool is_custom = ui_font_family != L"default";
          is_custom || scaler != 100) {
        if (is_custom) {
          font_adjustment.font_family_override.swap(ui_font_family);
        }
        font_adjustment.font_scale = scaler / 100.0;
      }
    }
  }
  font_adjustment.font_scale *= display::win::GetAccessibilityFontScale();
}

void OverrideLocaleWithUILanguageList() {
  std::vector<std::wstring> ui_languages;
  CHECK(base::win::i18n::GetThreadPreferredUILanguageList(&ui_languages));
  std::vector<std::string> ascii_languages;
  ascii_languages.reserve(ui_languages.size());
  base::ranges::transform(ui_languages, std::back_inserter(ascii_languages),
                          &base::WideToASCII);
  override_locale_holder.Get().swap_value(&ascii_languages);
}

const std::vector<std::string>& GetLocaleOverrides() {
  return override_locale_holder.Get().value();
}

std::wstring GetWideString(int message_id) {
  return base::UTF16ToWide(GetStringUTF16(message_id));
}

}  // namespace l10n_util
