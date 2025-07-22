// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/motion_event_android_java.h"

#include <android/input.h>

#include <cmath>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/angle_conversions.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/android/motion_event_android_source.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/events/motionevent_jni_headers/MotionEvent_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace ui {

MotionEventAndroidJava::MotionEventAndroidJava(
    jfloat pix_to_dip,
    jfloat ticks_x,
    jfloat ticks_y,
    jfloat tick_multiplier,
    base::TimeTicks oldest_event_time,
    base::TimeTicks latest_event_time,
    base::TimeTicks down_time_ms,
    jint android_action,
    jint pointer_count,
    jint history_size,
    jint action_index,
    jint android_action_button,
    jint android_gesture_classification,
    jint android_button_state,
    jint meta_state,
    jfloat raw_offset_x_pixels,
    jfloat raw_offset_y_pixels,
    jboolean for_touch_handle,
    const Pointer* const pointer0,
    const Pointer* const pointer1,
    std::unique_ptr<MotionEventAndroidSource> source)
    : MotionEventAndroid(pix_to_dip,
                         ticks_x,
                         ticks_y,
                         tick_multiplier,
                         oldest_event_time,
                         latest_event_time,
                         down_time_ms,
                         android_action,
                         pointer_count,
                         history_size,
                         action_index,
                         android_action_button,
                         android_gesture_classification,
                         android_button_state,
                         meta_state,
                         raw_offset_x_pixels,
                         raw_offset_y_pixels,
                         for_touch_handle,
                         pointer0,
                         pointer1),
      source_(std::move(source)) {
  DCHECK(source_);
}

MotionEventAndroidJava::MotionEventAndroidJava(
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
    jint meta_state,
    jfloat raw_offset_x_pixels,
    jfloat raw_offset_y_pixels,
    jboolean for_touch_handle,
    const Pointer* const pointer0,
    const Pointer* const pointer1,
    std::unique_ptr<MotionEventAndroidSource> source)
    : MotionEventAndroidJava(pix_to_dip,
                             ticks_x,
                             ticks_y,
                             tick_multiplier,
                             oldest_event_time,
                             oldest_event_time,
                             base::TimeTicks(),
                             android_action,
                             pointer_count,
                             history_size,
                             action_index,
                             android_action_button,
                             android_gesture_classification,
                             android_button_state,
                             meta_state,
                             raw_offset_x_pixels,
                             raw_offset_y_pixels,
                             for_touch_handle,
                             pointer0,
                             pointer1,
                             std::move(source)) {
  DCHECK_EQ(history_size, 0);
}

MotionEventAndroidJava::MotionEventAndroidJava(const MotionEventAndroidJava& e,
                                               const gfx::PointF& point)
    : MotionEventAndroid(e, point), source_(e.source_->Clone()) {}

std::unique_ptr<MotionEventAndroid> MotionEventAndroidJava::CreateFor(
    const gfx::PointF& point) const {
  return base::WrapUnique(new MotionEventAndroidJava(*this, point));
}

MotionEventAndroidJava::~MotionEventAndroidJava() = default;

ScopedJavaLocalRef<jobject> MotionEventAndroidJava::GetJavaObject() const {
  return source_->GetJavaObject();
}

int MotionEventAndroidJava::GetPointerId(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerId(pointer_index);
  }
  return source_->GetPointerId(pointer_index);
}

float MotionEventAndroidJava::GetX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerPosition(pointer_index).x();
  }
  return ToDips(source_->GetXPix(pointer_index));
}

float MotionEventAndroidJava::GetY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerPosition(pointer_index).y();
  }
  return ToDips(source_->GetYPix(pointer_index));
}

float MotionEventAndroidJava::GetXPix(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerPosition(pointer_index).x() / pix_to_dip();
  }
  return source_->GetXPix(pointer_index);
}

float MotionEventAndroidJava::GetYPix(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerPosition(pointer_index).y() / pix_to_dip();
  }
  return source_->GetYPix(pointer_index);
}

int MotionEventAndroidJava::GetSource() const {
  return source_->GetSource();
}

float MotionEventAndroidJava::GetTouchMajor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerTouchMajor(pointer_index);
  }
  return ToDips(source_->GetTouchMajorPix(pointer_index));
}

float MotionEventAndroidJava::GetTouchMinor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerTouchMinor(pointer_index);
  }
  return ToDips(source_->GetTouchMinorPix(pointer_index));
}

float MotionEventAndroidJava::GetOrientation(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerOrientation(pointer_index);
  }
  return ToValidFloat(source_->GetRawOrientation(pointer_index));
}

float MotionEventAndroidJava::GetPressure(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerPressure(pointer_index);
  }
  return source_->GetPressure(pointer_index);
}

float MotionEventAndroidJava::GetTiltX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerTiltX(pointer_index);
  }
  float tilt_x, tilt_y;
  float tilt_rad = ToValidFloat(source_->GetRawTilt(pointer_index));
  float orientation_rad =
      ToValidFloat(source_->GetRawOrientation(pointer_index));
  ConvertTiltOrientationToTiltXY(tilt_rad, orientation_rad, &tilt_x, &tilt_y);
  return tilt_x;
}

float MotionEventAndroidJava::GetTiltY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerTiltY(pointer_index);
  }
  float tilt_x, tilt_y;
  float tilt_rad = ToValidFloat(source_->GetRawTilt(pointer_index));
  float orientation_rad =
      ToValidFloat(source_->GetRawOrientation(pointer_index));
  ConvertTiltOrientationToTiltXY(tilt_rad, orientation_rad, &tilt_x, &tilt_y);
  return tilt_y;
}

base::TimeTicks MotionEventAndroidJava::GetHistoricalEventTime(
    size_t historical_index) const {
  return FromAndroidTime(source_->GetHistoricalEventTime(historical_index));
}

float MotionEventAndroidJava::GetHistoricalTouchMajor(
    size_t pointer_index,
    size_t historical_index) const {
  return ToDips(
      source_->GetHistoricalTouchMajorPix(pointer_index, historical_index));
}

float MotionEventAndroidJava::GetHistoricalX(size_t pointer_index,
                                             size_t historical_index) const {
  return ToDips(source_->GetHistoricalXPix(pointer_index, historical_index));
}

float MotionEventAndroidJava::GetHistoricalY(size_t pointer_index,
                                             size_t historical_index) const {
  return ToDips(source_->GetHistoricalYPix(pointer_index, historical_index));
}

ui::MotionEvent::ToolType MotionEventAndroidJava::GetToolType(
    size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (IsPointerCacheable(pointer_index)) {
    return GetCachedPointerToolType(pointer_index);
  }
  return source_->GetToolType(pointer_index);
}

bool MotionEventAndroidJava::IsLatestEventTimeResampled() const {
  return source_->IsLatestEventTimeResampled();
}

}  // namespace ui
