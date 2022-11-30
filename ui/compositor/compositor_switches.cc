// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_switches.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

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

namespace features {

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

}  // namespace features

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

bool IsPixelCanvasRecordingEnabled() {
  return base::FeatureList::IsEnabled(features::kEnablePixelCanvasRecording);
}

}  // namespace ui
