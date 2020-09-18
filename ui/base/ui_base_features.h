// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UI_BASE_FEATURES_H_
#define UI_BASE_UI_BASE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/base/buildflags.h"

namespace features {

// Keep sorted!

COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kColorProviderRedirection;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kCompositorThreadedScrollbarScrolling;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kExperimentalFlingAnimation;
#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kSettingsShowsPerKeyboardSettings;
#endif  // defined(OS_CHROMEOS)
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kInputMethodSettingsUiUpdate;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kPercentBasedScrolling;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kPointerLockOptions;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kSystemCaptionStyle;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kSystemKeyboardLock;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kNotificationIndicator;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kUiCompositorScrollWithLayers;

COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsNotificationIndicatorEnabled();

COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsUiGpuRasterizationEnabled();

#if defined(OS_WIN)
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kCalculateNativeWinOcclusion;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kElasticOverscrollWin;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kInputPaneOnScreenKeyboard;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kPointerEventsForTouch;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kPrecisionTouchpadLogging;
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const base::Feature kTSFImeSupport;

// Returns true if the system should use WM_POINTER events for touch events.
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsUsingWMPointerForTouch();
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kDirectManipulationStylus;
#endif  // defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

// Used to enable forced colors mode for web content.
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const base::Feature kForcedColors;
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsForcedColorsEnabled();

// Used to enable the eye-dropper in the refresh color-picker.
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const base::Feature kEyeDropper;
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsEyeDropperEnabled();

// Used to enable form controls and scrollbar dark mode rendering.
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kCSSColorSchemeUARendering;
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsCSSColorSchemeUARenderingEnabled();

// Used to enable the new controls UI.
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kFormControlsRefresh;
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsFormControlsRefreshEnabled();

// Used to enable the common select popup.
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kUseCommonSelectPopup;
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsUseCommonSelectPopupEnabled();

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kHandwritingGesture;

COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kNewShortcutMapping;

COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsNewShortcutMappingEnabled();
#endif

// Indicates whether DrmOverlayManager should used the synchronous API to
// perform pageflip tests.
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kSynchronousPageFlipTesting;

COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsSynchronousPageFlipTestingEnabled();

#if defined(USE_X11) || defined(USE_OZONE)
// Indicates whether the OzonePlatform feature is used on Linux. Although, it is
// available for all Ozone platforms, this always resolves to true for
// non-desktop Linux builds. The reason why it is needed for all Ozone builds is
// that we have many places in the code that Ozone takes independently of the
// platform, and it's highly important that when USE_X11 && USE_OZONE are true
// and the OzonePlatform feature is not enabled, the Ozone path is never taken.
// This will be removed as soon as Ozone/Linux is default and USE_X11 is
// removed.  More info at
// https://docs.google.com/document/d/1PvKquOHWySbvbe4bgduAcpW0Pda4BBhXI7xphtyDtPQ
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const base::Feature kUseOzonePlatform;

COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsUsingOzonePlatform();
#endif

// The type of predictor to use for the resampling events. These values are
// used as the 'predictor' feature param for
// |blink::features::kResamplingScrollEvents|.
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const char kPredictorNameLsq[];
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const char kPredictorNameKalman[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kPredictorNameLinearFirst[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kPredictorNameLinearSecond[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kPredictorNameLinearResampling[];
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const char kPredictorNameEmpty[];

// The type of filter to use for filtering events. These values are used as the
// 'filter' feature param for |blink::features::kFilteringScrollPrediction|.
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const char kFilterNameEmpty[];
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const char kFilterNameOneEuro[];

// Android only feature, for swipe to move cursor.
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kSwipeToMoveCursor;

}  // namespace features

#endif  // UI_BASE_UI_BASE_FEATURES_H_
