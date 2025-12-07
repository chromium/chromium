// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by ui/base.

#ifndef UI_BASE_UI_BASE_SWITCHES_H_
#define UI_BASE_UI_BASE_SWITCHES_H_

#include "base/component_export.h"
#include "build/build_config.h"

namespace switches {

#if BUILDFLAG(IS_ANDROID)
// Disable overscroll edge effects like those found in Android views.
inline constexpr char kDisableOverscrollEdgeEffect[] =
    "disable-overscroll-edge-effect";

// Disable the pull-to-refresh effect when vertically overscrolling content.
inline constexpr char kDisablePullToRefreshEffect[] =
    "disable-pull-to-refresh-effect";

// Enables drawing debug layers for edge-to-edge components to highlight the
// system insets those components are drawing into.
// LINT.IfChange(EnableEdgeToEdgeDebugLayers)
inline constexpr char kEnableEdgeToEdgeDebugLayers[] =
    "enable-edge-to-edge-debug-layers";
// LINT.ThenChange(//ui/android/java/src/org/chromium/ui/UiSwitches.java:EnableEdgeToEdgeDebugLayers)
#endif

#if BUILDFLAG(IS_MAC)
// Disable animations for showing and hiding modal dialogs.
inline constexpr char kDisableModalAnimations[] = "disable-modal-animations";

// Show borders around CALayers corresponding to overlays and partial damage.
inline constexpr char kShowMacOverlayBorders[] = "show-mac-overlay-borders";
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Specifies system font family name. Improves determinism when rendering pages
// in headless mode.
inline constexpr char kSystemFontFamily[] = "system-font-family";
#endif

#if BUILDFLAG(IS_LINUX)
// Specify the toolkit used to construct the Linux GUI.
inline constexpr char kUiToolkitFlag[] = "ui-toolkit";
// Specify the GTK version to be loaded.
inline constexpr char kGtkVersionFlag[] = "gtk-version";
// Specify the QT version to be loaded.
inline constexpr char kQtVersionFlag[] = "qt-version";
// Disables GTK IME integration.
inline constexpr char kDisableGtkIme[] = "disable-gtk-ime";
#endif

// Treats DRM virtual connector as external to enable display mode change in VM.
inline constexpr char kDRMVirtualConnectorIsExternal[] =
    "drm-virtual-connector-is-external";

// Forces the caption style for WebVTT captions.
inline constexpr char kForceCaptionStyle[] = "force-caption-style";

// Forces dark mode in UI for platforms that support it.
inline constexpr char kForceDarkMode[] = "force-dark-mode";

// Forces high-contrast mode for native UI and web content, regardless of system
// settings.
inline constexpr char kForceHighContrast[] = "force-high-contrast";

// The language file that we want to try to open. Of the form
// language[-country] where language is the 2 letter code from ISO-639.
// On Linux, this flag does not work; use the LC_*/LANG environment variables
// instead.
inline constexpr char kLang[] = "lang";

// Visualize overdraw by color-coding elements based on if they have other
// elements drawn underneath. This is good for showing where the UI might be
// doing more rendering work than necessary. The colors are hinting at the
// amount of overdraw on your screen for each pixel, as follows:
//
// True color: No overdraw.
// Blue: Overdrawn once.
// Green: Overdrawn twice.
// Pink: Overdrawn three times.
// Red: Overdrawn four or more times.
inline constexpr char kShowOverdrawFeedback[] = "show-overdraw-feedback";

// Re-draw everything multiple times to simulate a much slower machine.
// Give a slow down factor to cause renderer to take that many times longer to
// complete, such as --slow-down-compositing-scale-factor=2.
inline constexpr char kSlowDownCompositingScaleFactor[] =
    "slow-down-compositing-scale-factor";

// Tint composited color.
inline constexpr char kTintCompositedContent[] = "tint-composited-content";

// Controls touch-optimized UI layout for top chrome.
inline constexpr char kTopChromeTouchUi[] = "top-chrome-touch-ui";
inline constexpr char kTopChromeTouchUiAuto[] = "auto";
inline constexpr char kTopChromeTouchUiDisabled[] = "disabled";
inline constexpr char kTopChromeTouchUiEnabled[] = "enabled";

// Disable partial swap which is needed for some OpenGL drivers / emulators.
inline constexpr char kUIDisablePartialSwap[] = "ui-disable-partial-swap";

// Enables the ozone x11 clipboard for linux-chromeos.
inline constexpr char kUseSystemClipboard[] = "use-system-clipboard";

// Test related.
// Disable re-use of non-exact resources to fulfill ResourcePool requests.
// Intended only for use in layout or pixel tests to reduce noise.
inline constexpr char kDisallowNonExactResourceReuse[] =
    "disallow-non-exact-resource-reuse";

// Transform localized strings to be longer, with beginning and end markers to
// make truncation visually apparent.
inline constexpr char kMangleLocalizedStrings[] = "mangle-localized-strings";

}  // namespace switches

#endif  // UI_BASE_UI_BASE_SWITCHES_H_
