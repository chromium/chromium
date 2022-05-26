// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_features.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace features {

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_CHROMEOS_LACROS)
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

// Fluent scrollbars aim to modernize the Chromium scrollbars (both overlay
// and non-overlay) to fit the Windows 11 Fluent design language. For now,
// the feature will only support Windows platform and can be later available
// on Linux as well. The feature is currently in development and disabled
// by default.
const base::Feature kFluentScrollbar{"FluentScrollbar",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

namespace ui {

bool IsOverlayScrollbarEnabled() {
  return base::FeatureList::IsEnabled(features::kOverlayScrollbar);
}

bool IsFluentScrollbarEnabled() {
// Currently, the feature is only supported on Windows.
#if BUILDFLAG(IS_WIN)
  return IsOverlayScrollbarEnabled() &&
         base::FeatureList::IsEnabled(features::kFluentScrollbar);
#else
  return false;
#endif
}

}  // namespace ui
