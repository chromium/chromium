// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_LOGGING_H_
#define UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_LOGGING_H_

// libgestures.so binds to this function for logging.
// TODO(spang): Fix libgestures to not require this.
// clang-format off
extern "C"
    __attribute__((visibility("default")))
    __attribute__((used, retain))
  void gestures_log(int verb, const char* fmt, ...);
// clang-format on

#endif  // UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_LOGGING_H_
