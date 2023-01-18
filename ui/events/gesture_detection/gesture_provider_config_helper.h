// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_GESTURE_PROVIDER_CONFIG_HELPER_H_
#define UI_EVENTS_GESTURE_DETECTION_GESTURE_PROVIDER_CONFIG_HELPER_H_

#include "base/task/sequenced_task_runner.h"
#include "ui/events/gesture_detection/gesture_detection_export.h"
#include "ui/events/gesture_detection/gesture_detector.h"
#include "ui/events/gesture_detection/gesture_provider.h"
#include "ui/events/gesture_detection/scale_gesture_detector.h"

namespace ui {

enum class GestureProviderConfigType {
  CURRENT_PLATFORM,  // Parameters tailored for the current platform.
  GENERIC_DESKTOP,   // Parameters typical for a desktop machine.
  GENERIC_MOBILE     // Parameters typical for a mobile device (phone/tablet).
};

GESTURE_DETECTION_EXPORT GestureProvider::Config GetGestureProviderConfig(
    GestureProviderConfigType,
    scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr);

}  // namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_GESTURE_PROVIDER_CONFIG_HELPER_H_
