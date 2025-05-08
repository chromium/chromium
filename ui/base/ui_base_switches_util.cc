// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_switches_util.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace switches {

bool IsElasticOverscrollEnabled() {
// On macOS and iOS this value is adjusted in `UpdateScrollbarTheme()`,
// but the system default is true.
#if BUILDFLAG(IS_APPLE)
  return true;
#elif BUILDFLAG(IS_WIN)
  return base::FeatureList::IsEnabled(features::kElasticOverscroll);
#elif BUILDFLAG(IS_ANDROID)
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
             base::android::SDK_VERSION_S &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableOverscrollEdgeEffect) &&
         base::FeatureList::IsEnabled(features::kElasticOverscroll);
#else
  return false;
#endif
}

}  // namespace switches
