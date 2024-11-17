// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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

MotionEventAndroidNative::~MotionEventAndroidNative() {
  CHECK(base::android::BuildInfo::GetInstance()->sdk_int() >=
        base::android::SDK_VERSION_S);
  // If check to suppress the compiler warning.
  if (__builtin_available(android 31, *)) {
    AInputEvent_release(native_event_);
  }
}

MotionEventAndroidNative::MotionEventAndroidNative(
    const AInputEvent* event,
    float pix_to_dip,
    float ticks_x,
    float ticks_y,
    float tick_multiplier,
    base::TimeTicks oldest_event_time,
    base::TimeTicks latest_event_time,
    int android_action,
    int pointer_count,
    int history_size,
    int action_index,
    int android_action_button,
    int android_gesture_classification,
    int android_button_state,
    int android_meta_state,
    int source,
    float raw_offset_x_pixels,
    float raw_offset_y_pixels,
    bool for_touch_handle,
    const Pointer* const pointer0,
    const Pointer* const pointer1,
    int y_offset_pix)
    : MotionEventAndroid(pix_to_dip,
                         ticks_x,
                         ticks_y,
                         tick_multiplier,
                         oldest_event_time,
                         latest_event_time,
                         android_action,
                         pointer_count,
                         history_size,
                         action_index,
                         android_action_button,
                         android_gesture_classification,
                         android_button_state,
                         android_meta_state,
                         source,
                         raw_offset_x_pixels,
                         raw_offset_y_pixels,
                         for_touch_handle,
                         pointer0,
                         pointer1),
      native_event_(event),
      y_offset_pix_(y_offset_pix) {}

// static
std::unique_ptr<MotionEventAndroid> MotionEventAndroidNative::Create(
    const AInputEvent* event,
    float pix_to_dip,
    int y_offset_pix) {
  // We need to call release on AInputEvent_release on destruction which was
  // introduced in API level 31 - Android S.
  CHECK(base::android::BuildInfo::GetInstance()->sdk_int() >=
        base::android::SDK_VERSION_S);
  CHECK(AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION);
  CHECK(event != nullptr);

  const size_t history_size = AMotionEvent_getHistorySize(event);
  // AMotionEvent_getEventTime and AMotionEvent_getHistoricalEventTime returns
  // the time with nanoseconds precision.
  const base::TimeTicks latest_event_time =
      base::TimeTicks::FromJavaNanoTime(AMotionEvent_getEventTime(event));
  // Native side doesn't have MotionEvent.getActionMasked() or
  // MotionEvent.getActionIndex counterparts.
  const int action = AMotionEvent_getAction(event);
  const int masked_action = action & AMOTION_EVENT_ACTION_MASK;
  const int action_index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                           AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
  const base::TimeTicks oldest_event_time =
      (history_size == 0) ? latest_event_time
                          : base::TimeTicks::FromJavaNanoTime(
                                AMotionEvent_getHistoricalEventTime(
                                    event, /* history_index= */ 0));

  const size_t pointer_count = AMotionEvent_getPointerCount(event);

  const std::unique_ptr<ui::MotionEventAndroid::Pointer> pointer0 =
      std::make_unique<ui::MotionEventAndroid::Pointer>(
          AMotionEvent_getPointerId(event, 0), AMotionEvent_getX(event, 0),
          AMotionEvent_getY(event, 0) + y_offset_pix,
          AMotionEvent_getTouchMajor(event, 0),
          AMotionEvent_getTouchMinor(event, 0),
          AMotionEvent_getOrientation(event, 0),
          AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_TILT, 0),
          AMotionEvent_getToolType(event, 0));

  std::unique_ptr<ui::MotionEventAndroid::Pointer> pointer1 = nullptr;
  if (pointer_count > 1) {
    pointer1 = std::make_unique<ui::MotionEventAndroid::Pointer>(
        AMotionEvent_getPointerId(event, 1), AMotionEvent_getX(event, 1),
        AMotionEvent_getY(event, 1) + y_offset_pix,
        AMotionEvent_getTouchMajor(event, 1),
        AMotionEvent_getTouchMinor(event, 1),
        AMotionEvent_getOrientation(event, 1),
        AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_TILT, 1),
        AMotionEvent_getToolType(event, 1));
  }

  // TODO(crbug.com/373345667): Move this and other duplicate calculations to
  // base class.
  const float raw_offset_x_pixels =
      AMotionEvent_getRawX(event, 0) - pointer0->pos_x_pixels;
  const float raw_offset_y_pixels =
      AMotionEvent_getRawY(event, 0) - pointer0->pos_y_pixels;

  int gesture_classification = 0;
  if (__builtin_available(android 33, *)) {
    gesture_classification = AMotionEvent_getClassification(event);
  }

  return base::WrapUnique<MotionEventAndroid>(new MotionEventAndroidNative(
      event, pix_to_dip,
      /* ticks_x= */ 0.f,
      /* ticks_y= */ 0.f,
      /* tick_multiplier= */ 0.f, oldest_event_time, latest_event_time,
      masked_action, pointer_count, history_size, action_index,
      /* android_action_button= */ 0, gesture_classification,
      AMotionEvent_getButtonState(event), AMotionEvent_getMetaState(event),
      AInputEvent_getSource(event), raw_offset_x_pixels, raw_offset_y_pixels,
      /* for_touch_handle= */ false, pointer0.get(), pointer1.get(),
      y_offset_pix));
}

int MotionEventAndroidNative::GetPointerId(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].id;
  }
  return AMotionEvent_getPointerId(native_event_, pointer_index);
}

float MotionEventAndroidNative::GetX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].position.x();
  }
  return ToDips(AMotionEvent_getX(native_event_, pointer_index));
}
float MotionEventAndroidNative::GetY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].position.y();
  }
  return ToDips(AMotionEvent_getY(native_event_, pointer_index) +
                y_offset_pix_);
}

float MotionEventAndroidNative::GetTouchMajor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].touch_major;
  }
  return ToDips(AMotionEvent_getTouchMajor(native_event_, pointer_index));
}

float MotionEventAndroidNative::GetTouchMinor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].touch_minor;
  }
  return ToDips(AMotionEvent_getTouchMajor(native_event_, pointer_index));
}

float MotionEventAndroidNative::GetOrientation(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].orientation;
  }
  return ToValidFloat(
      AMotionEvent_getOrientation(native_event_, pointer_index));
}

float MotionEventAndroidNative::GetPressure(size_t pointer_index) const {
  if (GetAction() == MotionEvent::Action::UP) {
    return 0.f;
  }
  return AMotionEvent_getPressure(native_event_, pointer_index);
}

float MotionEventAndroidNative::GetTiltX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].tilt_x;
  }
  float tilt_x, tilt_y;
  float tilt_rad = ToValidFloat(AMotionEvent_getAxisValue(
      native_event_, AMOTION_EVENT_AXIS_TILT, pointer_index));
  float orientation_rad =
      ToValidFloat(AMotionEvent_getOrientation(native_event_, pointer_index));
  ConvertTiltOrientationToTiltXY(tilt_rad, orientation_rad, &tilt_x, &tilt_y);
  return tilt_x;
}

float MotionEventAndroidNative::GetTiltY(size_t pointer_index) const {
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].tilt_y;
  }
  float tilt_x, tilt_y;
  float tilt_rad = ToValidFloat(AMotionEvent_getAxisValue(
      native_event_, AMOTION_EVENT_AXIS_TILT, pointer_index));
  float orientation_rad =
      ToValidFloat(AMotionEvent_getOrientation(native_event_, pointer_index));
  ConvertTiltOrientationToTiltXY(tilt_rad, orientation_rad, &tilt_x, &tilt_y);
  return tilt_y;
}

base::TimeTicks MotionEventAndroidNative::GetHistoricalEventTime(
    size_t historical_index) const {
  DCHECK_LT(historical_index, GetHistorySize());
  return base::TimeTicks::FromJavaNanoTime(
      AMotionEvent_getHistoricalEventTime(native_event_, historical_index));
}

float MotionEventAndroidNative::GetHistoricalTouchMajor(
    size_t pointer_index,
    size_t historical_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  DCHECK_LT(historical_index, GetHistorySize());
  return ToDips(AMotionEvent_getHistoricalTouchMajor(
      native_event_, pointer_index, historical_index));
}

float MotionEventAndroidNative::GetHistoricalX(size_t pointer_index,
                                               size_t historical_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  DCHECK_LT(historical_index, GetHistorySize());
  return ToDips(AMotionEvent_getHistoricalX(native_event_, pointer_index,
                                            historical_index));
}

float MotionEventAndroidNative::GetHistoricalY(size_t pointer_index,
                                               size_t historical_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  DCHECK_LT(historical_index, GetHistorySize());
  return ToDips(AMotionEvent_getHistoricalY(native_event_, pointer_index,
                                            historical_index) +
                y_offset_pix_);
}

ui::MotionEvent::ToolType MotionEventAndroidNative::GetToolType(
    size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].tool_type;
  }
  return FromAndroidToolType(
      AMotionEvent_getToolType(native_event_, pointer_index));
}

float MotionEventAndroidNative::GetXPix(size_t pointer_index) const {
  return GetX(pointer_index) / pix_to_dip();
}
float MotionEventAndroidNative::GetYPix(size_t pointer_index) const {
  return GetY(pointer_index) / pix_to_dip();
}

}  // namespace ui
