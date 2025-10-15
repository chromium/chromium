// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/switches.h"

#include "base/command_line.h"
#include "build/build_config.h"

namespace switches {

// Scale factor to apply to every animation duration. Must be >= 0.0. This will
// only apply to LinearAnimation and its subclasses.
const char kAnimationDurationScale[] = "animation-duration-scale";

// Force disables font subpixel positioning. This affects the character glyph
// sharpness, kerning, hinting and layout.
const char kDisableFontSubpixelPositioning[] =
    "disable-font-subpixel-positioning";

// Enable native CPU-mappable GPU memory buffer support on Linux.
const char kEnableNativeGpuMemoryBuffers[] = "enable-native-gpu-memory-buffers";

// Forces whether the user desires reduced motion, regardless of system
// settings.
const char kForcePrefersReducedMotion[] = "force-prefers-reduced-motion";

// Forces whether the user desires no reduced motion, regardless of system
// settings.
const char kForcePrefersNoReducedMotion[] = "force-prefers-no-reduced-motion";

// Run in headless mode, i.e., without a UI or display server dependencies.
const char kHeadless[] = "headless";

// Headless screen info in the format: {0,0 800x600}{800,0 600x800}.
// See //components/headless/screen_info/README.md for more details.
const char kScreenInfo[] = "screen-info";

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Which X11 display to connect to. Emulates the GTK+ "--display=" command line
// argument. In use only with Ozone/X11.
const char kX11Display[] = "display";
// Disables MIT-SHM extension. In use only with Ozone/X11.
const char kNoXshm[] = "no-xshm";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace switches

namespace features {
BASE_FEATURE(kUseSmartRefForGPUFenceHandle, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables using rounding instead of flooring for coordinate conversions.
//
// Using `round()` instead of `floor()` prevents the systematic downward bias
// that causes layout drift.
//
// Assume we have value d in DIP and scale factor p > 1.
// Let d' = screen_to_dip(dip_to_screen(d)), then
//     d' == screen_to_dip(round(d*p))
//        == screen_to_dip(d*p + eps), where |eps| <= 0.5. Then,
//     d' == round( (d*p+eps) / p)
//        == round(d + eps/p).
// Since |eps / p < 0.5| and d is an integer, round(d + eps/p) == d,
// therefore d' == d. QED.
BASE_FEATURE(kUseRoundedPointConversion, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHdrAgtm, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTransparentIconWorkaround,
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)
}  // namespace features
