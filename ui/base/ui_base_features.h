// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UI_BASE_FEATURES_H_
#define UI_BASE_UI_BASE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/buildflags.h"

namespace features {

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kExperimentalFlingAnimation);
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kFocusFollowsCursor);
#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kSettingsShowsPerKeyboardSettings);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kInputMethodSettingsUiUpdate);
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kWindowsScrollingPersonality);
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsPercentBasedScrollingEnabled();
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kSystemCaptionStyle);
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kSystemKeyboardLock);
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kUiCompositorScrollWithLayers);

COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsUiGpuRasterizationEnabled();

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kClipboardFiles);
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kDragDropEmpty);
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kDragDropFiles);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kElasticOverscroll);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kApplyNativeOccludedRegionToWindowTracker);
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kCalculateNativeWinOcclusion);
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kPointerEventsForTouch);
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kScreenPowerListenerForNativeWinOcclusion);

// Returns true if the system should use WM_POINTER events for touch events.
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsUsingWMPointerForTouch();
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_LACROS)
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kApplyNativeOcclusionToCompositor);
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kAlwaysTrackNativeWindowOcclusionForTest);
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const base::FeatureParam<std::string>
    kApplyNativeOcclusionToCompositorType;
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kApplyNativeOcclusionToCompositorTypeRelease[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kApplyNativeOcclusionToCompositorTypeThrottle[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kApplyNativeOcclusionToCompositorTypeThrottleAndRelease[];
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kImprovedKeyboardShortcuts);
COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsImprovedKeyboardShortcutsEnabled();
#endif  // BUILDFLAG(IS_CHROMEOS)

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kTouchTextEditingRedesign);
COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsTouchTextEditingRedesignEnabled();

// Used to enable forced colors mode for web content.
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kForcedColors);
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsForcedColorsEnabled();

// Used to enable the eye-dropper in the refresh color-picker.
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kEyeDropper);
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsEyeDropperEnabled();

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kSystemCursorSizeSupported);
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsSystemCursorSizeSupported();

// Used to enable keyboard accessible tooltips in in-page content
// (i.e., inside Blink). See
// ::views::features::kKeyboardAccessibleTooltipInViews for
// keyboard-accessible tooltips in Views UI.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kKeyboardAccessibleTooltip);
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsKeyboardAccessibleTooltipEnabled();

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kDeprecateAltClick);

COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsDeprecateAltClickEnabled();

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kNotificationsIgnoreRequireInteraction);

COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsNotificationsIgnoreRequireInteractionEnabled();

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kShortcutCustomization);

COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsShortcutCustomizationEnabled();

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kLacrosResourcesFileSharing);

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kSupportF11AndF12KeyShortcuts);

COMPONENT_EXPORT(UI_BASE_FEATURES) bool AreF11AndF12ShortcutsEnabled();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_OZONE)
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kOzoneBubblesUsePlatformWidgets);

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kWaylandPerSurfaceScale);

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kWaylandTextInputV3);

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kWaylandUiScale);
#endif  // BUILDFLAG(IS_OZONE)

#if BUILDFLAG(IS_LINUX)
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kOverrideDefaultOzonePlatformHintToAuto);
#endif  // BUILDFLAG(IS_LINUX)

// Indicates whether DrmOverlayManager should used the synchronous API to
// perform pageflip tests.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kSynchronousPageFlipTesting);

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
BASE_DECLARE_FEATURE(kResamplingScrollEventsExperimentalPrediction);

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
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kSwipeToMoveCursor);

// Enables UI debugging tools such as shortcuts.
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kUIDebugTools);

COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsSwipeToMoveCursorEnabled();

// Enables Raw Draw.
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kRawDraw);
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsUsingRawDraw();
COMPONENT_EXPORT(UI_BASE_FEATURES) double RawDrawTileSizeFactor();
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsRawDrawUsingMSAA();

COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kStylusSpecificTapSlop);

// This feature indicates that this device should have variable refresh rates
// enabled by default if available. This overrides the default value of
// |kEnableVariableRefreshRate|. This flag is added by cros-config and not
// exposed in the chrome://flags UI.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kVariableRefreshRateAvailable);
// Enables the variable refresh rate feature for Borealis gaming only.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kEnableVariableRefreshRate);
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsVariableRefreshRateEnabled();
// Enables the variable refresh rate feature at all times.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kEnableVariableRefreshRateAlwaysOn);
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsVariableRefreshRateAlwaysOn();

COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kLacrosColorManagement);
COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsLacrosColorManagementEnabled();

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kBubbleMetricsApi);

#if BUILDFLAG(IS_APPLE)
// Font Smoothing, a CoreText technique, simulates optical sizes to enhance text
// readability at smaller scales. In practice, it leads to an increased
// perception of text weight, creating discrepancies between renderings in UX
// design tools and actual macOS displays. This feature is only effective when
// ChromeRefresh2023 is enabled.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kCr2023MacFontSmoothing);
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_WIN)
// Use font settings for contrast and gamma as specified in system settings.
// If not set, these values fall back to the pre-defined Skia defaults.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kUseGammaContrastRegistrySettings);

// Increases the contrast of text to align more closely with contemporary
// applications.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kIncreaseWindowsTextContrast);
#endif  // BUILDFLAG(IS_WIN)

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kBubbleFrameViewTitleIsHeading);

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kEnableGestureBeginEndTypes);

// Use the UTF-8 encoding for SVG images instead of UTF-16.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kUseUtf8EncodingForSvgImage);

// Copy bookmark and write url format to clipboard with empty title.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kWriteBookmarkWithoutTitle);

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kAsyncFullscreenWindowState);

}  // namespace features

#endif  // UI_BASE_UI_BASE_FEATURES_H_
