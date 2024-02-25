// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define all the prefs used by ui/accessibility.
#ifndef UI_ACCESSIBILITY_ACCESSIBILITY_PREFS_H_
#define UI_ACCESSIBILITY_ACCESSIBILITY_PREFS_H_

#include "ui/accessibility/ax_base_export.h"
#include "build/build_config.h"

namespace prefs {
// Local state prefs.

#if BUILDFLAG(IS_ANDROID)
// Whether different accessibility filtering modes for performance are allowed.
// Exposed only to mobile Android.
AX_BASE_EXPORT extern const char kAccessibilityPerformanceFilteringAllowed[];

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace prefs

#endif  // UI_ACCESSIBILITY_ACCESSIBILITY_PREFS_H_
