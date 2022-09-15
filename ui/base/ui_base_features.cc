// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_features.h"

#include <stdlib.h>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/base/shortcut_mapping_pref_delegate.h"
#endif

namespace features {

#if BUILDFLAG(IS_WIN)
// If enabled, the occluded region of the HWND is supplied to WindowTracker.
const base::Feature kApplyNativeOccludedRegionToWindowTracker{
    "ApplyNativeOccludedRegionToWindowTracker",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Once enabled, the exact behavior is dictated by the field trial param
// name `kApplyNativeOcclusionToCompositorType`.
const base::Feature kApplyNativeOcclusionToCompositor{
    "ApplyNativeOcclusionToCompositor", base::FEATURE_DISABLED_BY_DEFAULT};

// Field trial param name for `kApplyNativeOcclusionToCompositor`.
const char kApplyNativeOcclusionToCompositorType[] = "type";
// When the WindowTreeHost is occluded or hidden, resources are released and
// the compositor is hidden. See WindowTreeHost for specifics on what this
// does.
const char kApplyNativeOcclusionToCompositorTypeRelease[] = "release";
// When the WindowTreeHost is occluded the frame rate is throttled.
const char kApplyNativeOcclusionToCompositorTypeThrottle[] = "throttle";

// If enabled, calculate native window occlusion - Windows-only.
const base::Feature kCalculateNativeWinOcclusion{
    "CalculateNativeWinOcclusion", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, listen for screen power state change and factor into the native
// window occlusion detection - Windows-only.
const base::Feature kScreenPowerListenerForNativeWinOcclusion{
    "ScreenPowerListenerForNativeWinOcclusion",
    base::FEATURE_ENABLED_BY_DEFAULT};

#endif  // BUILDFLAG(IS_WIN)

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

// Share the resource file with ash-chrome. This feature reduces the memory
// consumption while the disk usage slightly increases.
// https://crbug.com/1253280.
const base::Feature kLacrosResourcesFileSharing = {
    "LacrosResourcesFileSharing", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Update of the virtual keyboard settings UI as described in
// https://crbug.com/876901.
const base::Feature kInputMethodSettingsUiUpdate = {
    "InputMethodSettingsUiUpdate", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables percent-based scrolling for mousewheel and keyboard initiated
// scrolls and impulse curve animations.
const enum base::FeatureState kWindowsScrollingPersonalityDefaultStatus =
    base::FEATURE_DISABLED_BY_DEFAULT;
static_assert(!BUILDFLAG(IS_MAC) ||
                  (BUILDFLAG(IS_MAC) &&
                   kWindowsScrollingPersonalityDefaultStatus ==
                       base::FEATURE_DISABLED_BY_DEFAULT),
              "Do not enable this on the Mac. The animation does not match the "
              "system scroll animation curve to such an extent that it makes "
              "Chromium stand out in a bad way.");
const base::Feature kWindowsScrollingPersonality = {
    "WindowsScrollingPersonality", kWindowsScrollingPersonalityDefaultStatus};

bool IsPercentBasedScrollingEnabled() {
  return base::FeatureList::IsEnabled(features::kWindowsScrollingPersonality);
}

// Allows requesting unadjusted movement when entering pointerlock.
const base::Feature kPointerLockOptions = {"PointerLockOptions",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Allows system caption style for WebVTT Captions.
const base::Feature kSystemCaptionStyle{"SystemCaptionStyle",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, the feature will query the OS for a default cursor size,
// to be used in determining the concrete object size of a custom cursor in
// blink. Currently enabled by default on Windows only.
// TODO(crbug.com/1333523) - Implement for other platforms.
const base::Feature kSystemCursorSizeSupported{
  "SystemCursorSizeSupported",
#if BUILDFLAG(IS_WIN)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

bool IsSystemCursorSizeSupported() {
  return base::FeatureList::IsEnabled(kSystemCursorSizeSupported);
}

// Allows system keyboard event capture via the keyboard lock API.
const base::Feature kSystemKeyboardLock{"SystemKeyboardLock",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Enables GPU rasterization for all UI drawing (where not blocklisted).
const base::Feature kUiGpuRasterization = {"UiGpuRasterization",
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_CHROMEOS_LACROS)
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
#if BUILDFLAG(IS_APPLE)
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
#if BUILDFLAG(IS_WIN) ||                                   \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
     !BUILDFLAG(IS_CHROMEOS_LACROS))
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
// Cached in Java as well, make sure defaults are updated together.
const base::Feature kElasticOverscroll = {"ElasticOverscroll",
#if BUILDFLAG(IS_ANDROID)
                                          base::FEATURE_ENABLED_BY_DEFAULT
#else  // BUILDFLAG(IS_ANDROID)
                                          base::FEATURE_DISABLED_BY_DEFAULT
#endif
};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
const base::Feature kAndroidPermissionsCache{"AndroidPermissionsCache",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const char kElasticOverscrollType[] = "type";
const char kElasticOverscrollTypeFilter[] = "filter";
const char kElasticOverscrollTypeTransform[] = "transform";
#endif  // BUILDFLAG(IS_ANDROID)

// Enables focus follow follow cursor (sloppyfocus).
const base::Feature kFocusFollowsCursor = {"FocusFollowsCursor",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_WIN)
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

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
// This feature supercedes kNewShortcutMapping.
const base::Feature kImprovedKeyboardShortcuts = {
    "ImprovedKeyboardShortcuts", base::FEATURE_ENABLED_BY_DEFAULT};

bool IsImprovedKeyboardShortcutsEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug/1264581): Remove this once kDeviceI18nShortcutsEnabled policy is
  // deprecated.
  if (::ui::ShortcutMappingPrefDelegate::IsInitialized()) {
    ::ui::ShortcutMappingPrefDelegate* instance =
        ::ui::ShortcutMappingPrefDelegate::GetInstance();
    if (instance && instance->IsDeviceEnterpriseManaged()) {
      return instance->IsI18nShortcutPrefEnabled();
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
#endif  // BUILDFLAG(IS_CHROMEOS)

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
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
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
#if BUILDFLAG(IS_ANDROID)
      base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_R;
#else
      base::FeatureList::IsEnabled(kSwipeToMoveCursor);
#endif
  return enabled;
}

// Enable raw draw for tiles.
const base::Feature kRawDraw{"RawDraw", base::FEATURE_DISABLED_BY_DEFAULT};

// Tile size = viewport size * TileSizeFactor
const base::FeatureParam<double> kRawDrawTileSizeFactor{&kRawDraw,
                                                        "TileSizeFactor", 1};

const base::FeatureParam<bool> kIsRawDrawUsingMSAA{&kRawDraw, "IsUsingMSAA",
                                                   false};
bool IsUsingRawDraw() {
  return base::FeatureList::IsEnabled(kRawDraw);
}

double RawDrawTileSizeFactor() {
  return kRawDrawTileSizeFactor.Get();
}

bool IsRawDrawUsingMSAA() {
  return kIsRawDrawUsingMSAA.Get();
}

const base::Feature kUiCompositorReleaseTileResourcesForHiddenLayers{
    "UiCompositorReleaseTileResourcesForHiddenLayers",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kUiCompositorRequiredTilesOnly{
    "UiCompositorRequiredTilesOnly", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableVariableRefreshRate = {
    "EnableVariableRefreshRate", base::FEATURE_DISABLED_BY_DEFAULT};
bool IsVariableRefreshRateEnabled() {
  return base::FeatureList::IsEnabled(kEnableVariableRefreshRate);
}

const base::Feature kWaylandScreenCoordinatesEnabled{
  "WaylandScreenCoordinatesEnabled",
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

bool IsWaylandScreenCoordinatesEnabled() {
  return base::FeatureList::IsEnabled(kWaylandScreenCoordinatesEnabled);
}

// Enables chrome color management wayland protocol for lacros.
const base::Feature kLacrosColorManagement{"LacrosColorManagement",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

bool IsLacrosColorManagementEnabled() {
  return base::FeatureList::IsEnabled(kLacrosColorManagement);
}

}  // namespace features
