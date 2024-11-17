// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/android/motion_event_android_java.h"

#include <android/input.h>

#include <cmath>

#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/numerics/angle_conversions.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/events/motionevent_jni_headers/MotionEvent_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace ui {

MotionEventAndroidJava::MotionEventAndroidJava(
    JNIEnv* env,
    jobject event,
    jfloat pix_to_dip,
    jfloat ticks_x,
    jfloat ticks_y,
    jfloat tick_multiplier,
    base::TimeTicks oldest_event_time,
    base::TimeTicks latest_event_time,
    jint android_action,
    jint pointer_count,
    jint history_size,
    jint action_index,
    jint android_action_button,
    jint android_gesture_classification,
    jint android_button_state,
    jint android_meta_state,
    jint source,
    jfloat raw_offset_x_pixels,
    jfloat raw_offset_y_pixels,
    jboolean for_touch_handle,
    const Pointer* const pointer0,
    const Pointer* const pointer1)
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
                         pointer1) {
  event_.Reset(env, event);
  if (GetPointerCount() > MAX_POINTERS_TO_CACHE || GetHistorySize() > 0) {
    DCHECK(event_.obj());
  }
}

MotionEventAndroidJava::MotionEventAndroidJava(
    JNIEnv* env,
    jobject event,
    jfloat pix_to_dip,
    jfloat ticks_x,
    jfloat ticks_y,
    jfloat tick_multiplier,
    base::TimeTicks oldest_event_time,
    jint android_action,
    jint pointer_count,
    jint history_size,
    jint action_index,
    jint android_action_button,
    jint android_gesture_classification,
    jint android_button_state,
    jint android_meta_state,
    jint source,
    jfloat raw_offset_x_pixels,
    jfloat raw_offset_y_pixels,
    jboolean for_touch_handle,
    const Pointer* const pointer0,
    const Pointer* const pointer1)
    : MotionEventAndroidJava(env,
                             event,
                             pix_to_dip,
                             ticks_x,
                             ticks_y,
                             tick_multiplier,
                             oldest_event_time,
                             oldest_event_time,
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
                             pointer1) {
  DCHECK_EQ(history_size, 0);
}

MotionEventAndroidJava::MotionEventAndroidJava(const MotionEventAndroidJava& e,
                                               const gfx::PointF& point)
    : MotionEventAndroid(e, point), event_(e.event_) {}

std::unique_ptr<MotionEventAndroid> MotionEventAndroidJava::CreateFor(
    const gfx::PointF& point) const {
  std::unique_ptr<MotionEventAndroid> event(
      new MotionEventAndroidJava(*this, point));
  return event;
}

MotionEventAndroidJava::~MotionEventAndroidJava() = default;

ScopedJavaLocalRef<jobject> MotionEventAndroidJava::GetJavaObject() const {
  return ScopedJavaLocalRef<jobject>(event_);
}

int MotionEventAndroidJava::GetPointerId(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].id;
  }
  return JNI_MotionEvent::Java_MotionEvent_getPointerId(AttachCurrentThread(),
                                                        event_, pointer_index);
}

float MotionEventAndroidJava::GetX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].position.x();
  }
  return ToDips(JNI_MotionEvent::Java_MotionEvent_getX(AttachCurrentThread(),
                                                       event_, pointer_index));
}

float MotionEventAndroidJava::GetY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].position.y();
  }
  return ToDips(JNI_MotionEvent::Java_MotionEvent_getY(AttachCurrentThread(),
                                                       event_, pointer_index));
}

float MotionEventAndroidJava::GetXPix(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].position.x() / pix_to_dip();
  }
  return JNI_MotionEvent::Java_MotionEvent_getX(AttachCurrentThread(), event_,
                                                pointer_index);
}

float MotionEventAndroidJava::GetYPix(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].position.y() / pix_to_dip();
  }
  return JNI_MotionEvent::Java_MotionEvent_getY(AttachCurrentThread(), event_,
                                                pointer_index);
}

float MotionEventAndroidJava::GetTouchMajor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].touch_major;
  }
  return ToDips(JNI_MotionEvent::Java_MotionEvent_getTouchMajor(
      AttachCurrentThread(), event_, pointer_index));
}

float MotionEventAndroidJava::GetTouchMinor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].touch_minor;
  }
  return ToDips(JNI_MotionEvent::Java_MotionEvent_getTouchMinor(
      AttachCurrentThread(), event_, pointer_index));
}

float MotionEventAndroidJava::GetOrientation(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].orientation;
  }
  return MotionEventAndroid::ToValidFloat(
      JNI_MotionEvent::Java_MotionEvent_getOrientation(AttachCurrentThread(),
                                                       event_, pointer_index));
}

float MotionEventAndroidJava::GetPressure(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  // Note that this early return is a special case exercised only in testing, as
  // caching the pressure values is not a worthwhile optimization (they're
  // accessed at most once per event instance).
  if (!event_.obj()) {
    return 0.f;
  }
  if (GetAction() == MotionEvent::Action::UP) {
    return 0.f;
  }
  return JNI_MotionEvent::Java_MotionEvent_getPressure(AttachCurrentThread(),
                                                       event_, pointer_index);
}

float MotionEventAndroidJava::GetTiltX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].tilt_x;
  }
  if (!event_.obj()) {
    return 0.f;
  }
  float tilt_x, tilt_y;
  float tilt_rad = MotionEventAndroid::ToValidFloat(
      Java_MotionEvent_getAxisValue(AttachCurrentThread(), event_,
                                    JNI_MotionEvent::AXIS_TILT, pointer_index));
  float orientation_rad = MotionEventAndroid::ToValidFloat(
      JNI_MotionEvent::Java_MotionEvent_getOrientation(AttachCurrentThread(),
                                                       event_, pointer_index));
  ConvertTiltOrientationToTiltXY(tilt_rad, orientation_rad, &tilt_x, &tilt_y);
  return tilt_x;
}

float MotionEventAndroidJava::GetTiltY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].tilt_y;
  }
  if (!event_.obj()) {
    return 0.f;
  }
  float tilt_x, tilt_y;
  float tilt_rad = MotionEventAndroid::ToValidFloat(
      JNI_MotionEvent::Java_MotionEvent_getAxisValue(
          AttachCurrentThread(), event_, JNI_MotionEvent::AXIS_TILT,
          pointer_index));
  float orientation_rad = MotionEventAndroid::ToValidFloat(
      JNI_MotionEvent::Java_MotionEvent_getOrientation(AttachCurrentThread(),
                                                       event_, pointer_index));
  ConvertTiltOrientationToTiltXY(tilt_rad, orientation_rad, &tilt_x, &tilt_y);
  return tilt_y;
}

base::TimeTicks MotionEventAndroidJava::GetHistoricalEventTime(
    size_t historical_index) const {
  jlong time_ms = JNI_MotionEvent::Java_MotionEvent_getHistoricalEventTime(
      AttachCurrentThread(), event_, historical_index);
  return MotionEventAndroid::FromAndroidTime(
      base::TimeTicks::FromUptimeMillis(time_ms));
}

float MotionEventAndroidJava::GetHistoricalTouchMajor(
    size_t pointer_index,
    size_t historical_index) const {
  return ToDips(JNI_MotionEvent::Java_MotionEvent_getHistoricalTouchMajor(
      AttachCurrentThread(), event_, pointer_index, historical_index));
}

float MotionEventAndroidJava::GetHistoricalX(size_t pointer_index,
                                             size_t historical_index) const {
  return ToDips(JNI_MotionEvent::Java_MotionEvent_getHistoricalX(
      AttachCurrentThread(), event_, pointer_index, historical_index));
}

float MotionEventAndroidJava::GetHistoricalY(size_t pointer_index,
                                             size_t historical_index) const {
  return ToDips(JNI_MotionEvent::Java_MotionEvent_getHistoricalY(
      AttachCurrentThread(), event_, pointer_index, historical_index));
}

ui::MotionEvent::ToolType MotionEventAndroidJava::GetToolType(
    size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < MAX_POINTERS_TO_CACHE) {
    return cached_pointers_[pointer_index].tool_type;
  }
  return MotionEventAndroid::FromAndroidToolType(
      JNI_MotionEvent::Java_MotionEvent_getToolType(AttachCurrentThread(),
                                                    event_, pointer_index));
}

}  // namespace ui
