// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by native theme

#ifndef UI_NATIVE_THEME_NATIVE_THEME_FEATURES_H_
#define UI_NATIVE_THEME_NATIVE_THEME_FEATURES_H_

#include "base/feature_list.h"
#include "ui/native_theme/native_theme_export.h"

namespace features {

NATIVE_THEME_EXPORT BASE_DECLARE_FEATURE(kOverlayScrollbar);

#if BUILDFLAG(IS_CHROMEOS_ASH)
NATIVE_THEME_EXPORT BASE_DECLARE_FEATURE(kOverlayScrollbarsOSSetting);
#endif

NATIVE_THEME_EXPORT BASE_DECLARE_FEATURE(kFluentScrollbar);
NATIVE_THEME_EXPORT BASE_DECLARE_FEATURE(kFluentOverlayScrollbar);

}  // namespace features

namespace ui {

NATIVE_THEME_EXPORT bool IsOverlayScrollbarEnabled();
NATIVE_THEME_EXPORT bool IsFluentScrollbarEnabled();
NATIVE_THEME_EXPORT bool IsFluentOverlayScrollbarEnabled();

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_FEATURES_H_
