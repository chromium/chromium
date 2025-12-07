// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_features.h"

#include <stdlib.h>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_info.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/base/shortcut_mapping_pref_delegate.h"
#endif

namespace features {

// If enabled, generates an empty GestureScrollUpdate if the preceding TouchMove
// event had no gestures and sends both events together.
BASE_FEATURE(kSendEmptyGestureScrollUpdate,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE_PARAM(bool,
                   kSendEmptyGestureScrollUpdateFilterOutEmptyUpdates,
                   &kSendEmptyGestureScrollUpdate,
                   "filter_out_empty_updates",
                   false);

#if BUILDFLAG(IS_WIN)
// If enabled, calculate native window occlusion - Windows-only.
BASE_FEATURE(kCalculateNativeWinOcclusion, base::FEATURE_ENABLED_BY_DEFAULT);

// Once enabled, the exact behavior is dictated by the field trial param
// name `kApplyNativeOcclusionToCompositorType`.
BASE_FEATURE(kApplyNativeOcclusionToCompositor,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, native window occlusion tracking will always be used, even if
// CHROME_HEADLESS is set.
BASE_FEATURE(kAlwaysTrackNativeWindowOcclusionForTest,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Field trial param name for `kApplyNativeOcclusionToCompositor`.
const base::FeatureParam<std::string> kApplyNativeOcclusionToCompositorType{
    &kApplyNativeOcclusionToCompositor, "type", /*default=*/""};

// When the WindowTreeHost is occluded or hidden, resources are released and
// the compositor is hidden. See WindowTreeHost for specifics on what this
// does.
const char kApplyNativeOcclusionToCompositorTypeRelease[] = "release";
// When the WindowTreeHost is occluded the frame rate is throttled.
const char kApplyNativeOcclusionToCompositorTypeThrottle[] = "throttle";
// Release when hidden, throttle when occluded.
const char kApplyNativeOcclusionToCompositorTypeThrottleAndRelease[] =
    "throttle_and_release";
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
// Integrate input method specific settings to Chrome OS settings page.
// https://crbug.com/895886.
BASE_FEATURE(kSettingsShowsPerKeyboardSettings,
             "InputMethodIntegratedSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDeprecateAltClick, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDeprecateAltClickEnabled() {
  return base::FeatureList::IsEnabled(kDeprecateAltClick);
}

BASE_FEATURE(kNotificationsIgnoreRequireInteraction,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsNotificationsIgnoreRequireInteractionEnabled() {
  return base::FeatureList::IsEnabled(kNotificationsIgnoreRequireInteraction);
}

// Enables settings that allow users to remap the F11 and F12 keys in the
// "Customize keyboard keys" page.
BASE_FEATURE(kSupportF11AndF12KeyShortcuts, base::FEATURE_ENABLED_BY_DEFAULT);

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
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_OZONE)
BASE_FEATURE(kOzoneBubblesUsePlatformWidgets, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether support for Wayland's linux-drm-syncobj is enabled.
BASE_FEATURE(kWaylandLinuxDrmSyncobj, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether support for Wayland's per-surface scaling is enabled.
BASE_FEATURE(kWaylandPerSurfaceScale,
#if BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_LINUX)
);

// Controls whether Wayland text-input-v3 protocol support is enabled.
BASE_FEATURE(kWaylandTextInputV3, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether Wayland session management protocol is enabled.
BASE_FEATURE(kWaylandSessionManagement, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_OZONE)

// When enabled, the feature will query the OS for a default cursor size,
// to be used in determining the concrete object size of a custom cursor in
// blink. Currently enabled by default on Windows only.
// TODO(crbug.com/40845719) - Implement for other platforms.
BASE_FEATURE(kSystemCursorSizeSupported,
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

bool IsSystemCursorSizeSupported() {
  return base::FeatureList::IsEnabled(kSystemCursorSizeSupported);
}

// When enabled, uses a WinEvent hook to track system cursor visibility
// changes. This is only available on Windows.
BASE_FEATURE(kUseCursorEventHook, base::FEATURE_DISABLED_BY_DEFAULT);

bool ShouldUseCursorEventHook() {
  return base::FeatureList::IsEnabled(kUseCursorEventHook);
}

// Allows system keyboard event capture via the keyboard lock API.
BASE_FEATURE(kSystemKeyboardLock, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables GPU rasterization for all UI drawing (where not blocklisted).
BASE_FEATURE(kUiGpuRasterization, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsUiGpuRasterizationEnabled() {
  return base::FeatureList::IsEnabled(kUiGpuRasterization);
}

// Enables scrolling with layers under ui using the ui::Compositor.
BASE_FEATURE(kUiCompositorScrollWithLayers,
// TODO(crbug.com/40471184): Use composited scrolling on all platforms.
#if BUILDFLAG(IS_APPLE)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// TODO(crbug.com/389771428): Switch the ui::Compositor to use
// cc::PropertyTrees and layer lists rather than layer trees.
BASE_FEATURE(kUiCompositorUsesLayerLists, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the use of a touch fling curve that is based on the behavior of
// native apps on Windows.
BASE_FEATURE(kExperimentalFlingAnimation,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if !BUILDFLAG(IS_APPLE)
// Cached in Java as well, make sure defaults are updated together.
BASE_FEATURE(kElasticOverscroll,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else  // BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
#endif

// Limits the scroll delta to the size of the scroller when scrolled using the
// mouse wheel only.
BASE_FEATURE(kLimitScrollDeltaToScrollerSize, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables focus follow follow cursor (sloppyfocus).
BASE_FEATURE(kFocusFollowsCursor, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDragDropOnlySynthesizeHttpOrHttpsUrlsFromText,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
bool IsImprovedKeyboardShortcutsEnabled() {
  // TODO(crbug.com/40203434): Remove this once kDeviceI18nShortcutsEnabled
  // policy is deprecated.
  if (::ui::ShortcutMappingPrefDelegate::IsInitialized()) {
    ::ui::ShortcutMappingPrefDelegate* instance =
        ::ui::ShortcutMappingPrefDelegate::GetInstance();
    if (instance && instance->IsDeviceEnterpriseManaged()) {
      return instance->IsI18nShortcutPrefEnabled();
    }
  }
  return true;
}

#endif  // BUILDFLAG(IS_CHROMEOS)

// Whether to enable new touch text editing features such as extra touch
// selection gestures and quick menu options. Planning to release for ChromeOS
// first, then possibly also enable some parts for other platforms later.
// TODO(b/262297017): Clean up after touch text editing redesign ships.
BASE_FEATURE(kTouchTextEditingRedesign,
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

bool IsTouchTextEditingRedesignEnabled() {
  return base::FeatureList::IsEnabled(kTouchTextEditingRedesign);
}

// This feature enables drag and drop using touch input devices.
BASE_FEATURE(kTouchDragAndDrop,
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

bool IsTouchDragAndDropEnabled() {
  static const bool touch_drag_and_drop_enabled =
      base::FeatureList::IsEnabled(kTouchDragAndDrop);
  return touch_drag_and_drop_enabled;
}

// Enables forced colors mode for web content.
BASE_FEATURE(kForcedColors, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsForcedColorsEnabled() {
  static const bool forced_colors_enabled =
      base::FeatureList::IsEnabled(features::kForcedColors);
  return forced_colors_enabled;
}

// Enables the eye-dropper in the refresh color-picker for Windows, Mac
// and Linux. This feature will be released for other platforms in later
// milestones.
BASE_FEATURE(kEyeDropper,
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
BASE_FEATURE(kKeyboardAccessibleTooltip, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsKeyboardAccessibleTooltipEnabled() {
  static const bool keyboard_accessible_tooltip_enabled =
      base::FeatureList::IsEnabled(features::kKeyboardAccessibleTooltip);
  return keyboard_accessible_tooltip_enabled;
}

BASE_FEATURE(kSynchronousPageFlipTesting, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsSynchronousPageFlipTestingEnabled() {
  return base::FeatureList::IsEnabled(kSynchronousPageFlipTesting);
}

BASE_FEATURE(kResamplingScrollEventsExperimentalPrediction,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kResampleScrollEventsLatency, base::FEATURE_DISABLED_BY_DEFAULT);

const char kResampleLatencyModeFixedMs[] = "fixed_ms";
const char kResampleLatencyModeFractional[] = "fractional";

const base::FeatureParam<std::string> kResampleLatencyModeParam(
    &kResampleScrollEventsLatency,
    "mode",
    kResampleLatencyModeFixedMs);

const base::FeatureParam<double>
    kResampleLatencyValueParam(&kResampleScrollEventsLatency, "value", -5.0);

const char kPredictorNameLsq[] = "lsq";
const char kPredictorNameKalman[] = "kalman";
const char kPredictorNameLinearFirst[] = "linear_first";
const char kPredictorNameLinearSecond[] = "linear_second";
const char kPredictorNameLinearResampling[] = "linear_resampling";
const char kPredictorNameEmpty[] = "empty";

const char kFilterNameEmpty[] = "empty_filter";
const char kFilterNameOneEuro[] = "one_euro_filter";

const char kPredictionTypeFramesBased[] = "frames";
const char kPredictionTypeDefaultFramesVariation1[] = "0.25";
const char kPredictionTypeDefaultFramesVariation2[] = "0.375";
const char kPredictionTypeDefaultFramesVariation3[] = "0.5";

BASE_FEATURE(kSwipeToMoveCursor, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUIDebugTools,
             "ui-debug-tools",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSwipeToMoveCursorEnabled() {
  static const bool enabled =
#if BUILDFLAG(IS_ANDROID)
      base::android::android_info::sdk_int() >=
      base::android::android_info::SDK_VERSION_R;
#else
      base::FeatureList::IsEnabled(kSwipeToMoveCursor) ||
      IsTouchTextEditingRedesignEnabled();
#endif
  return enabled;
}

// Enable raw draw for tiles.
BASE_FEATURE(kRawDraw, base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kVariableRefreshRateAvailable, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableVariableRefreshRate, base::FEATURE_DISABLED_BY_DEFAULT);
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
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsVariableRefreshRateAlwaysOn() {
  return base::FeatureList::IsEnabled(kEnableVariableRefreshRateAlwaysOn);
}

BASE_FEATURE(kBubbleMetricsApi, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kUseGammaContrastRegistrySettings,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

BASE_FEATURE(kBubbleFrameViewTitleIsHeading, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableGestureBeginEndTypes,
#if !BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // !BUILDFLAG(IS_CHROMEOS)
);

BASE_FEATURE(kUseUtf8EncodingForSvgImage, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables copy bookmark and writes url format to clipboard with empty title.
BASE_FEATURE(kWriteBookmarkWithoutTitle, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, fullscreen window state is updated asynchronously.
BASE_FEATURE(kAsyncFullscreenWindowState, base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag for enabling platform clipboard monitoring.
BASE_FEATURE(kPlatformClipboardMonitor, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, all draw commands recorded on canvas are done in pixel aligned
// measurements. This also enables scaling of all elements in views and layers
// to be done via corner points. See https://crbug.com/720596 for details.
BASE_FEATURE(kEnablePixelCanvasRecording,
             "enable-pixel-canvas-recording",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

bool IsPixelCanvasRecordingEnabled() {
  return base::FeatureList::IsEnabled(features::kEnablePixelCanvasRecording);
}

BASE_FEATURE(kHandleIMESpanChangesOnUpdateComposition,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsHandleIMESpanChangesOnUpdateCompositionEnabled() {
  return base::FeatureList::IsEnabled(
      features::kHandleIMESpanChangesOnUpdateComposition);
}

BASE_FEATURE(kUseSystemDefaultAccentColors, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kStringWidthCache, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
