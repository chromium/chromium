// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by native theme

#ifndef UI_NATIVE_THEME_FEATURES_NATIVE_THEME_FEATURES_H_
#define UI_NATIVE_THEME_FEATURES_NATIVE_THEME_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

enum class ScrollbarMode {
  kOverlay,
  kDevice,
  kClassic,
};

COMPONENT_EXPORT(NATIVE_THEME_FEATURES) BASE_DECLARE_FEATURE(kOverlayScrollbar);
COMPONENT_EXPORT(NATIVE_THEME_FEATURES)
BASE_DECLARE_FEATURE_PARAM(ScrollbarMode, kScrollbarMode);
COMPONENT_EXPORT(NATIVE_THEME_FEATURES)
BASE_DECLARE_FEATURE(kScrollbarAnimations);

COMPONENT_EXPORT(NATIVE_THEME_FEATURES)
BASE_DECLARE_FEATURE(kOverlayScrollbarFlashOnlyOnceVisibleOnViewport);
COMPONENT_EXPORT(NATIVE_THEME_FEATURES)
BASE_DECLARE_FEATURE(kOverlayScrollbarFlashWhenMouseEnter);

}  // namespace features

namespace ui {

COMPONENT_EXPORT(NATIVE_THEME_FEATURES) bool IsFluentOverlayScrollbarEnabled();
COMPONENT_EXPORT(NATIVE_THEME_FEATURES) bool IsFluentScrollbarEnabled();

COMPONENT_EXPORT(NATIVE_THEME_FEATURES)
bool IsOverlayScrollbarEnabledByFeatureFlag();

COMPONENT_EXPORT(NATIVE_THEME_FEATURES)
bool ShouldUseOverlayScrollbar(bool os_prefers_overlay_scrollbars);

}  // namespace ui

#endif  // UI_NATIVE_THEME_FEATURES_NATIVE_THEME_FEATURES_H_
