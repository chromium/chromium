// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_features.h"

#include "ui/base/ui_base_switches_util.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace features {

// If enabled, the emoji picker context menu item may be shown for editable
// text areas.
const base::Feature kEnableEmojiContextMenu {
  "EnableEmojiContextMenu",
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_CHROMEOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enables the floating virtual keyboard behavior.
const base::Feature kEnableFloatingVirtualKeyboard = {
    "enable-floating-virtual-keyboard", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the full screen handwriting virtual keyboard behavior.
const base::Feature kEnableFullscreenHandwritingVirtualKeyboard = {
    "enable-fullscreen-handwriting-virtual-keyboard",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableStylusVirtualKeyboard = {
    "enable-stylus-virtual-keyboard", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, uses the Material Design UI for virtual keyboard.
const base::Feature kEnableVirtualKeyboardMdUi = {
    "EnableVirtualKeyboardMdUi", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnableVirtualKeyboardUkm = {
    "EnableVirtualKeyboardUkm", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables all upcoming UI features.
const base::Feature kExperimentalUi{"ExperimentalUi",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Allows system keyboard event capture when |features::kKeyboardLockApi| is on.
const base::Feature kSystemKeyboardLock{"SystemKeyboardLock",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kTouchableAppContextMenu = {
    "EnableTouchableAppContextMenu", base::FEATURE_ENABLED_BY_DEFAULT};

bool IsTouchableAppContextMenuEnabled() {
  return base::FeatureList::IsEnabled(kTouchableAppContextMenu) ||
         switches::IsTouchableAppContextMenuEnabled();
}

const base::Feature kNotificationIndicator = {
    "EnableNotificationIndicator", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsNotificationIndicatorEnabled() {
  return base::FeatureList::IsEnabled(kNotificationIndicator);
}

// Enables GPU rasterization for all UI drawing (where not blacklisted).
const base::Feature kUiGpuRasterization = {"UiGpuRasterization",
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
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
#if defined(OS_MACOSX)
    base::FEATURE_ENABLED_BY_DEFAULT
#else
    base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

#if defined(OS_WIN)
// Enables InputPane API for controlling on screen keyboard.
const base::Feature kInputPaneOnScreenKeyboard = {
    "InputPaneOnScreenKeyboard", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables using WM_POINTER instead of WM_TOUCH for touch events.
const base::Feature kPointerEventsForTouch = {"PointerEventsForTouch",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
// Enables using TSF (over IMM32) for IME.
const base::Feature kTSFImeSupport = {"TSFImeSupport",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

bool IsUsingWMPointerForTouch() {
  return base::win::GetVersion() >= base::win::VERSION_WIN8 &&
         base::FeatureList::IsEnabled(kPointerEventsForTouch);
}

// Enables DirectManipulation API for processing Precision Touchpad events.
const base::Feature kPrecisionTouchpad{"PrecisionTouchpad",
                                       base::FEATURE_ENABLED_BY_DEFAULT};
// Enables Swipe left/right to navigation back/forward API for processing
// Precision Touchpad events.
const base::Feature kPrecisionTouchpadScrollPhase{
    "PrecisionTouchpadScrollPhase", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
// Enables stylus appearing as touch when in contact with digitizer.
const base::Feature kDirectManipulationStylus = {
    "DirectManipulationStylus",
#if defined(OS_WIN)
    base::FEATURE_ENABLED_BY_DEFAULT
#else
    base::FEATURE_DISABLED_BY_DEFAULT
#endif
};
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

const base::Feature kMash = {"Mash", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSingleProcessMash = {"SingleProcessMash",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

bool IsUsingWindowService() {
  return IsSingleProcessMash() || IsMultiProcessMash();
}

bool IsMultiProcessMash() {
  return base::FeatureList::IsEnabled(features::kMash);
}

bool IsSingleProcessMash() {
  return base::FeatureList::IsEnabled(features::kSingleProcessMash);
}

#if defined(OS_MACOSX)
// When enabled, the NSWindows for apps will be created in the app's process,
// and will forward input to the browser process.
const base::Feature kHostWindowsInAppShimProcess{
    "HostWindowsInAppShimProcess", base::FEATURE_DISABLED_BY_DEFAULT};

bool HostWindowsInAppShimProcess() {
  return base::FeatureList::IsEnabled(kHostWindowsInAppShimProcess);
}
#endif  //  defined(OS_MACOSX)

const base::Feature kEnableOzoneDrmMojo = {"OzoneDrmMojo",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

bool IsOzoneDrmMojo() {
  return base::FeatureList::IsEnabled(kEnableOzoneDrmMojo) ||
         IsMultiProcessMash();
}

const base::Feature kDarkMode = {"DarkMode", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
