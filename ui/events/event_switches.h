// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENTS_SWITCHES_H_
#define UI_EVENTS_EVENTS_SWITCHES_H_

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "ui/events/events_base_export.h"

namespace switches {

EVENTS_BASE_EXPORT extern const char kCompensateForUnstablePinchZoom[];

#if defined(OS_LINUX)
EVENTS_BASE_EXPORT extern const char kTouchDevices[];
EVENTS_BASE_EXPORT extern const char kPenDevices[];
#endif

#if defined(USE_X11) || defined(USE_OZONE)
EVENTS_BASE_EXPORT extern const char kExtraTouchNoiseFiltering[];
EVENTS_BASE_EXPORT extern const char kTouchCalibration[];
EVENTS_BASE_EXPORT extern const char kEdgeTouchFiltering[];
EVENTS_BASE_EXPORT extern const char kLowPressureTouchFiltering[];
EVENTS_BASE_EXPORT extern const char kDisableCancelAllTouches[];
#endif

}  // namespace switches

#endif  // UI_EVENTS_EVENTS_SWITCHES_H_
