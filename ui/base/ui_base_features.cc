// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_features.h"

#include <stdlib.h>

#include "build/chromeos_buildflags.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace features {

#if defined(OS_WIN)
// Once enabled, the exact behavior is dictated by the field trial param
// name `kApplyNativeOcclusionToCompositorType`.
const base::Feature kApplyNativeOcclusionToCompositor{
    "ApplyNativeOcclusionToCompositor", base::FEATURE_DISABLED_BY_DEFAULT};

// Field trial param name for `kApplyNativeOcclusionToCompositor`.
const char kApplyNativeOcclusionToCompositorType[] = "type";
// Indicates occlusion should be applied to the compositor.
const char kApplyNativeOcclusionToCompositorTypeApplyOnly[] = "apply";
// Indicates occlusion should be applied to the compositor, and when occluded
// the root surface should be evicted when hidden/occluded.
const char kApplyNativeOcclusionToCompositorTypeApplyAndEvict[] =
    "apply-and-evict";
// Indicates the root surface should be evicted when hidden/occluded.
const char kApplyNativeOcclusionToCompositorTypeEvictOnly[] = "evict";

// If enabled, calculate native window occlusion - Windows-only.
const base::Feature kCalculateNativeWinOcclusion{
    "CalculateNativeWinOcclusion", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, listen for screen power state change and factor into the native
// window occlusion detection - Windows-only.
const base::Feature kScreenPowerListenerForNativeWinOcclusion{
    "ScreenPowerListenerForNativeWinOcclusion",
    base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, displays Windows 11 style menus on Windows 11.
const base::Feature kWin11StyleMenus{"Win11StyleMenus",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// If this Windows 11 style menu feature parameter is enabled, displays that
// style menu on all Windows versions.
const char kWin11StyleMenuAllWindowsVersionsName[] = "All Windows Versions";

#endif  // defined(OS_WIN)

// Whether or not to delegate color queries to the color provider.
const base::Feature kColorProviderRedirection = {
    "ColorProviderRedirection", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Integrate input method specific settings to Chrome OS settings page.
// https://crbug.com/895886.
const base::Feature kSettingsShowsPerKeyboardSettings = {
    "InputMethodIntegratedSettings", base::FEATURE_DISABLED_BY_DEFAULT};

// Experimental shortcut handling and mapping to address i18n issues.
// https://crbug.com/1067269
const base::Feature kNewShortcutMapping = {"NewShortcutMapping",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

bool IsNewShortcutMappingEnabled() {
  // kImprovedKeyboardShortcuts supercedes kNewShortcutMapping.
  return !IsImprovedKeyboardShortcutsEnabled() &&
         base::FeatureList::IsEnabled(kNewShortcutMapping);
}

const base::Feature kDeprecateAltClick = {"DeprecateAltClick",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

bool IsDeprecateAltClickEnabled() {
  return base::FeatureList::IsEnabled(kDeprecateAltClick);
}

const base::Feature kShortcutCustomizationApp = {
    "ShortcutCustomizationApp", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsShortcutCustomizationAppEnabled() {
  return base::FeatureList::IsEnabled(kShortcutCustomizationApp);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Update of the virtual keyboard settings UI as described in
// https://crbug.com/876901.
const base::Feature kInputMethodSettingsUiUpdate = {
    "InputMethodSettingsUiUpdate", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables percent-based scrolling for mousewheel and keyboard initiated
// scrolls.
const base::Feature kPercentBasedScrolling = {
    "PercentBasedScrolling", base::FEATURE_DISABLED_BY_DEFAULT};

// Allows requesting unadjusted movement when entering pointerlock.
const base::Feature kPointerLockOptions = {"PointerLockOptions",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Allows system caption style for WebVTT Captions.
const base::Feature kSystemCaptionStyle{"SystemCaptionStyle",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Allows system keyboard event capture via the keyboard lock API.
const base::Feature kSystemKeyboardLock{"SystemKeyboardLock",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Enables GPU rasterization for all UI drawing (where not blocklisted).
const base::Feature kUiGpuRasterization = {"UiGpuRasterization",
#if defined(OS_APPLE) || BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_FUCHSIA) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
                                           base::FEATURE_ENABLED_BY_DEFAULT
#else
                                           base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

bool IsUiGpuRasterizationEnabled() {
  return base::FeatureList::IsEnabled(kUiGpuRasterization);
}

// Enables scrolling with layers under ui using the ui::Compositor.
const base::Feature kUiCompositorScrollWithLayers = {
    "UiCompositorScrollWithLayers",
// TODO(https://crbug.com/615948): Use composited scrolling on all platforms.
#if defined(OS_APPLE)
    base::FEATURE_ENABLED_BY_DEFAULT
#else
    base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enables compositor threaded scrollbar scrolling by mapping pointer events to
// gesture events.
const base::Feature kCompositorThreadedScrollbarScrolling = {
    "CompositorThreadedScrollbarScrolling", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the use of a touch fling curve that is based on the behavior of
// native apps on Windows.
const base::Feature kExperimentalFlingAnimation {
  "ExperimentalFlingAnimation",
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_WIN) || (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
                        !BUILDFLAG(IS_CHROMEOS_LACROS))
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

#if defined(OS_ANDROID) || defined(OS_WIN)
// Cached in Java as well, make sure defaults are updated together.
const base::Feature kElasticOverscroll = {"ElasticOverscroll",
#if defined(OS_ANDROID)
                                          base::FEATURE_ENABLED_BY_DEFAULT
#else  // defined(OS_ANDROID)
                                          base::FEATURE_DISABLED_BY_DEFAULT
#endif
};
#endif  // defined(OS_WIN) || defined(OS_ANDROID)

#if defined(OS_ANDROID)
const char kElasticOverscrollType[] = "type";
const char kElasticOverscrollTypeFilter[] = "filter";
const char kElasticOverscrollTypeTransform[] = "transform";
#endif  // defined(OS_ANDROID)

// Enables focus follow follow cursor (sloppyfocus).
const base::Feature kFocusFollowsCursor = {"FocusFollowsCursor",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_WIN)
// Enables InputPane API for controlling on screen keyboard.
const base::Feature kInputPaneOnScreenKeyboard = {
    "InputPaneOnScreenKeyboard", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables using WM_POINTER instead of WM_TOUCH for touch events.
const base::Feature kPointerEventsForTouch = {"PointerEventsForTouch",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
// Enables using TSF (over IMM32) for IME.
const base::Feature kTSFImeSupport = {"TSFImeSupport",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

bool IsUsingWMPointerForTouch() {
  return base::win::GetVersion() >= base::win::Version::WIN8 &&
         base::FeatureList::IsEnabled(kPointerEventsForTouch);
}

#endif  // defined(OS_WIN)

#if defined(OS_CHROMEOS)
// This feature supercedes kNewShortcutMapping.
const base::Feature kImprovedKeyboardShortcuts = {
    "ImprovedKeyboardShortcuts", base::FEATURE_ENABLED_BY_DEFAULT};

bool IsImprovedKeyboardShortcutsEnabled() {
  return base::FeatureList::IsEnabled(kImprovedKeyboardShortcuts);
}

// Whether to deprecate the Alt-Based event rewrites that map to the
// Page Up/Down, Home/End, Insert/Delete keys. This feature was a
// part of kImprovedKeyboardShortcuts, but it is being postponed until
// the new shortcut customization app ships.
// TODO(crbug.com/1179893): Remove after the customization app ships.
const base::Feature kDeprecateAltBasedSixPack = {
    "DeprecateAltBasedSixPack", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsDeprecateAltBasedSixPackEnabled() {
  return base::FeatureList::IsEnabled(kDeprecateAltBasedSixPack);
}
#endif  // defined(OS_CHROMEOS)

// Enables forced colors mode for web content.
const base::Feature kForcedColors{"ForcedColors",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

bool IsForcedColorsEnabled() {
  static const bool forced_colors_enabled =
      base::FeatureList::IsEnabled(features::kForcedColors);
  return forced_colors_enabled;
}

// Enables the eye-dropper in the refresh color-picker for Windows, Mac
// and Linux. This feature will be released for other platforms in later
// milestones.
const base::Feature kEyeDropper {
  "EyeDropper",
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

bool IsEyeDropperEnabled() {
  return base::FeatureList::IsEnabled(features::kEyeDropper);
}

// Enable the common select popup.
const base::Feature kUseCommonSelectPopup = {"UseCommonSelectPopup",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

bool IsUseCommonSelectPopupEnabled() {
  return base::FeatureList::IsEnabled(features::kUseCommonSelectPopup);
}

// Enables keyboard accessible tooltip.
const base::Feature kKeyboardAccessibleTooltip{
    "KeyboardAccessibleTooltip", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsKeyboardAccessibleTooltipEnabled() {
  static const bool keyboard_accessible_tooltip_enabled =
      base::FeatureList::IsEnabled(features::kKeyboardAccessibleTooltip);
  return keyboard_accessible_tooltip_enabled;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kHandwritingGesture = {"HandwritingGesture",
                                           base::FEATURE_ENABLED_BY_DEFAULT};
#endif

const base::Feature kSynchronousPageFlipTesting{
    "SynchronousPageFlipTesting", base::FEATURE_ENABLED_BY_DEFAULT};

bool IsSynchronousPageFlipTestingEnabled() {
  return base::FeatureList::IsEnabled(kSynchronousPageFlipTesting);
}

const base::Feature kResamplingScrollEventsExperimentalPrediction{
    "ResamplingScrollEventsExperimentalPrediction",
    base::FEATURE_DISABLED_BY_DEFAULT};

bool IsUsingOzonePlatform() {
#if defined(USE_X11) && !defined(USE_OZONE)

#error Non-Ozone/X11 builds are no longer supported

#endif  // defined(USE_X11) || defined(USE_OZONE)
  return true;
}

const char kPredictorNameLsq[] = "lsq";
const char kPredictorNameKalman[] = "kalman";
const char kPredictorNameLinearFirst[] = "linear_first";
const char kPredictorNameLinearSecond[] = "linear_second";
const char kPredictorNameLinearResampling[] = "linear_resampling";
const char kPredictorNameEmpty[] = "empty";

const char kFilterNameEmpty[] = "empty_filter";
const char kFilterNameOneEuro[] = "one_euro_filter";

const char kPredictionTypeTimeBased[] = "time";
const char kPredictionTypeFramesBased[] = "frames";
const char kPredictionTypeDefaultTime[] = "3.3";
const char kPredictionTypeDefaultFramesRatio[] = "0.5";

const base::Feature kSwipeToMoveCursor{"SwipeToMoveCursor",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUIDebugTools{"ui-debug-tools",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

bool IsSwipeToMoveCursorEnabled() {
  static const bool enabled =
#if defined(OS_ANDROID)
      base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_R;
#else
      base::FeatureList::IsEnabled(kSwipeToMoveCursor);
#endif
  return enabled;
}

}  // namespace features
