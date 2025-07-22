// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/motion_event_android_native.h"

#include <android/input.h>

#include <cmath>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/angle_conversions.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/android/motion_event_android_source.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"

namespace ui {

MotionEventAndroidNative::~MotionEventAndroidNative() = default;

MotionEventAndroidNative::MotionEventAndroidNative(
    float pix_to_dip,
    float ticks_x,
    float ticks_y,
    float tick_multiplier,
    base::TimeTicks oldest_event_time,
    base::TimeTicks latest_event_time,
    base::TimeTicks cached_down_time_ms,
    int android_action,
    int pointer_count,
    int history_size,
    int action_index,
    int android_action_button,
    int android_gesture_classification,
    int android_button_state,
    int android_meta_state,
    float raw_offset_x_pixels,
    float raw_offset_y_pixels,
    bool for_touch_handle,
    const Pointer* const pointer0,
    const Pointer* const pointer1,
    std::unique_ptr<MotionEventAndroidSource> source)
    : MotionEventAndroid(pix_to_dip,
                         ticks_x,
                         ticks_y,
                         tick_multiplier,
                         oldest_event_time,
                         latest_event_time,
                         cached_down_time_ms,
                         android_action,
                         pointer_count,
                         history_size,
                         action_index,
                         android_action_button,
                         android_gesture_classification,
                         android_button_state,
                         android_meta_state,
                         raw_offset_x_pixels,
                         raw_offset_y_pixels,
                         for_touch_handle,
                         pointer0,
                         pointer1,
                         std::move(source)) {}

float MotionEventAndroidNative::GetPressure(size_t pointer_index) const {
  if (GetAction() == MotionEvent::Action::UP) {
    return 0.f;
  }
  return source()->GetPressure(pointer_index);
}

float MotionEventAndroidNative::GetXPix(size_t pointer_index) const {
  return GetX(pointer_index) / pix_to_dip();
}
float MotionEventAndroidNative::GetYPix(size_t pointer_index) const {
  return GetY(pointer_index) / pix_to_dip();
}

}  // namespace ui
