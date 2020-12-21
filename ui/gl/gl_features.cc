// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/chromeos_buildflags.h"
#include "ui/gl/gl_switches.h"

namespace features {
namespace {

const base::Feature kGpuVsync{"GpuVsync", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace

// Use the passthrough command decoder by default.  This can be overridden with
// the --use-cmd-decoder=passthrough or --use-cmd-decoder=validating flags.
// Feature lives in ui/gl because it affects the GL binding initialization on
// platforms that would otherwise not default to using EGL bindings.
// Launched on Windows, still experimental on other platforms.
const base::Feature kDefaultPassthroughCommandDecoder{
  "DefaultPassthroughCommandDecoder",
#if defined(OS_WIN) ||                                       \
    ((defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && \
     !defined(CHROMECAST_BUILD))
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

bool UseGpuVsync() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableGpuVsync) &&
         base::FeatureList::IsEnabled(kGpuVsync);
}

}  // namespace features
