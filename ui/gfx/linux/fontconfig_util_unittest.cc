// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/fontconfig_util.h"

#include <fontconfig/fontconfig.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

TEST(FontConfigUtilTest, FcPatternAccessors) {
  ScopedFcPattern pattern(FcPatternCreate());

  const char kFamilyName[] = "sans";
  FcPatternAddString(pattern.get(), FC_FAMILY,
                     reinterpret_cast<const FcChar8*>(kFamilyName));
  const char kFileName[] = "/usr/share/fonts/arial.ttf";
  FcPatternAddString(pattern.get(), FC_FILE,
                     reinterpret_cast<const FcChar8*>(kFileName));
  const int kIndex = 42;
  FcPatternAddInteger(pattern.get(), FC_INDEX, kIndex);
  FcPatternAddInteger(pattern.get(), FC_WEIGHT, FC_WEIGHT_BOLD);
  FcPatternAddInteger(pattern.get(), FC_SLANT, FC_SLANT_ROMAN);
  FcPatternAddBool(pattern.get(), FC_SCALABLE, FcTrue);
  const char kFontFormat[] = "TrueType";
  FcPatternAddString(pattern.get(), FC_FONTFORMAT,
                     reinterpret_cast<const FcChar8*>(kFontFormat));

  EXPECT_EQ(kFamilyName, GetFontName(pattern.get()));
  EXPECT_EQ(kFileName, GetFilename(pattern.get()));
  EXPECT_EQ(kIndex, GetFontTtcIndex(pattern.get()));
  EXPECT_TRUE(IsFontBold(pattern.get()));
  EXPECT_FALSE(IsFontItalic(pattern.get()));
  EXPECT_TRUE(IsFontScalable(pattern.get()));
  EXPECT_EQ(kFontFormat, GetFontFormat(pattern.get()));
}

TEST(FontConfigUtilTest, GetFontPathWithSysRoot) {
  ScopedFcPattern pattern(FcPatternCreate());

  // Save the old sysroot, if specified.
  std::string old_sysroot;
  const FcChar8* old_sysroot_ptr = FcConfigGetSysRoot(nullptr);
  if (old_sysroot_ptr)
    old_sysroot = reinterpret_cast<const char*>(old_sysroot_ptr);

  // Override the sysroot.
  base::FilePath sysroot("/var/opt/fonts");
  FcConfigSetSysRoot(nullptr, reinterpret_cast<const FcChar8*>(
                                  sysroot.AsUTF8Unsafe().c_str()));

  // Validate that path are relative to sysroot.
  const char kFileName[] = "fonts/arial.ttf";
  FcPatternAddString(pattern.get(), FC_FILE,
                     reinterpret_cast<const FcChar8*>(kFileName));
  const char kExpectedFileName[] = "/var/opt/fonts/fonts/arial.ttf";
  EXPECT_EQ(base::FilePath(kExpectedFileName), GetFontPath(pattern.get()));

  // Restore the old sysroot, if specified.
  if (old_sysroot_ptr) {
    FcConfigSetSysRoot(nullptr,
                       reinterpret_cast<const FcChar8*>(old_sysroot.c_str()));
  }
}

TEST(FontConfigUtilTest, GetFontPathWithoutSysRoot) {
  ScopedFcPattern pattern(FcPatternCreate());

  // Save the old sysroot, if specified.
  std::string old_sysroot;
  const FcChar8* old_sysroot_ptr = FcConfigGetSysRoot(nullptr);
  if (old_sysroot_ptr)
    old_sysroot = reinterpret_cast<const char*>(old_sysroot_ptr);

  // Override (remove) the sysroot.
  FcConfigSetSysRoot(nullptr, nullptr);

  // Check that the filename is not changed without a sysroot present.
  const char kFileName[] = "/var/opt/font/fonts/arial.ttf";
  FcPatternAddString(pattern.get(), FC_FILE,
                     reinterpret_cast<const FcChar8*>(kFileName));
  EXPECT_EQ(base::FilePath(kFileName), GetFontPath(pattern.get()));

  // Restore the old sysroot, if specified.
  if (old_sysroot_ptr) {
    FcConfigSetSysRoot(nullptr,
                       reinterpret_cast<const FcChar8*>(old_sysroot.c_str()));
  }
}

TEST(FontConfigUtilTest, GetFontRenderParamsFromFcPatternWithEmptyPattern) {
  ScopedFcPattern pattern(FcPatternCreate());

  FontRenderParams params;
  GetFontRenderParamsFromFcPattern(pattern.get(), &params);

  FontRenderParams empty_params;
  EXPECT_EQ(params, empty_params);
}

TEST(FontConfigUtilTest, GetFontRenderParamsFromFcPatternWithFalseValues) {
  ScopedFcPattern pattern(FcPatternCreate());
  FcPatternAddBool(pattern.get(), FC_ANTIALIAS, FcFalse);
  FcPatternAddBool(pattern.get(), FC_AUTOHINT, FcFalse);
  FcPatternAddBool(pattern.get(), FC_EMBEDDED_BITMAP, FcFalse);

  FontRenderParams params;
  GetFontRenderParamsFromFcPattern(pattern.get(), &params);

  FontRenderParams expected_params;
  expected_params.antialiasing = false;
  expected_params.autohinter = false;
  expected_params.use_bitmaps = false;
  EXPECT_EQ(params, expected_params);
}

TEST(FontConfigUtilTest, GetFontRenderParamsFromFcPatternWithValues) {
  ScopedFcPattern pattern(FcPatternCreate());
  FcPatternAddBool(pattern.get(), FC_ANTIALIAS, FcTrue);
  FcPatternAddBool(pattern.get(), FC_AUTOHINT, FcTrue);
  FcPatternAddInteger(pattern.get(), FC_HINT_STYLE, FC_HINT_MEDIUM);
  FcPatternAddBool(pattern.get(), FC_EMBEDDED_BITMAP, FcTrue);
  FcPatternAddInteger(pattern.get(), FC_RGBA, FC_RGBA_RGB);

  FontRenderParams params;
  GetFontRenderParamsFromFcPattern(pattern.get(), &params);

  FontRenderParams expected_params;
  expected_params.antialiasing = true;
  expected_params.autohinter = true;
  expected_params.hinting = FontRenderParams::HINTING_MEDIUM;
  expected_params.use_bitmaps = true;
  expected_params.subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_RGB;

  EXPECT_EQ(params, expected_params);
}

}  // namespace gfx
