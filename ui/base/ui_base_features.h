// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UI_BASE_FEATURES_H_
#define UI_BASE_UI_BASE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/buildflags.h"

namespace features {

// Keep sorted!

COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kCompositorThreadedScrollbarScrolling;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kExperimentalFlingAnimation;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kFocusFollowsCursor;
#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kSettingsShowsPerKeyboardSettings;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kInputMethodSettingsUiUpdate;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kWindowsScrollingPersonality;
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsPercentBasedScrollingEnabled();
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kPointerLockOptions;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kSystemCaptionStyle;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kSystemKeyboardLock;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kUiCompositorScrollWithLayers;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kVirtualKeyboardMultitouch;

COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsUiGpuRasterizationEnabled();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kElasticOverscroll;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kAndroidPermissionsCache;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kElasticOverscrollType[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kElasticOverscrollTypeFilter[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kElasticOverscrollTypeTransform[];
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kApplyNativeOccludedRegionToWindowTracker;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kApplyNativeOcclusionToCompositor;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kApplyNativeOcclusionToCompositorType[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kApplyNativeOcclusionToCompositorTypeRelease[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kApplyNativeOcclusionToCompositorTypeThrottle[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kCalculateNativeWinOcclusion;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kInputPaneOnScreenKeyboard;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kPointerEventsForTouch;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kScreenPowerListenerForNativeWinOcclusion;
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const base::Feature kTSFImeSupport;

// Returns true if the system should use WM_POINTER events for touch events.
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsUsingWMPointerForTouch();
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
// This flag is intended to supercede kNewShortcutMapping.
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kImprovedKeyboardShortcuts;
COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsImprovedKeyboardShortcutsEnabled();
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kDeprecateAltBasedSixPack;
COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsDeprecateAltBasedSixPackEnabled();
#endif  // BUILDFLAG(IS_CHROMEOS)

// Used to enable forced colors mode for web content.
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const base::Feature kForcedColors;
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsForcedColorsEnabled();

// Used to enable the eye-dropper in the refresh color-picker.
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const base::Feature kEyeDropper;
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsEyeDropperEnabled();

COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kSystemCursorSizeSupported;
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsSystemCursorSizeSupported();

// Used to enable the common select popup.
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kUseCommonSelectPopup;
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsUseCommonSelectPopupEnabled();

// Used to enable keyboard accessible tooltips.
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kKeyboardAccessibleTooltip;
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsKeyboardAccessibleTooltipEnabled();

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kHandwritingGesture;

COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kNewShortcutMapping;

COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsNewShortcutMappingEnabled();

COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kDeprecateAltClick;

COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsDeprecateAltClickEnabled();

COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kShortcutCustomizationApp;

COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsShortcutCustomizationAppEnabled();

COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kLacrosResourcesFileSharing;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Indicates whether DrmOverlayManager should used the synchronous API to
// perform pageflip tests.
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kSynchronousPageFlipTesting;

COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsSynchronousPageFlipTestingEnabled();

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

// Enables resampling of scroll events using an experimental latency of +3.3ms
// instead of the original -5ms.
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kResamplingScrollEventsExperimentalPrediction;

// The type of prediction used. TimeBased uses a fixed timing, FramesBased uses
// a ratio of the vsync refresh rate. The timing/ratio can be changed on the
// command line through a `latency` param.
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const char kPredictionTypeTimeBased[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kPredictionTypeFramesBased[];
// The default values for `latency`
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kPredictionTypeDefaultTime[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kPredictionTypeDefaultFramesRatio[];

// The type of filter to use for filtering events. These values are used as the
// 'filter' feature param for |blink::features::kFilteringScrollPrediction|.
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const char kFilterNameEmpty[];
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const char kFilterNameOneEuro[];

// Android only feature, for swipe to move cursor.
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kSwipeToMoveCursor;

// Enables UI debugging tools such as shortcuts.
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const base::Feature kUIDebugTools;

COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsSwipeToMoveCursorEnabled();

// Enables Raw Draw.
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const base::Feature kRawDraw;
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsUsingRawDraw();
COMPONENT_EXPORT(UI_BASE_FEATURES) double RawDrawTileSizeFactor();
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsRawDrawUsingMSAA();

COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kUiCompositorReleaseTileResourcesForHiddenLayers;

COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kUiCompositorRequiredTilesOnly;

COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kEnableVariableRefreshRate;
COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsVariableRefreshRateEnabled();

COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::Feature kWaylandScreenCoordinatesEnabled;
COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsWaylandScreenCoordinatesEnabled();

}  // namespace features

#endif  // UI_BASE_UI_BASE_FEATURES_H_
