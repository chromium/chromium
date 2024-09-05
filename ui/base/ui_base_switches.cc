// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_switches.h"

#include "build/build_config.h"

namespace switches {

#if BUILDFLAG(IS_ANDROID)
// Disable overscroll edge effects like those found in Android views.
const char kDisableOverscrollEdgeEffect[] = "disable-overscroll-edge-effect";

// Disable the pull-to-refresh effect when vertically overscrolling content.
const char kDisablePullToRefreshEffect[] = "disable-pull-to-refresh-effect";
#endif

#if BUILDFLAG(IS_MAC)
// Disable animations for showing and hiding modal dialogs.
const char kDisableModalAnimations[] = "disable-modal-animations";

// Show borders around CALayers corresponding to overlays and partial damage.
const char kShowMacOverlayBorders[] = "show-mac-overlay-borders";
#endif

#if BUILDFLAG(IS_CHROMEOS)
// Enable resources file sharing with ash-chrome.
// This flag is enabled when feature::kLacrosResourcesFileSharing is set and
// ash-side operation is successfully done.
const char kEnableResourcesFileSharing[] = "enable-resources-file-sharing";
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Specifies system font family name. Improves determenism when rendering
// pages in headless mode.
const char kSystemFontFamily[] = "system-font-family";
#endif

#if BUILDFLAG(IS_LINUX)
// Specify the toolkit used to construct the Linux GUI.
const char kUiToolkitFlag[] = "ui-toolkit";
// Disables GTK IME integration.
const char kDisableGtkIme[] = "disable-gtk-ime";
#endif

// Disables layer-edge anti-aliasing in the compositor.
const char kDisableCompositedAntialiasing[] = "disable-composited-antialiasing";

// Disables touch event based drag and drop.
const char kDisableTouchDragDrop[] = "disable-touch-drag-drop";

// Disable re-use of non-exact resources to fulfill ResourcePool requests.
// Intended only for use in layout or pixel tests to reduce noise.
const char kDisallowNonExactResourceReuse[] =
    "disallow-non-exact-resource-reuse";

// Treats DRM virtual connector as external to enable display mode change in VM.
const char kDRMVirtualConnectorIsExternal[] =
    "drm-virtual-connector-is-external";

// Enables touch event based drag and drop.
const char kEnableTouchDragDrop[] = "enable-touch-drag-drop";

// Forces the caption style for WebVTT captions.
const char kForceCaptionStyle[] = "force-caption-style";

// Forces dark mode in UI for platforms that support it.
const char kForceDarkMode[] = "force-dark-mode";

// Forces high-contrast mode in native UI drawing, regardless of system
// settings. Note that this has limited effect on Windows: only Aura colors will
// be switched to high contrast, not other system colors.
const char kForceHighContrast[] = "force-high-contrast";

// The language file that we want to try to open. Of the form
// language[-country] where language is the 2 letter code from ISO-639.
// On Linux, this flag does not work; use the LC_*/LANG environment variables
// instead.
const char kLang[] = "lang";

// Transform localized strings to be longer, with beginning and end markers to
// make truncation visually apparent.
const char kMangleLocalizedStrings[] = "mangle-localized-strings";

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
const char kShowOverdrawFeedback[] = "show-overdraw-feedback";

// Re-draw everything multiple times to simulate a much slower machine.
// Give a slow down factor to cause renderer to take that many times longer to
// complete, such as --slow-down-compositing-scale-factor=2.
const char kSlowDownCompositingScaleFactor[] =
    "slow-down-compositing-scale-factor";

// Tint composited color.
const char kTintCompositedContent[] = "tint-composited-content";

// Controls touch-optimized UI layout for top chrome.
const char kTopChromeTouchUi[] = "top-chrome-touch-ui";
const char kTopChromeTouchUiAuto[] = "auto";
const char kTopChromeTouchUiDisabled[] = "disabled";
const char kTopChromeTouchUiEnabled[] = "enabled";

// Disable partial swap which is needed for some OpenGL drivers / emulators.
const char kUIDisablePartialSwap[] = "ui-disable-partial-swap";

// Enables the ozone x11 clipboard for linux-chromeos.
const char kUseSystemClipboard[] = "use-system-clipboard";

}  // namespace switches
