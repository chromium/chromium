// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/system_fonts_win.h"

#include <windows.h>

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {
namespace win {

namespace {

class SystemFontsWinTest : public testing::Test {
 public:
  SystemFontsWinTest() = default;

  SystemFontsWinTest(const SystemFontsWinTest&) = delete;
  SystemFontsWinTest& operator=(const SystemFontsWinTest&) = delete;

 protected:
  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    // System fonts is keeping a cache of loaded system fonts. These fonts are
    // scaled based on global callbacks configured on startup. The tests in this
    // file are testing these callbacks and need to be sure we cleared the
    // global state to avoid flaky tests.
    win::ResetSystemFontsForTesting();
#endif
  }
};

LOGFONT CreateLOGFONT(const wchar_t* name, LONG height) {
  LOGFONT logfont = {};
  logfont.lfHeight = height;
  auto result = wcscpy_s(logfont.lfFaceName, name);
  DCHECK_EQ(0, result);
  return logfont;
}

const wchar_t kSegoeUI[] = L"Segoe UI";
const wchar_t kArial[] = L"Arial";

}  // namespace

TEST_F(SystemFontsWinTest, AdjustFontSize) {
  EXPECT_EQ(10, gfx::win::AdjustFontSize(10, 0));
  EXPECT_EQ(-10, gfx::win::AdjustFontSize(-10, 0));
  EXPECT_EQ(8, gfx::win::AdjustFontSize(10, -2));
  EXPECT_EQ(-8, gfx::win::AdjustFontSize(-10, -2));
  EXPECT_EQ(13, gfx::win::AdjustFontSize(10, 3));
  EXPECT_EQ(-13, gfx::win::AdjustFontSize(-10, 3));
  EXPECT_EQ(1, gfx::win::AdjustFontSize(10, -9));
  EXPECT_EQ(-1, gfx::win::AdjustFontSize(-10, -9));
  EXPECT_EQ(0, gfx::win::AdjustFontSize(10, -12));
  EXPECT_EQ(0, gfx::win::AdjustFontSize(-10, -12));
}

TEST_F(SystemFontsWinTest, AdjustFontSize_MinimumSizeSpecified) {
  gfx::win::SetGetMinimumFontSizeCallback([] { return 1; });
  EXPECT_EQ(10, gfx::win::AdjustFontSize(10, 0));
  EXPECT_EQ(-10, gfx::win::AdjustFontSize(-10, 0));
  EXPECT_EQ(8, gfx::win::AdjustFontSize(10, -2));
  EXPECT_EQ(-8, gfx::win::AdjustFontSize(-10, -2));
  EXPECT_EQ(13, gfx::win::AdjustFontSize(10, 3));
  EXPECT_EQ(-13, gfx::win::AdjustFontSize(-10, 3));
  EXPECT_EQ(1, gfx::win::AdjustFontSize(10, -9));
  EXPECT_EQ(-1, gfx::win::AdjustFontSize(-10, -9));
  EXPECT_EQ(1, gfx::win::AdjustFontSize(10, -12));
  EXPECT_EQ(-1, gfx::win::AdjustFontSize(-10, -12));
}

TEST_F(SystemFontsWinTest, AdjustLOGFONT_NoAdjustment) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -12);
  FontAdjustment adjustment;
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(-12, logfont.lfHeight);
  EXPECT_STREQ(kSegoeUI, logfont.lfFaceName);
}

TEST_F(SystemFontsWinTest, AdjustLOGFONT_ChangeFace) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -12);
  FontAdjustment adjustment{kArial, 1.0};
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(-12, logfont.lfHeight);
  EXPECT_STREQ(kArial, logfont.lfFaceName);
}

TEST_F(SystemFontsWinTest, AdjustLOGFONT_ScaleDown) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -12);
  FontAdjustment adjustment{L"", 0.5};
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(-6, logfont.lfHeight);
  EXPECT_STREQ(kSegoeUI, logfont.lfFaceName);

  logfont = CreateLOGFONT(kSegoeUI, 12);
  adjustment = {L"", 0.5};
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(6, logfont.lfHeight);
  EXPECT_STREQ(kSegoeUI, logfont.lfFaceName);
}

TEST_F(SystemFontsWinTest, AdjustLOGFONT_ScaleDownWithRounding) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -10);
  FontAdjustment adjustment{L"", 0.85};
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(-9, logfont.lfHeight);
  EXPECT_STREQ(kSegoeUI, logfont.lfFaceName);

  logfont = CreateLOGFONT(kSegoeUI, 10);
  adjustment = {L"", 0.85};
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(9, logfont.lfHeight);
  EXPECT_STREQ(kSegoeUI, logfont.lfFaceName);
}

TEST_F(SystemFontsWinTest, AdjustLOGFONT_ScaleUpWithFaceChange) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -12);
  FontAdjustment adjustment{kArial, 1.5};
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(-18, logfont.lfHeight);
  EXPECT_STREQ(kArial, logfont.lfFaceName);

  logfont = CreateLOGFONT(kSegoeUI, 12);
  adjustment = {kArial, 1.5};
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(18, logfont.lfHeight);
  EXPECT_STREQ(kArial, logfont.lfFaceName);
}

TEST_F(SystemFontsWinTest, AdjustLOGFONT_ScaleUpWithRounding) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -10);
  FontAdjustment adjustment{L"", 1.111};
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(-11, logfont.lfHeight);
  EXPECT_STREQ(kSegoeUI, logfont.lfFaceName);

  logfont = CreateLOGFONT(kSegoeUI, 10);
  adjustment = {L"", 1.11};
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(11, logfont.lfHeight);
  EXPECT_STREQ(kSegoeUI, logfont.lfFaceName);
}

TEST_F(SystemFontsWinTest, GetFontFromLOGFONT) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -10);
  Font font = GetFontFromLOGFONTForTesting(logfont);
  EXPECT_EQ(font.GetStyle(), Font::FontStyle::NORMAL);
  EXPECT_EQ(font.GetWeight(), Font::Weight::NORMAL);
}

TEST_F(SystemFontsWinTest, GetFontFromLOGFONT_WithStyle) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -10);
  logfont.lfItalic = 1;
  logfont.lfWeight = 700;

  Font font = GetFontFromLOGFONTForTesting(logfont);
  EXPECT_EQ(font.GetStyle(), Font::FontStyle::ITALIC);
  EXPECT_EQ(font.GetWeight(), Font::Weight::BOLD);
}

TEST_F(SystemFontsWinTest, GetDefaultSystemFont) {
  Font system_font = GetDefaultSystemFont();
  EXPECT_EQ(base::WideToUTF8(kSegoeUI), system_font.GetFontName());
}

}  // namespace win
}  // namespace gfx
