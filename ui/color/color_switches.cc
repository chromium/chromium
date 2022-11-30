// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_switches.h"

#include "build/build_config.h"

namespace switches {

#if BUILDFLAG(IS_WIN)
// Use the system accent color as the Chrome UI accent color.
const char kPervasiveSystemAccentColor[] = "pervasive-system-accent-color";
#endif

}  // namespace switches
