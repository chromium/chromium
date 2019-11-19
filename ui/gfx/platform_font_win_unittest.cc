// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_win.h"

#include <memory.h>
#include <string.h>
#include <windows.h>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/platform_font_skia.h"
#include "ui/gfx/win/scoped_set_map_mode.h"

namespace gfx {

// Test whether font metrics retrieved by DirectWrite (skia) and GDI match as
// per assumptions mentioned below:-
// 1. Font size is the same
// 2. The difference between GDI and DirectWrite for font height, baseline,
//    and cap height is at most 1. For smaller font sizes under 12, GDI
//    font heights/baselines/cap height are equal/larger by 1 point. For larger
//    font sizes DirectWrite font heights/baselines/cap height are equal/larger
//    by 1 point.
TEST(PlatformFontWinTest, Metrics_SkiaVersusGDI) {
  // Describes the font being tested.
  struct FontInfo {
    base::string16 font_name;
    int font_size;
  };

  FontInfo fonts[] = {
    {base::ASCIIToUTF16("Arial"), 6},
    {base::ASCIIToUTF16("Arial"), 8},
    {base::ASCIIToUTF16("Arial"), 10},
    {base::ASCIIToUTF16("Arial"), 12},
    {base::ASCIIToUTF16("Arial"), 16},
    {base::ASCIIToUTF16("Symbol"), 6},
    {base::ASCIIToUTF16("Symbol"), 10},
    {base::ASCIIToUTF16("Symbol"), 12},
    {base::ASCIIToUTF16("Tahoma"), 10},
    {base::ASCIIToUTF16("Tahoma"), 16},
    {base::ASCIIToUTF16("Segoe UI"), 6},
    {base::ASCIIToUTF16("Segoe UI"), 8},
    {base::ASCIIToUTF16("Segoe UI"), 20},
  };

  base::win::ScopedGetDC screen_dc(NULL);
  gfx::ScopedSetMapMode mode(screen_dc, MM_TEXT);

  for (const FontInfo& font : fonts) {
    LOGFONT font_info = {0};

    font_info.lfHeight = -font.font_size;
    font_info.lfWeight = FW_NORMAL;
    wcscpy_s(font_info.lfFaceName, font.font_name.length() + 1,
             font.font_name.c_str());

    HFONT hFont = CreateFontIndirect(&font_info);

    TEXTMETRIC font_metrics;
    PlatformFontWin::GetTextMetricsForFont(screen_dc, hFont, &font_metrics);

    scoped_refptr<PlatformFontWin::HFontRef> h_font_gdi(
        PlatformFontWin::CreateHFontRefFromGDI(hFont, font_metrics));

    scoped_refptr<PlatformFontWin::HFontRef> h_font_skia(
        PlatformFontWin::CreateHFontRefFromSkia(hFont, font_metrics));

    EXPECT_EQ(h_font_gdi->font_size(), h_font_skia->font_size());
    EXPECT_EQ(h_font_gdi->style(), h_font_skia->style());
    EXPECT_EQ(h_font_gdi->font_name(), h_font_skia->font_name());
    EXPECT_EQ(h_font_gdi->ave_char_width(), h_font_skia->ave_char_width());

    EXPECT_LE(abs(h_font_gdi->cap_height() - h_font_skia->cap_height()), 1);
    EXPECT_LE(abs(h_font_gdi->baseline() - h_font_skia->baseline()), 1);
    EXPECT_LE(abs(h_font_gdi->height() - h_font_skia->height()), 1);
  }
}

// Test if DirectWrite font fallback works correctly, i.e. whether DirectWrite
// fonts handle the font names in the
// HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\FontSubstitutes key
// correctly. The expectation is that the actual font created should be the
// one specified by the value for the substituted font. For e.g. MS Shell Dlg
// should create Microsoft Sans Serif, etc.
// For random fonts which are not substitutes, DirectWrite should fallback
// to Arial on a properly configured machine.
TEST(PlatformFontWinTest, DirectWriteFontSubstitution) {
  // Describes the font being tested.
  struct FontInfo {
    base::string16 font_name;
    std::string expected_font_name;
  };

  FontInfo fonts[] = {
    {base::ASCIIToUTF16("MS Shell Dlg"), "Microsoft Sans Serif"},
    {base::ASCIIToUTF16("MS Shell Dlg 2"), "Tahoma"},
    {base::ASCIIToUTF16("FooBar"), "Arial"},
  };

  base::win::ScopedGetDC screen_dc(NULL);
  gfx::ScopedSetMapMode mode(screen_dc, MM_TEXT);

  for (const FontInfo& font : fonts) {
    LOGFONT font_info = {0};

    font_info.lfHeight = -10;
    font_info.lfWeight = FW_NORMAL;
    wcscpy_s(font_info.lfFaceName, font.font_name.length() + 1,
             font.font_name.c_str());

    HFONT hFont = CreateFontIndirect(&font_info);

    TEXTMETRIC font_metrics;
    PlatformFontWin::GetTextMetricsForFont(screen_dc, hFont, &font_metrics);

    scoped_refptr<PlatformFontWin::HFontRef> h_font_skia(
        PlatformFontWin::CreateHFontRefFromSkia(hFont, font_metrics));

    EXPECT_EQ(font.expected_font_name, h_font_skia->font_name());
  }
}

// TODO(etienneb): Move this test to platform_font_skia_unittest when the
// font migration to skia font is completed.
TEST(PlatformFontWinTest, DefaultFontRenderParams) {
  scoped_refptr<PlatformFontSkia> default_font(new PlatformFontSkia());
  scoped_refptr<PlatformFontSkia> named_font(new PlatformFontSkia(
      default_font->GetFontName(), default_font->GetFontSize()));

  // Ensures that both constructors are producing fonts with the same render
  // params.
  EXPECT_EQ(default_font->GetFontRenderParams(),
            named_font->GetFontRenderParams());
}

TEST(PlatformFontWinTest, SkiaTypefaceConstructor) {
  gfx::Font default_font;

  // The PlatformFontWin constructor doesn't create a skia typeface.
  if (!base::FeatureList::IsEnabled(kPlatformFontSkiaOnWindows)) {
    EXPECT_EQ(default_font.platform_font()->GetNativeSkTypefaceIfAvailable(),
              nullptr);
  }

  sk_sp<SkFontMgr> font_mgr = SkFontMgr::RefDefault();
  sk_sp<SkTypeface> typeface(
      font_mgr->matchFamilyStyle("Segoe UI", SkFontStyle()));
  ASSERT_TRUE(typeface);
  gfx::Font fallback_font(new PlatformFontWin(typeface, 13, base::nullopt));
  EXPECT_EQ(fallback_font.platform_font()->GetNativeSkTypefaceIfAvailable(),
            typeface);
}

}  // namespace gfx
