// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_switches.h"
#include "build/build_config.h"

namespace switches {

// TODO(rjkroege): Some of these have an "ash" prefix. When ChromeOS startup
// scripts have been updated, the leading "ash" prefix should be removed.

// Enables software based mirroring.
const char kEnableSoftwareMirroring[] = "ash-enable-software-mirroring";

// Crash the browser at startup if the display's color profile does not match
// the forced color profile. This is necessary on Mac because Chrome's pixel
// output is always subject to the color conversion performed by the operating
// system. On all other platforms, this is a no-op.
const char kEnsureForcedColorProfile[] = "ensure-forced-color-profile";

// Force all monitors to be treated as though they have the specified color
// profile. Accepted values are "srgb" and "generic-rgb" (currently used by Mac
// layout tests) and "color-spin-gamma24" (used by layout tests).
const char kForceDisplayColorProfile[] = "force-color-profile";

// Force rastering to take place in the specified color profile. Accepted values
// are the same as for the kForceDisplayColorProfile case above.
const char kForceRasterColorProfile[] = "force-raster-color-profile";

// Overrides the device scale factor for the browser UI and the contents.
const char kForceDeviceScaleFactor[] = "force-device-scale-factor";

// Sets a window size, optional position, and optional scale factor.
// "1024x768" creates a window of size 1024x768.
// "100+200-1024x768" positions the window at 100,200.
// "1024x768*2" sets the scale factor to 2 for a high DPI display.
// "800,0+800-800x800" for two displays at 800x800 resolution.
// "800,0+800-800x800,0+1600-800x800" for three displays at 800x800 resolution.
const char kHostWindowBounds[] = "ash-host-window-bounds";

// Specifies the layout mode and offsets for the secondary display for
// testing. The format is "<t|r|b|l>,<offset>" where t=TOP, r=RIGHT,
// b=BOTTOM and L=LEFT. For example, 'r,-100' means the secondary display
// is positioned on the right with -100 offset. (above than primary)
const char kSecondaryDisplayLayout[] = "secondary-display-layout";

// Specifies the initial screen configuration, or state of all displays, for
// FakeDisplayDelegate, see class for format details.
const char kScreenConfig[] = "screen-config";

// Uses the 1st display in --ash-host-window-bounds as internal display.
// This is for debugging on linux desktop.
const char kUseFirstDisplayAsInternal[] = "use-first-display-as-internal";

#if defined(OS_CHROMEOS)
// Enables unified desktop mode.
const char kEnableUnifiedDesktop[] = "ash-enable-unified-desktop";

// Enables using HDR transfer function if the monitor says it supports it.
const char kEnableUseHDRTransferFunction[] = "enable-use-hdr-transfer-function";
#endif

}  // namespace switches
