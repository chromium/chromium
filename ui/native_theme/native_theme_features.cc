// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_features.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace features {

#if defined(OS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    defined(OS_FUCHSIA) || BUILDFLAG(IS_CHROMEOS_LACROS)
constexpr base::FeatureState kOverlayScrollbarFeatureState =
    base::FEATURE_ENABLED_BY_DEFAULT;
#else
constexpr base::FeatureState kOverlayScrollbarFeatureState =
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif

// Enables or disables overlay scrollbars in Blink (i.e. web content) on Aura
// or Linux.  The status of native UI overlay scrollbars is determined in
// PlatformStyle::CreateScrollBar. Does nothing on Mac.
const base::Feature kOverlayScrollbar{"OverlayScrollbar",
                                      kOverlayScrollbarFeatureState};

}  // namespace features

namespace ui {

bool IsOverlayScrollbarEnabled() {
  return base::FeatureList::IsEnabled(features::kOverlayScrollbar);
}

}  // namespace ui
