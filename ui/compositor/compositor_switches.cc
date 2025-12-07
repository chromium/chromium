// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_switches.h"

#include "base/command_line.h"
#include "build/build_config.h"

namespace switches {

// Forces tests to produce pixel output when they normally wouldn't.
const char kEnablePixelOutputInTests[] = "enable-pixel-output-in-tests";

const char kUIEnableRGBA4444Textures[] = "ui-enable-rgba-4444-textures";

const char kUIEnableZeroCopy[] = "ui-enable-zero-copy";
const char kUIDisableZeroCopy[] = "ui-disable-zero-copy";

const char kUIShowPaintRects[] = "ui-show-paint-rects";

const char kUISlowAnimations[] = "ui-slow-animations";

const char kDisableVsyncForTests[] = "disable-vsync-for-tests";

}  // namespace switches

namespace ui {

bool IsUIZeroCopyEnabled() {
  // Match the behavior of IsZeroCopyUploadEnabled() in content/browser/gpu.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(IS_APPLE)
  return !command_line.HasSwitch(switches::kUIDisableZeroCopy);
#else
  return command_line.HasSwitch(switches::kUIEnableZeroCopy);
#endif
}

}  // namespace ui
