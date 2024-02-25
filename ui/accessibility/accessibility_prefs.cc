// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define all the prefs used by ui/accessibility.
#include "ui/accessibility/accessibility_prefs.h"
#include "build/build_config.h"

namespace prefs {
// Local state prefs.

#if BUILDFLAG(IS_ANDROID)
// Whether different accessibility filtering modes for performance are allowed.
// Exposed only to mobile Android.
const char kAccessibilityPerformanceFilteringAllowed[] =
    "settings.a11y.allow_accessibility_performance_filtering";

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace prefs
