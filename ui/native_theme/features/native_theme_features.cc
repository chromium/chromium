// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/features/native_theme_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace features {

constexpr base::FeatureParam<ScrollbarMode>::Option kScrollbarModeOptions[] = {
    {ScrollbarMode::kOverlay, "overlay"},
    {ScrollbarMode::kDevice, "device"},
    {ScrollbarMode::kClassic, "classic"},
};

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_IOS) || BUILDFLAG(IS_MAC)
constexpr base::FeatureState kOverlayScrollbarFeatureState =
    base::FEATURE_ENABLED_BY_DEFAULT;
#else
constexpr base::FeatureState kOverlayScrollbarFeatureState =
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif

// Controls the scrollbar mode in Blink (i.e. web content) on desktop
// platforms.
// - enabled with mode/overlay: force overlay scrollbars
// - enabled with mode/device: follow OS setting
// - enabled with mode/classic: force non-overlay scrollbars
// - disabled: equivalent to mode/classic
//
// On mobile (Android / iOS), scrollbars are always overlay regardless.
//
// Enabled defaults to mode/device. The OS value for mode/device comes from:
// - AccessibilityController::always_show_scrollbar() on ChromeOS
// - OsSettingsProvider::PrefersOverlayScrollbars() (subclass overrides) on
//   other platforms
BASE_FEATURE(kOverlayScrollbar, kOverlayScrollbarFeatureState);
constinit const base::FeatureParam<ScrollbarMode> kScrollbarMode{
    &kOverlayScrollbar, "mode", ScrollbarMode::kDevice, &kScrollbarModeOptions};

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
  return ShouldUseOverlayScrollbar(true);
}

bool ShouldUseOverlayScrollbar(bool os_prefers_overlay_scrollbars) {
  if (!base::FeatureList::IsEnabled(features::kOverlayScrollbar)) {
    return false;
  }
  const auto mode = features::kScrollbarMode.Get();
  return mode == features::ScrollbarMode::kOverlay ||
         (mode == features::ScrollbarMode::kDevice &&
          os_prefers_overlay_scrollbars);
}

}  // namespace ui
