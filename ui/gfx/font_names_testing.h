// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_NAMES_TESTING_H_
#define UI_GFX_FONT_NAMES_TESTING_H_

#include "build/build_config.h"

namespace gfx {

/*
Reference for fonts available on Android:

Jelly Bean:
  https://android.googlesource.com/platform/frameworks/base/+/jb-release/data/fonts/system_fonts.xml
KitKat:
  https://android.googlesource.com/platform/frameworks/base/+/kitkat-release/data/fonts/system_fonts.xml
master:
  https://android.googlesource.com/platform/frameworks/base/+/master/data/fonts/fonts.xml

Note that we have to support the full range from JellyBean to the latest
dessert.
*/

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
inline constexpr char kTestFontName[] = "Arimo";
#elif BUILDFLAG(IS_ANDROID)
inline constexpr char kTestFontName[] = "sans-serif";
#else
inline constexpr char kTestFontName[] = "Arial";
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
inline constexpr char kSymbolFontName[] = "DejaVu Sans";
#elif BUILDFLAG(IS_ANDROID)
inline constexpr char kSymbolFontName[] = "monospace";
#elif BUILDFLAG(IS_WIN)
inline constexpr char kSymbolFontName[] = "Segoe UI Symbol";
#else
inline constexpr char kSymbolFontName[] = "Symbol";
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
inline constexpr char kCJKFontName[] = "Noto Sans CJK JP";
#elif BUILDFLAG(IS_ANDROID)
inline constexpr char kCJKFontName[] = "serif";
#elif BUILDFLAG(IS_MAC)
inline constexpr char kCJKFontName[] = "Heiti SC";
#elif BUILDFLAG(IS_IOS)
inline constexpr char kCJKFontName[] = "PingFang SC";
#else
inline constexpr char kCJKFontName[] = "SimSun";
#endif

} // end namespace gfx

#endif  // UI_GFX_FONT_NAMES_TESTING_H_
