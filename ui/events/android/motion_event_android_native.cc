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
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"

namespace ui {

MotionEventAndroidNative::~MotionEventAndroidNative() = default;

MotionEventAndroidNative::MotionEventAndroidNative(
    base::android::ScopedInputEvent input_event,
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
    float y_offset_pix)
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
                         pointer1),
      native_event_(std::move(input_event)),
      y_offset_pix_(y_offset_pix) {
  CHECK(native_event_);
}

int MotionEventAndroidNative::GetPointerId(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerId(pointer_index);
  }
  return AMotionEvent_getPointerId(native_event_.a_input_event(),
                                   pointer_index);
}

float MotionEventAndroidNative::GetX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerPosition(pointer_index).x();
  }
  return ToDips(
      AMotionEvent_getX(native_event_.a_input_event(), pointer_index));
}
float MotionEventAndroidNative::GetY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerPosition(pointer_index).y();
  }
  return ToDips(
      AMotionEvent_getY(native_event_.a_input_event(), pointer_index) +
      y_offset_pix_);
}

float MotionEventAndroidNative::GetTouchMajor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerTouchMajor(pointer_index);
  }
  return ToDips(
      AMotionEvent_getTouchMajor(native_event_.a_input_event(), pointer_index));
}

float MotionEventAndroidNative::GetTouchMinor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerTouchMinor(pointer_index);
  }
  return ToDips(
      AMotionEvent_getTouchMajor(native_event_.a_input_event(), pointer_index));
}

float MotionEventAndroidNative::GetOrientation(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerOrientation(pointer_index);
  }
  return ToValidFloat(AMotionEvent_getOrientation(native_event_.a_input_event(),
                                                  pointer_index));
}

float MotionEventAndroidNative::GetPressure(size_t pointer_index) const {
  if (GetAction() == MotionEvent::Action::UP) {
    return 0.f;
  }
  return AMotionEvent_getPressure(native_event_.a_input_event(), pointer_index);
}

float MotionEventAndroidNative::GetTiltX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerTiltX(pointer_index);
  }
  float tilt_x, tilt_y;
  float tilt_rad = ToValidFloat(AMotionEvent_getAxisValue(
      native_event_.a_input_event(), AMOTION_EVENT_AXIS_TILT, pointer_index));
  float orientation_rad = ToValidFloat(AMotionEvent_getOrientation(
      native_event_.a_input_event(), pointer_index));
  ConvertTiltOrientationToTiltXY(tilt_rad, orientation_rad, &tilt_x, &tilt_y);
  return tilt_x;
}

float MotionEventAndroidNative::GetTiltY(size_t pointer_index) const {
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerTiltY(pointer_index);
  }
  float tilt_x, tilt_y;
  float tilt_rad = ToValidFloat(AMotionEvent_getAxisValue(
      native_event_.a_input_event(), AMOTION_EVENT_AXIS_TILT, pointer_index));
  float orientation_rad = ToValidFloat(AMotionEvent_getOrientation(
      native_event_.a_input_event(), pointer_index));
  ConvertTiltOrientationToTiltXY(tilt_rad, orientation_rad, &tilt_x, &tilt_y);
  return tilt_y;
}

base::TimeTicks MotionEventAndroidNative::GetHistoricalEventTime(
    size_t historical_index) const {
  DCHECK_LT(historical_index, GetHistorySize());
  return base::TimeTicks::FromJavaNanoTime(AMotionEvent_getHistoricalEventTime(
      native_event_.a_input_event(), historical_index));
}

float MotionEventAndroidNative::GetHistoricalTouchMajor(
    size_t pointer_index,
    size_t historical_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  DCHECK_LT(historical_index, GetHistorySize());
  return ToDips(AMotionEvent_getHistoricalTouchMajor(
      native_event_.a_input_event(), pointer_index, historical_index));
}

float MotionEventAndroidNative::GetHistoricalX(size_t pointer_index,
                                               size_t historical_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  DCHECK_LT(historical_index, GetHistorySize());
  return ToDips(AMotionEvent_getHistoricalX(native_event_.a_input_event(),
                                            pointer_index, historical_index));
}

float MotionEventAndroidNative::GetHistoricalY(size_t pointer_index,
                                               size_t historical_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  DCHECK_LT(historical_index, GetHistorySize());
  return ToDips(AMotionEvent_getHistoricalY(native_event_.a_input_event(),
                                            pointer_index, historical_index) +
                y_offset_pix_);
}

ui::MotionEvent::ToolType MotionEventAndroidNative::GetToolType(
    size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerToolType(pointer_index);
  }
  return FromAndroidToolType(
      AMotionEvent_getToolType(native_event_.a_input_event(), pointer_index));
}

float MotionEventAndroidNative::GetXPix(size_t pointer_index) const {
  return GetX(pointer_index) / pix_to_dip();
}
float MotionEventAndroidNative::GetYPix(size_t pointer_index) const {
  return GetY(pointer_index) / pix_to_dip();
}

int MotionEventAndroidNative::GetSource() const {
  return AInputEvent_getSource(native_event_.a_input_event());
}

}  // namespace ui
