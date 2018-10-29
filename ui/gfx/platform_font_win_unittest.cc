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
#include "ui/gfx/font.h"
#include "ui/gfx/win/direct_write.h"
#include "ui/gfx/win/scoped_set_map_mode.h"

namespace gfx {

TEST(PlatformFontWinTest, AdjustFontSize) {
  PlatformFontWin::SetGetMinimumFontSizeCallback(nullptr);
  EXPECT_EQ(10, PlatformFontWin::AdjustFontSize(10, 0));
  EXPECT_EQ(-10, PlatformFontWin::AdjustFontSize(-10, 0));
  EXPECT_EQ(8, PlatformFontWin::AdjustFontSize(10, -2));
  EXPECT_EQ(-8, PlatformFontWin::AdjustFontSize(-10, -2));
  EXPECT_EQ(13, PlatformFontWin::AdjustFontSize(10, 3));
  EXPECT_EQ(-13, PlatformFontWin::AdjustFontSize(-10, 3));
  EXPECT_EQ(1, PlatformFontWin::AdjustFontSize(10, -9));
  EXPECT_EQ(-1, PlatformFontWin::AdjustFontSize(-10, -9));
  EXPECT_EQ(0, PlatformFontWin::AdjustFontSize(10, -12));
  EXPECT_EQ(0, PlatformFontWin::AdjustFontSize(-10, -12));
}

TEST(PlatformFontWinTest, AdjustFontSize_MinimumSizeSpecified) {
  PlatformFontWin::SetGetMinimumFontSizeCallback([] { return 1; });
  EXPECT_EQ(10, PlatformFontWin::AdjustFontSize(10, 0));
  EXPECT_EQ(-10, PlatformFontWin::AdjustFontSize(-10, 0));
  EXPECT_EQ(8, PlatformFontWin::AdjustFontSize(10, -2));
  EXPECT_EQ(-8, PlatformFontWin::AdjustFontSize(-10, -2));
  EXPECT_EQ(13, PlatformFontWin::AdjustFontSize(10, 3));
  EXPECT_EQ(-13, PlatformFontWin::AdjustFontSize(-10, 3));
  EXPECT_EQ(1, PlatformFontWin::AdjustFontSize(10, -9));
  EXPECT_EQ(-1, PlatformFontWin::AdjustFontSize(-10, -9));
  EXPECT_EQ(1, PlatformFontWin::AdjustFontSize(10, -12));
  EXPECT_EQ(-1, PlatformFontWin::AdjustFontSize(-10, -12));
}

namespace {

LOGFONT CreateLOGFONT(const base::string16& name, LONG height) {
  LOGFONT logfont{};
  logfont.lfHeight = height;
  auto result = wcscpy_s(logfont.lfFaceName, name.c_str());
  DCHECK_EQ(0, result);
  return logfont;
}

const base::string16 kSegoeUI(L"Segoe UI");
const base::string16 kArial(L"Arial");

}  // namespace

TEST(PlatformFontWinTest, AdjustLOGFONT_NoAdjustment) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -12);
  PlatformFontWin::FontAdjustment adjustment;
  PlatformFontWin::AdjustLOGFONT(adjustment, &logfont);
  EXPECT_EQ(-12, logfont.lfHeight);
  EXPECT_EQ(kSegoeUI, logfont.lfFaceName);
}

TEST(PlatformFontWinTest, AdjustLOGFONT_ChangeFace) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -12);
  PlatformFontWin::FontAdjustment adjustment{kArial, 1.0};
  PlatformFontWin::AdjustLOGFONT(adjustment, &logfont);
  EXPECT_EQ(-12, logfont.lfHeight);
  EXPECT_EQ(kArial, logfont.lfFaceName);
}

TEST(PlatformFontWinTest, AdjustLOGFONT_ScaleDown) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -12);
  PlatformFontWin::FontAdjustment adjustment{L"", 0.5};
  PlatformFontWin::AdjustLOGFONT(adjustment, &logfont);
  EXPECT_EQ(-6, logfont.lfHeight);
  EXPECT_EQ(kSegoeUI, logfont.lfFaceName);

  logfont = CreateLOGFONT(kSegoeUI, 12);
  adjustment = {L"", 0.5};
  PlatformFontWin::AdjustLOGFONT(adjustment, &logfont);
  EXPECT_EQ(6, logfont.lfHeight);
  EXPECT_EQ(kSegoeUI, logfont.lfFaceName);
}

TEST(PlatformFontWinTest, AdjustLOGFONT_ScaleDownWithRounding) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -10);
  PlatformFontWin::FontAdjustment adjustment{L"", 0.85};
  PlatformFontWin::AdjustLOGFONT(adjustment, &logfont);
  EXPECT_EQ(-9, logfont.lfHeight);
  EXPECT_EQ(kSegoeUI, logfont.lfFaceName);

  logfont = CreateLOGFONT(kSegoeUI, 10);
  adjustment = {L"", 0.85};
  PlatformFontWin::AdjustLOGFONT(adjustment, &logfont);
  EXPECT_EQ(9, logfont.lfHeight);
  EXPECT_EQ(kSegoeUI, logfont.lfFaceName);
}

TEST(PlatformFontWinTest, AdjustLOGFONT_ScaleUpWithFaceChange) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -12);
  PlatformFontWin::FontAdjustment adjustment{kArial, 1.5};
  PlatformFontWin::AdjustLOGFONT(adjustment, &logfont);
  EXPECT_EQ(-18, logfont.lfHeight);
  EXPECT_EQ(kArial, logfont.lfFaceName);

  logfont = CreateLOGFONT(kSegoeUI, 12);
  adjustment = {kArial, 1.5};
  PlatformFontWin::AdjustLOGFONT(adjustment, &logfont);
  EXPECT_EQ(18, logfont.lfHeight);
  EXPECT_EQ(kArial, logfont.lfFaceName);
}

TEST(PlatformFontWinTest, AdjustLOGFONT_ScaleUpWithRounding) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -10);
  PlatformFontWin::FontAdjustment adjustment{L"", 1.111};
  PlatformFontWin::AdjustLOGFONT(adjustment, &logfont);
  EXPECT_EQ(-11, logfont.lfHeight);
  EXPECT_EQ(kSegoeUI, logfont.lfFaceName);

  logfont = CreateLOGFONT(kSegoeUI, 10);
  adjustment = {L"", 1.11};
  PlatformFontWin::AdjustLOGFONT(adjustment, &logfont);
  EXPECT_EQ(11, logfont.lfHeight);
  EXPECT_EQ(kSegoeUI, logfont.lfFaceName);
}

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

}  // namespace gfx
