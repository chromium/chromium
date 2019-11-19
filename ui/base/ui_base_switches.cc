// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_switches.h"

namespace switches {

#if defined(OS_MACOSX) && !defined(OS_IOS)
// Disable use of AVFoundation to draw video content.
const char kDisableAVFoundationOverlays[] = "disable-avfoundation-overlays";

// Fall back to using CAOpenGLLayers display content, instead of the IOSurface
// based overlay display path.
const char kDisableMacOverlays[] = "disable-mac-overlays";

// Disable animations for showing and hiding modal dialogs.
const char kDisableModalAnimations[] = "disable-modal-animations";

// Disable use of cross-process CALayers to display content directly from the
// GPU process on Mac.
const char kDisableRemoteCoreAnimation[] = "disable-remote-core-animation";

// Show borders around CALayers corresponding to overlays and partial damage.
const char kShowMacOverlayBorders[] = "show-mac-overlay-borders";
#endif

// Scale factor to apply to every animation duration. Must be >= 0.0. This will
// only apply to LinearAnimation and its subclasses.
const char kAnimationDurationScale[] = "animation-duration-scale";

// Disables layer-edge anti-aliasing in the compositor.
const char kDisableCompositedAntialiasing[] = "disable-composited-antialiasing";

// Disables use of DWM composition for top level windows.
const char kDisableDwmComposition[] = "disable-dwm-composition";

// Disables touch adjustment.
const char kDisableTouchAdjustment[] = "disable-touch-adjustment";

// Disables touch event based drag and drop.
const char kDisableTouchDragDrop[] = "disable-touch-drag-drop";

// Disable re-use of non-exact resources to fulfill ResourcePool requests.
// Intended only for use in layout or pixel tests to reduce noise.
const char kDisallowNonExactResourceReuse[] =
    "disallow-non-exact-resource-reuse";

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

// Tint GL-composited color.
const char kTintGlCompositedContent[] = "tint-gl-composited-content";

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
