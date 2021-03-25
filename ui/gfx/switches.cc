// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ui/gfx/switches.h"

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

// Run in headless mode, i.e., without a UI or display server dependencies.
const char kHeadless[] = "headless";

}  // namespace switches
