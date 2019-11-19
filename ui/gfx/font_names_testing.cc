// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_names_testing.h"

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

#if defined(OS_LINUX) || defined(OS_FUCHSIA)
const char kTestFontName[] = "Arimo";
#elif defined(OS_ANDROID)
const char kTestFontName[] = "sans-serif";
#else
const char kTestFontName[] = "Arial";
#endif

#if defined(OS_LINUX) || defined(OS_FUCHSIA)
const char kSymbolFontName[] = "DejaVu Sans";
#elif defined(OS_ANDROID)
const char kSymbolFontName[] = "monospace";
#elif defined(OS_WIN)
const char kSymbolFontName[] = "Segoe UI Symbol";
#else
const char kSymbolFontName[] = "Symbol";
#endif

#if defined(OS_LINUX) || defined(OS_FUCHSIA)
const char kCJKFontName[] = "Noto Sans CJK JP";
#elif defined(OS_ANDROID)
const char kCJKFontName[] = "serif";
#elif defined(OS_MACOSX)
const char kCJKFontName[] = "Heiti SC";
#else
const char kCJKFontName[] = "SimSun";
#endif

} // namespace gfx
