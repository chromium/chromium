// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/direct_write.h"

#include "base/i18n/rtl.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(DirectWrite, RetrieveLocalizedFontName) {
  // Retrieve the en-US localized names.
  EXPECT_EQ(gfx::win::RetrieveLocalizedFontName("MS Gothic", "en-US"),
            "MS Gothic");
  EXPECT_EQ(gfx::win::RetrieveLocalizedFontName("Malgun Gothic", "en-US"),
            "Malgun Gothic");

  // Retrieve the localized names.
  EXPECT_EQ(gfx::win::RetrieveLocalizedFontName("MS Gothic", "ja-JP"),
            "ＭＳ ゴシック");
  EXPECT_EQ(gfx::win::RetrieveLocalizedFontName("Malgun Gothic", "ko-KR"),
            "맑은 고딕");

  // Retrieve the default font name.
  EXPECT_EQ(gfx::win::RetrieveLocalizedFontName("ＭＳ ゴシック", ""),
            "MS Gothic");
  EXPECT_EQ(gfx::win::RetrieveLocalizedFontName("맑은 고딕", ""),
            "Malgun Gothic");
}
