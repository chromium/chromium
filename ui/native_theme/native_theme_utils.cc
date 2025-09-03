// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_utils.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/native_theme/native_theme.h"
#else
#include "ui/native_theme/features/native_theme_features.h"
#endif

namespace ui {

bool IsOverlayScrollbarEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
  return NativeTheme::GetInstanceForWeb()->use_overlay_scrollbar();
#else
  return IsOverlayScrollbarEnabledByFeatureFlag();
#endif
}

}  // namespace ui
