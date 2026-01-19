// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/features/native_theme_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace features {

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_IOS)
constexpr base::FeatureState kOverlayScrollbarFeatureState =
    base::FEATURE_ENABLED_BY_DEFAULT;
#else
constexpr base::FeatureState kOverlayScrollbarFeatureState =
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif
// Enables or disables overlay scrollbars in Blink (i.e. web content) on Aura
// or Linux.  The status of native UI overlay scrollbars is determined in
// PlatformStyle::CreateScrollBar. Does nothing on Mac.
BASE_FEATURE(kOverlayScrollbar, kOverlayScrollbarFeatureState);

// Disable to keep scrollbars visible forever once shown, and immediately
// update scrollbar states instead of animating. This is used to ensure
// ref tests in WPT do not flake based on the time taken before the
// screenshot is captured.
BASE_FEATURE(kScrollbarAnimations, base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, scrollbars flash only once when a page is loaded or when they
// become visible on the viewport instead of flashing after every scroll update.
BASE_FEATURE(kOverlayScrollbarFlashOnlyOnceVisibleOnViewport,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables will flash scrollbar when user move mouse enter a scrollable area.
BASE_FEATURE(kOverlayScrollbarFlashWhenMouseEnter,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features

namespace ui {

bool IsFluentOverlayScrollbarEnabled() {
  return IsFluentScrollbarEnabled() && IsOverlayScrollbarEnabledByFeatureFlag();
}

bool IsFluentScrollbarEnabled() {
// Fluent scrollbars are only used for some OSes due to UI design guidelines.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  return true;
#else
  return false;
#endif
}

bool IsOverlayScrollbarEnabledByFeatureFlag() {
  return base::FeatureList::IsEnabled(features::kOverlayScrollbar);
}

}  // namespace ui
