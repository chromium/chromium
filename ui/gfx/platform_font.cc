// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font.h"

#include "ui/gfx/font_list.h"

namespace gfx {

// static
int PlatformFont::GetFontSizeDeltaIgnoringUserOrLocaleSettings(
    int desired_font_size) {
  int size_delta = desired_font_size - gfx::PlatformFont::kDefaultBaseFontSize;
  gfx::FontList base_font = gfx::FontList().DeriveWithSizeDelta(size_delta);

  // The default font may not actually be kDefaultBaseFontSize if, for example,
  // the user has changed their system font sizes or the current locale has been
  // overridden to use a different default font size. Adjust for the difference
  // in default font sizes.
  int user_or_locale_delta = 0;
  if (base_font.GetFontSize() != desired_font_size) {
    user_or_locale_delta = desired_font_size - base_font.GetFontSize();
    base_font =
        gfx::FontList().DeriveWithSizeDelta(size_delta + user_or_locale_delta);
  }
  DCHECK_EQ(desired_font_size, base_font.GetFontSize());

  // To ensure a subsequent request from the ResourceBundle ignores the delta
  // due to user or locale settings, include it here.
  return base_font.GetFontSize() - gfx::PlatformFont::kDefaultBaseFontSize +
         user_or_locale_delta;
}

}  // namespace gfx
