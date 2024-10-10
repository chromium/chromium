// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_features.h"

#include <stdlib.h>

#include "base/command_line.h"
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_LACROS)
// Once enabled, the exact behavior is dictated by the field trial param
// name `kApplyNativeOcclusionToCompositorType`.
BASE_FEATURE(kApplyNativeOcclusionToCompositor,
             "ApplyNativeOcclusionToCompositor",
#if BUILDFLAG(IS_CHROMEOS_LACROS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// If enabled, native window occlusion tracking will always be used, even if
// CHROME_HEADLESS is set.
BASE_FEATURE(kAlwaysTrackNativeWindowOcclusionForTest,
             "AlwaysTrackNativeWindowOcclusionForTest",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Field trial param name for `kApplyNativeOcclusionToCompositor`.
const base::FeatureParam<std::string> kApplyNativeOcclusionToCompositorType{
    &kApplyNativeOcclusionToCompositor, "type",
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    /*default=*/"throttle_and_release"
#else
    /*default=*/""
#endif
};

// When the WindowTreeHost is occluded or hidden, resources are released and
// the compositor is hidden. See WindowTreeHost for specifics on what this
// does.
const char kApplyNativeOcclusionToCompositorTypeRelease[] = "release";
// When the WindowTreeHost is occluded the frame rate is throttled.
const char kApplyNativeOcclusionToCompositorTypeThrottle[] = "throttle";
// Release when hidden, throttle when occluded.
const char kApplyNativeOcclusionToCompositorTypeThrottleAndRelease[] =
    "throttle_and_release";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Integrate input method specific settings to Chrome OS settings page.
// https://crbug.com/895886.
BASE_FEATURE(kSettingsShowsPerKeyboardSettings,
             "InputMethodIntegratedSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDeprecateAltClick,
             "DeprecateAltClick",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDeprecateAltClickEnabled() {
  return base::FeatureList::IsEnabled(kDeprecateAltClick);
}

BASE_FEATURE(kNotificationsIgnoreRequireInteraction,
             "NotificationsIgnoreRequireInteraction",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsNotificationsIgnoreRequireInteractionEnabled() {
  return base::FeatureList::IsEnabled(kNotificationsIgnoreRequireInteraction);
}

BASE_FEATURE(kShortcutCustomization,
             "ShortcutCustomization",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsShortcutCustomizationEnabled() {
  return base::FeatureList::IsEnabled(kShortcutCustomization);
}

// Share the resource file with ash-chrome. This feature reduces the memory
// consumption while the disk usage slightly increases.
// https://crbug.com/1253280.
BASE_FEATURE(kLacrosResourcesFileSharing,
             "LacrosResourcesFileSharing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables settings that allow users to remap the F11 and F12 keys in the
// "Customize keyboard keys" page.
BASE_FEATURE(kSupportF11AndF12KeyShortcuts,
             "SupportF11AndF12KeyShortcuts",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool AreF11AndF12ShortcutsEnabled() {
  // TODO(crbug.com/40203434): Remove this once kDeviceI18nShortcutsEnabled
  // policy is deprecated. This policy allows managed users to still be able to
  // use deprecated legacy shortcuts which some enterprise customers rely on.
  if (::ui::ShortcutMappingPrefDelegate::IsInitialized()) {
    ::ui::ShortcutMappingPrefDelegate* instance =
        ::ui::ShortcutMappingPrefDelegate::GetInstance();
    if (instance && instance->IsDeviceEnterpriseManaged()) {
      return instance->IsI18nShortcutPrefEnabled() &&
             base::FeatureList::IsEnabled(
                 features::kSupportF11AndF12KeyShortcuts);
    }
  }
  return base::FeatureList::IsEnabled(features::kSupportF11AndF12KeyShortcuts);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_OZONE)
BASE_FEATURE(kOzoneBubblesUsePlatformWidgets,
             "OzoneBubblesUsePlatformWidgets",
#if BUILDFLAG(IS_CHROMEOS_LACROS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Controls whether support for Wayland's per-surface scaling is enabled.
BASE_FEATURE(kWaylandPerSurfaceScale,
             "WaylandPerSurfaceScale",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether Wayland text-input-v3 protocol support is enabled.
BASE_FEATURE(kWaylandTextInputV3,
             "WaylandTextInputV3",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether support for "Large Text" accessibility setting via UI
// scaling is enabled.
BASE_FEATURE(kWaylandUiScale,
             "WaylandUiScale",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_OZONE)

#if BUILDFLAG(IS_LINUX)
// If this feature is enabled, users not specify --ozone-platform-hint switch
// will get --ozone-platform-hint=auto treatment. https://crbug.com/40250220.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_FEATURE(kOverrideDefaultOzonePlatformHintToAuto,
             "OverrideDefaultOzonePlatformHintToAuto",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX)

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
// TODO(crbug.com/40845719) - Implement for other platforms.
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
// TODO(crbug.com/40471184): Use composited scrolling on all platforms.
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
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) ||                                   \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
     !BUILDFLAG(IS_CHROMEOS_LACROS))
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kClipboardFiles,
             "ClipboardFiles",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDragDropEmpty, "DragDropEmpty", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDragDropFiles, "DragDropFiles", base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

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
// Enables using WM_POINTER instead of WM_TOUCH for touch events.
BASE_FEATURE(kPointerEventsForTouch,
             "PointerEventsForTouch",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsUsingWMPointerForTouch() {
  return base::FeatureList::IsEnabled(kPointerEventsForTouch);
}

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
// This feature supersedes kNewShortcutMapping.
BASE_FEATURE(kImprovedKeyboardShortcuts,
             "ImprovedKeyboardShortcuts",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsImprovedKeyboardShortcutsEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/40203434): Remove this once kDeviceI18nShortcutsEnabled
  // policy is deprecated.
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

#endif  // BUILDFLAG(IS_CHROMEOS)

// Whether to enable new touch text editing features such as extra touch
// selection gestures and quick menu options. Planning to release for ChromeOS
// first, then possibly also enable some parts for other platforms later.
// TODO(b/262297017): Clean up after touch text editing redesign ships.
BASE_FEATURE(kTouchTextEditingRedesign,
             "TouchTextEditingRedesign",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

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
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

bool IsEyeDropperEnabled() {
  return base::FeatureList::IsEnabled(features::kEyeDropper);
}

// Used to enable keyboard accessible tooltips in in-page content
// (i.e., inside Blink). See
// ::views::features::kKeyboardAccessibleTooltipInViews for
// keyboard-accessible tooltips in Views UI.
BASE_FEATURE(kKeyboardAccessibleTooltip,
             "KeyboardAccessibleTooltip",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsKeyboardAccessibleTooltipEnabled() {
  static const bool keyboard_accessible_tooltip_enabled =
      base::FeatureList::IsEnabled(features::kKeyboardAccessibleTooltip);
  return keyboard_accessible_tooltip_enabled;
}

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

BASE_FEATURE(kVariableRefreshRateAvailable,
             "VariableRefreshRateAvailable",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableVariableRefreshRate,
             "EnableVariableRefreshRate",
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsVariableRefreshRateEnabled() {
  if (base::FeatureList::IsEnabled(kEnableVariableRefreshRateAlwaysOn)) {
    return true;
  }

  // Special default case for devices with inverted default behavior, indicated
  // by |kVariableRefreshRateAvailable|. If |kEnableVariableRefreshRate| is not
  // overridden, then VRR is enabled by default.
  if (!(base::FeatureList::GetInstance() &&
        base::FeatureList::GetInstance()->IsFeatureOverridden(
            kEnableVariableRefreshRate.name)) &&
      base::FeatureList::IsEnabled(kVariableRefreshRateAvailable)) {
    return true;
  }

  return base::FeatureList::IsEnabled(kEnableVariableRefreshRate);
}
BASE_FEATURE(kEnableVariableRefreshRateAlwaysOn,
             "EnableVariableRefreshRateAlwaysOn",
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsVariableRefreshRateAlwaysOn() {
  return base::FeatureList::IsEnabled(kEnableVariableRefreshRateAlwaysOn);
}

// Enables chrome color management wayland protocol for lacros.
BASE_FEATURE(kLacrosColorManagement,
             "LacrosColorManagement",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsLacrosColorManagementEnabled() {
  return base::FeatureList::IsEnabled(kLacrosColorManagement);
}

BASE_FEATURE(kBubbleMetricsApi,
             "BubbleMetricsApi",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_APPLE)
// Font Smoothing was enabled by default prior to introducing this feature.
// We want to experiment with disabling it to align with CR2023 designs.
BASE_FEATURE(kCr2023MacFontSmoothing,
             "Cr2023MacFontSmoothing",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kUseGammaContrastRegistrySettings,
             "UseGammaContrastRegistrySettings",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIncreaseWindowsTextContrast,
             "IncreaseWindowsTextContrast",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

BASE_FEATURE(kBubbleFrameViewTitleIsHeading,
             "BubbleFrameViewTitleIsHeading",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableGestureBeginEndTypes,
             "EnableGestureBeginEndTypes",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseUtf8EncodingForSvgImage,
             "UseUtf8EncodingForSvgImage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables copy bookmark and writes url format to clipboard with empty title.
BASE_FEATURE(kWriteBookmarkWithoutTitle,
             "WriteBookmarkWithoutTitle",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, fullscreen window state is updated asynchronously.
BASE_FEATURE(kAsyncFullscreenWindowState,
             "AsyncFullscreenWindowState",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
