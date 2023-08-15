// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_features.h"

#include <stdlib.h>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/base/shortcut_mapping_pref_delegate.h"
#endif

namespace features {

#if BUILDFLAG(IS_WIN)
// If enabled, the occluded region of the HWND is supplied to WindowTracker.
BASE_FEATURE(kApplyNativeOccludedRegionToWindowTracker,
             "ApplyNativeOccludedRegionToWindowTracker",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Once enabled, the exact behavior is dictated by the field trial param
// name `kApplyNativeOcclusionToCompositorType`.
BASE_FEATURE(kApplyNativeOcclusionToCompositor,
             "ApplyNativeOcclusionToCompositor",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Field trial param name for `kApplyNativeOcclusionToCompositor`.
const char kApplyNativeOcclusionToCompositorType[] = "type";
// When the WindowTreeHost is occluded or hidden, resources are released and
// the compositor is hidden. See WindowTreeHost for specifics on what this
// does.
const char kApplyNativeOcclusionToCompositorTypeRelease[] = "release";
// When the WindowTreeHost is occluded the frame rate is throttled.
const char kApplyNativeOcclusionToCompositorTypeThrottle[] = "throttle";

// If enabled, calculate native window occlusion - Windows-only.
BASE_FEATURE(kCalculateNativeWinOcclusion,
             "CalculateNativeWinOcclusion",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, listen for screen power state change and factor into the native
// window occlusion detection - Windows-only.
BASE_FEATURE(kScreenPowerListenerForNativeWinOcclusion,
             "ScreenPowerListenerForNativeWinOcclusion",
             base::FEATURE_ENABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Integrate input method specific settings to Chrome OS settings page.
// https://crbug.com/895886.
BASE_FEATURE(kSettingsShowsPerKeyboardSettings,
             "InputMethodIntegratedSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Experimental shortcut handling and mapping to address i18n issues.
// https://crbug.com/1067269
BASE_FEATURE(kNewShortcutMapping,
             "NewShortcutMapping",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNewShortcutMappingEnabled() {
  // kImprovedKeyboardShortcuts supercedes kNewShortcutMapping.
  return !IsImprovedKeyboardShortcutsEnabled() &&
         base::FeatureList::IsEnabled(kNewShortcutMapping);
}

BASE_FEATURE(kDeprecateAltClick,
             "DeprecateAltClick",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDeprecateAltClickEnabled() {
  return base::FeatureList::IsEnabled(kDeprecateAltClick);
}

BASE_FEATURE(kShortcutCustomizationApp,
             "ShortcutCustomizationApp",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsShortcutCustomizationAppEnabled() {
  return base::FeatureList::IsEnabled(kShortcutCustomizationApp);
}

BASE_FEATURE(kShortcutCustomization,
             "ShortcutCustomization",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsShortcutCustomizationEnabled() {
  return base::FeatureList::IsEnabled(kShortcutCustomization);
}

// Share the resource file with ash-chrome. This feature reduces the memory
// consumption while the disk usage slightly increases.
// https://crbug.com/1253280.
BASE_FEATURE(kLacrosResourcesFileSharing,
             "LacrosResourcesFileSharing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When the input method wants to commit the composition, always call
// ConfirmCompositionText even if Ash thinks there's no composition.
// Enabling this fixes b/265853952.
BASE_FEATURE(kAlwaysConfirmComposition,
             "AlwaysConfirmComposition",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kRedundantImeCompositionClearing,
             "RedundantImeCompositionClearing",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)

// Update of the virtual keyboard settings UI as described in
// https://crbug.com/876901.
BASE_FEATURE(kInputMethodSettingsUiUpdate,
             "InputMethodSettingsUiUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
BASE_FEATURE(kWindowsScrollingPersonality,
             "WindowsScrollingPersonality",
             kWindowsScrollingPersonalityDefaultStatus);

bool IsPercentBasedScrollingEnabled() {
  return base::FeatureList::IsEnabled(features::kWindowsScrollingPersonality);
}

// Uses a stylus-specific tap slop region parameter for gestures.  Stylus taps
// tend to slip more than touch taps (presumably because the user doesn't feel
// the movement friction with a stylus).  As a result, it is harder to tap with
// a stylus. This feature makes the slop region for stylus input bigger than the
// touch slop.
BASE_FEATURE(kStylusSpecificTapSlop,
             "StylusSpecificTapSlop",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allows system caption style for WebVTT Captions.
BASE_FEATURE(kSystemCaptionStyle,
             "SystemCaptionStyle",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the feature will query the OS for a default cursor size,
// to be used in determining the concrete object size of a custom cursor in
// blink. Currently enabled by default on Windows only.
// TODO(crbug.com/1333523) - Implement for other platforms.
BASE_FEATURE(kSystemCursorSizeSupported,
             "SystemCursorSizeSupported",
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

bool IsSystemCursorSizeSupported() {
  return base::FeatureList::IsEnabled(kSystemCursorSizeSupported);
}

// Allows system keyboard event capture via the keyboard lock API.
BASE_FEATURE(kSystemKeyboardLock,
             "SystemKeyboardLock",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables GPU rasterization for all UI drawing (where not blocklisted).
BASE_FEATURE(kUiGpuRasterization,
             "UiGpuRasterization",
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_CHROMEOS_LACROS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

bool IsUiGpuRasterizationEnabled() {
  return base::FeatureList::IsEnabled(kUiGpuRasterization);
}

// Enables scrolling with layers under ui using the ui::Compositor.
BASE_FEATURE(kUiCompositorScrollWithLayers,
             "UiCompositorScrollWithLayers",
// TODO(https://crbug.com/615948): Use composited scrolling on all platforms.
#if BUILDFLAG(IS_APPLE)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enables the use of a touch fling curve that is based on the behavior of
// native apps on Windows.
BASE_FEATURE(kExperimentalFlingAnimation,
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
);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
// Cached in Java as well, make sure defaults are updated together.
BASE_FEATURE(kElasticOverscroll,
             "ElasticOverscroll",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else  // BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

// Enables focus follow follow cursor (sloppyfocus).
BASE_FEATURE(kFocusFollowsCursor,
             "FocusFollowsCursor",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// Enables InputPane API for controlling on screen keyboard.
BASE_FEATURE(kInputPaneOnScreenKeyboard,
             "InputPaneOnScreenKeyboard",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables using WM_POINTER instead of WM_TOUCH for touch events.
BASE_FEATURE(kPointerEventsForTouch,
             "PointerEventsForTouch",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Enables using TSF (over IMM32) for IME.
BASE_FEATURE(kTSFImeSupport, "TSFImeSupport", base::FEATURE_ENABLED_BY_DEFAULT);

bool IsUsingWMPointerForTouch() {
  return base::FeatureList::IsEnabled(kPointerEventsForTouch);
}

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
// This feature supercedes kNewShortcutMapping.
BASE_FEATURE(kImprovedKeyboardShortcuts,
             "ImprovedKeyboardShortcuts",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
BASE_FEATURE(kDeprecateAltBasedSixPack,
             "DeprecateAltBasedSixPack",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDeprecateAltBasedSixPackEnabled() {
  return base::FeatureList::IsEnabled(kDeprecateAltBasedSixPack);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Whether to enable new touch text editing features such as extra touch
// selection gestures and quick menu options. Planning to release for ChromeOS
// first, then possibly also enable some parts for other platforms later.
// TODO(b/262297017): Clean up after touch text editing redesign ships.
BASE_FEATURE(kTouchTextEditingRedesign,
             "TouchTextEditingRedesign",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTouchTextEditingRedesignEnabled() {
  return base::FeatureList::IsEnabled(kTouchTextEditingRedesign);
}

// Enables forced colors mode for web content.
BASE_FEATURE(kForcedColors, "ForcedColors", base::FEATURE_ENABLED_BY_DEFAULT);

bool IsForcedColorsEnabled() {
  static const bool forced_colors_enabled =
      base::FeatureList::IsEnabled(features::kForcedColors);
  return forced_colors_enabled;
}

// Enables the eye-dropper in the refresh color-picker for Windows, Mac
// and Linux. This feature will be released for other platforms in later
// milestones.
BASE_FEATURE(kEyeDropper,
             "EyeDropper",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

bool IsEyeDropperEnabled() {
  return base::FeatureList::IsEnabled(features::kEyeDropper);
}

// Enable the common select popup.
BASE_FEATURE(kUseCommonSelectPopup,
             "UseCommonSelectPopup",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsUseCommonSelectPopupEnabled() {
  return base::FeatureList::IsEnabled(features::kUseCommonSelectPopup);
}

// Enables keyboard accessible tooltip.
BASE_FEATURE(kKeyboardAccessibleTooltip,
             "KeyboardAccessibleTooltip",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsKeyboardAccessibleTooltipEnabled() {
  static const bool keyboard_accessible_tooltip_enabled =
      base::FeatureList::IsEnabled(features::kKeyboardAccessibleTooltip);
  return keyboard_accessible_tooltip_enabled;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
BASE_FEATURE(kHandwritingGesture,
             "HandwritingGesture",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSynchronousPageFlipTesting,
             "SynchronousPageFlipTesting",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsSynchronousPageFlipTestingEnabled() {
  return base::FeatureList::IsEnabled(kSynchronousPageFlipTesting);
}

BASE_FEATURE(kResamplingScrollEventsExperimentalPrediction,
             "ResamplingScrollEventsExperimentalPrediction",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kSwipeToMoveCursor,
             "SwipeToMoveCursor",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUIDebugTools,
             "ui-debug-tools",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSwipeToMoveCursorEnabled() {
  static const bool enabled =
#if BUILDFLAG(IS_ANDROID)
      base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_R;
#else
      base::FeatureList::IsEnabled(kSwipeToMoveCursor) ||
      IsTouchTextEditingRedesignEnabled();
#endif
  return enabled;
}

// Enable raw draw for tiles.
BASE_FEATURE(kRawDraw, "RawDraw", base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kEnableVariableRefreshRate,
             "EnableVariableRefreshRate",
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsVariableRefreshRateEnabled() {
  return base::FeatureList::IsEnabled(kEnableVariableRefreshRate);
}

// Fixes b/267944900.
BASE_FEATURE(kWaylandKeepSelectionFix,
             "WaylandKeepSelectionFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Fixes b/267944900.
BASE_FEATURE(kWaylandCancelComposition,
             "WaylandCancelComposition",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables chrome color management wayland protocol for lacros.
BASE_FEATURE(kLacrosColorManagement,
             "LacrosColorManagement",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsLacrosColorManagementEnabled() {
  return base::FeatureList::IsEnabled(kLacrosColorManagement);
}

BASE_FEATURE(kCustomizeChromeSidePanel,
             "CustomizeChromeSidePanel",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCustomizeChromeSidePanelNoChromeRefresh2023,
             "CustomizeChromeSidePanelNoChromeRefresh2023",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool CustomizeChromeSupportsChromeRefresh2023() {
  return base::FeatureList::IsEnabled(kCustomizeChromeSidePanel) &&
         !base::FeatureList::IsEnabled(
             kCustomizeChromeSidePanelNoChromeRefresh2023);
}

BASE_FEATURE(kChromeRefresh2023,
             "ChromeRefresh2023",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeRefreshSecondary2023,
             "ChromeRefreshSecondary2023",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsChromeRefresh2023() {
  if (!CustomizeChromeSupportsChromeRefresh2023()) {
    // Bail before checking any other feature flags so that associated studies
    // don't get activated.
    return false;
  }
  return base::FeatureList::IsEnabled(kChromeRefresh2023) ||
         base::FeatureList::IsEnabled(kChromeRefreshSecondary2023);
}

BASE_FEATURE(kChromeWebuiRefresh2023,
             "ChromeWebuiRefresh2023",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsChromeWebuiRefresh2023() {
  if (!CustomizeChromeSupportsChromeRefresh2023()) {
    // Bail before checking any other feature flags so that associated studies
    // don't get activated.
    return false;
  }
  return IsChromeRefresh2023() &&
         (base::FeatureList::IsEnabled(kChromeWebuiRefresh2023) ||
          base::FeatureList::IsEnabled(kChromeRefreshSecondary2023));
}

constexpr base::FeatureParam<ChromeRefresh2023Level>::Option
    kChromeRefresh2023LevelOption[] = {{ChromeRefresh2023Level::kLevel1, "1"},
                                       {ChromeRefresh2023Level::kLevel2, "2"}};

const base::FeatureParam<ChromeRefresh2023Level> kChromeRefresh2023Level(
    &kChromeRefresh2023,
    "level",
    ChromeRefresh2023Level::kLevel2,
    &kChromeRefresh2023LevelOption);

ChromeRefresh2023Level GetChromeRefresh2023LevelUncached() {
  if (!CustomizeChromeSupportsChromeRefresh2023()) {
    // Bail before checking any other feature flags so that associated studies
    // don't get activated.
    return ChromeRefresh2023Level::kDisabled;
  }
  // For simplicity, the secondary field trial to enable chrome refresh will
  // also enable the omnibox refresh.
  if (base::FeatureList::IsEnabled(kChromeRefreshSecondary2023)) {
    return ChromeRefresh2023Level::kLevel2;
  }

  return IsChromeRefresh2023() ? kChromeRefresh2023Level.Get()
                               : ChromeRefresh2023Level::kDisabled;
}

ChromeRefresh2023Level GetChromeRefresh2023Level() {
  // Cached due to frequent calls for performance optimization.
  // Please update `GetChromeRefresh2023LevelUncached()` for any changes.
  static const ChromeRefresh2023Level level =
      GetChromeRefresh2023LevelUncached();
  return level;
}

#if !BUILDFLAG(IS_LINUX)
BASE_FEATURE(kWebUiSystemFont,
             "WebUiSystemFont",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_MAC)
// When enabled, images will be written to the system clipboard as both a TIFF
// and a PNG (as opposed to just a TIFF). This requires encoding the sanitized
// bitmap to a PNG on the UI thread on copy, which may cause jank. This matches
// the behavior of other platforms.
// TODO(https://crbug.com/1443646): Remove this flag eventually.
BASE_FEATURE(kMacClipboardWriteImageWithPng,
             "MacClipboardWriteImageWithPng",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_APPLE)
// Font Smoothing was enabled by default prior to introducing this feature.
// We want to experiment with disabling it to align with CR2023 designs.
BASE_FEATURE(kCr2023MacFontSmoothing,
             "Cr2023MacFontSmoothing",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

}  // namespace features
