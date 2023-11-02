// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_TIMER_PROVIDER_H_
#define UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_TIMER_PROVIDER_H_

#include <gestures/gestures.h>

namespace ui {

stime_t StimeNow();

extern const GesturesTimerProvider kGestureTimerProvider;

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_TIMER_PROVIDER_H_
