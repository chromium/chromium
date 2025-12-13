// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/motion_event_android.h"

#include <android/input.h>

#include <cmath>

#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/numerics/angle_conversions.h"
#include "event_flags_android.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/android/event_flags_android.h"
#include "ui/events/android/events_android_utils.h"
#include "ui/events/android/motion_event_android_source.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/events/motionevent_jni_headers/MotionEvent_jni.h"

using base::android::ScopedJavaLocalRef;

namespace ui {
namespace {

base::TimeTicks FromAndroidTime(base::TimeTicks time) {
  ValidateEventTimeClock(&time);
  return time;
}

float ToValidFloat(float x) {
  if (std::isnan(x)) {
    return 0.f;
  }

  // Wildly large orientation values have been observed in the wild after device
  // rotation. There's not much we can do in that case other than simply
  // sanitize results beyond an absurd and arbitrary threshold.
  if (std::abs(x) > 1e5f) {
    return 0.f;
  }

  return x;
}

// Convert tilt and orientation to tilt_x and tilt_y. Tilt_x and tilt_y will lie
// in [-90, 90].
void ConvertTiltOrientationToTiltXY(float tilt_rad,
                                    float orientation_rad,
                                    float* tilt_x,
                                    float* tilt_y) {
  float r = sinf(tilt_rad);
  float z = cosf(tilt_rad);
  *tilt_x = base::RadToDeg(atan2f(sinf(-orientation_rad) * r, z));
  *tilt_y = base::RadToDeg(atan2f(cosf(-orientation_rad) * r, z));
}

#define ACTION_REVERSE_CASE(x)        \
  case MotionEventAndroid::Action::x: \
    return JNI_MotionEvent::ACTION_##x

#define TOOL_TYPE_REVERSE_CASE(x)       \
  case MotionEventAndroid::ToolType::x: \
    return JNI_MotionEvent::TOOL_TYPE_##x

int ToAndroidAction(MotionEventAndroid::Action action) {
  switch (action) {
    ACTION_REVERSE_CASE(DOWN);
    ACTION_REVERSE_CASE(UP);
    ACTION_REVERSE_CASE(MOVE);
    ACTION_REVERSE_CASE(CANCEL);
    ACTION_REVERSE_CASE(POINTER_DOWN);
    ACTION_REVERSE_CASE(POINTER_UP);
    ACTION_REVERSE_CASE(HOVER_ENTER);
    ACTION_REVERSE_CASE(HOVER_EXIT);
    ACTION_REVERSE_CASE(HOVER_MOVE);
    ACTION_REVERSE_CASE(BUTTON_PRESS);
    ACTION_REVERSE_CASE(BUTTON_RELEASE);
    default:
      NOTREACHED() << "Invalid MotionEvent action: " << action;
  }
}

int ToAndroidToolType(MotionEventAndroid::ToolType tool_type) {
  switch (tool_type) {
    TOOL_TYPE_REVERSE_CASE(UNKNOWN);
    TOOL_TYPE_REVERSE_CASE(FINGER);
    TOOL_TYPE_REVERSE_CASE(STYLUS);
    TOOL_TYPE_REVERSE_CASE(MOUSE);
    TOOL_TYPE_REVERSE_CASE(ERASER);
    default:
      NOTREACHED() << "Invalid MotionEvent tool type: " << tool_type;
  }
}

#undef ACTION_REVERSE_CASE
#undef TOOL_TYPE_REVERSE_CASE

int FromAndroidButtonState(int button_state) {
  int result = 0;
  if ((button_state & JNI_MotionEvent::BUTTON_BACK) != 0)
    result |= MotionEventAndroid::BUTTON_BACK;
  if ((button_state & JNI_MotionEvent::BUTTON_FORWARD) != 0)
    result |= MotionEventAndroid::BUTTON_FORWARD;
  if ((button_state & JNI_MotionEvent::BUTTON_PRIMARY) != 0)
    result |= MotionEventAndroid::BUTTON_PRIMARY;
  if ((button_state & JNI_MotionEvent::BUTTON_SECONDARY) != 0)
    result |= MotionEventAndroid::BUTTON_SECONDARY;
  if ((button_state & JNI_MotionEvent::BUTTON_TERTIARY) != 0)
    result |= MotionEventAndroid::BUTTON_TERTIARY;
  if ((button_state & JNI_MotionEvent::BUTTON_STYLUS_PRIMARY) != 0)
    result |= MotionEventAndroid::BUTTON_STYLUS_PRIMARY;
  if ((button_state & JNI_MotionEvent::BUTTON_STYLUS_SECONDARY) != 0)
    result |= MotionEventAndroid::BUTTON_STYLUS_SECONDARY;
  return result;
}

int ToEventFlags(int meta_state, int button_state) {
  return EventFlagsFromAndroidMetaState(meta_state) |
         EventFlagsFromAndroidButtonState(button_state);
}

size_t ToValidHistorySize(jint history_size, ui::MotionEvent::Action action) {
  DCHECK_GE(history_size, 0);
  // While the spec states that only Action::MOVE events should contain
  // historical entries, it's possible that an embedder could repurpose an
  // Action::MOVE event into a different kind of event. In that case, the
  // historical values are meaningless, and should not be exposed.
  if (action != ui::MotionEvent::Action::MOVE)
    return 0;
  return history_size;
}

}  // namespace

MotionEventAndroid::Pointer::Pointer(jint id,
                                     jfloat pos_x_pixels,
                                     jfloat pos_y_pixels,
                                     jfloat touch_major_pixels,
                                     jfloat touch_minor_pixels,
                                     jfloat pressure,
                                     jfloat orientation_rad,
                                     jfloat tilt_rad,
                                     jint tool_type)
    : id(id),
      pos_x_pixels(pos_x_pixels),
      pos_y_pixels(pos_y_pixels),
      touch_major_pixels(touch_major_pixels),
      touch_minor_pixels(touch_minor_pixels),
      pressure(pressure),
      orientation_rad(orientation_rad),
      tilt_rad(tilt_rad),
      tool_type(tool_type) {}

MotionEventAndroid::CachedPointer::CachedPointer() = default;

MotionEventAndroid::MotionEventAndroid()
    : unique_event_id_(ui::GetNextTouchEventId()) {}

MotionEventAndroid::MotionEventAndroid(
    float pix_to_dip,
    float ticks_x,
    float ticks_y,
    float tick_multiplier,
    base::TimeTicks oldest_event_time,
    base::TimeTicks latest_event_time,
    base::TimeTicks down_time_ms,
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
    : pix_to_dip_(pix_to_dip),
      ticks_x_(ticks_x),
      ticks_y_(ticks_y),
      tick_multiplier_(tick_multiplier),
      for_touch_handle_(for_touch_handle),
      cached_oldest_event_time_(FromAndroidTime(oldest_event_time)),
      cached_latest_event_time_(FromAndroidTime(latest_event_time)),
      cached_down_time_ms_(down_time_ms),
      cached_action_(FromAndroidAction(android_action)),
      cached_pointer_count_(pointer_count),
      cached_history_size_(ToValidHistorySize(history_size, cached_action_)),
      cached_action_index_(action_index),
      cached_action_button_(android_action_button),
      cached_gesture_classification_(android_gesture_classification),
      cached_button_state_(FromAndroidButtonState(android_button_state)),
      cached_flags_(ToEventFlags(android_meta_state, android_button_state)),
      cached_raw_position_offset_(ToDips(raw_offset_x_pixels),
                                  ToDips(raw_offset_y_pixels)),
      unique_event_id_(ui::GetNextTouchEventId()),
      source_(std::move(source)) {
  DCHECK_GT(cached_pointer_count_, 0U);
  DCHECK(cached_pointer_count_ == 1 || pointer1);

  cached_pointers_.push_back(FromAndroidPointer(*pointer0));
  if (cached_pointer_count_ > 1) {
    cached_pointers_.push_back(FromAndroidPointer(*pointer1));
  }
}

MotionEventAndroid::MotionEventAndroid(const MotionEventAndroid& e,
                                       const gfx::PointF& point)
    : pix_to_dip_(e.pix_to_dip_),
      ticks_x_(e.ticks_x_),
      ticks_y_(e.ticks_y_),
      tick_multiplier_(e.tick_multiplier_),
      for_touch_handle_(e.for_touch_handle_),
      cached_oldest_event_time_(e.cached_oldest_event_time_),
      cached_latest_event_time_(e.cached_latest_event_time_),
      cached_down_time_ms_(e.cached_down_time_ms_),
      cached_action_(e.cached_action_),
      cached_pointer_count_(e.cached_pointer_count_),
      cached_history_size_(e.cached_history_size_),
      cached_action_index_(e.cached_action_index_),
      cached_action_button_(e.cached_action_button_),
      cached_gesture_classification_(e.cached_gesture_classification_),
      cached_button_state_(e.cached_button_state_),
      cached_flags_(e.cached_flags_),
      cached_raw_position_offset_(e.cached_raw_position_offset_),
      unique_event_id_(ui::GetNextTouchEventId()),
      source_(e.source_->Clone()) {
  cached_pointers_.push_back(CreateCachedPointer(e.cached_pointers_[0], point));
  if (cached_pointer_count_ > 1) {
    gfx::Vector2dF diff = e.cached_pointers_[1].pointer_data.position -
                          e.cached_pointers_[0].pointer_data.position;
    cached_pointers_.push_back(
        CreateCachedPointer(e.cached_pointers_[1], point + diff));
  }
}

//  static
int MotionEventAndroid::GetAndroidAction(Action action) {
  return ToAndroidAction(action);
}

int MotionEventAndroid::GetAndroidToolType(ToolType tool_type) {
  return ToAndroidToolType(tool_type);
}

std::unique_ptr<MotionEventAndroid> MotionEventAndroid::CreateFor(
    const gfx::PointF& point) const {
  DUMP_WILL_BE_NOTREACHED();
  return nullptr;
}

MotionEventAndroid::~MotionEventAndroid() = default;

uint32_t MotionEventAndroid::GetUniqueEventId() const {
  return unique_event_id_;
}

MotionEventAndroid::Action MotionEventAndroid::GetAction() const {
  return cached_action_;
}

int MotionEventAndroid::GetActionButton() const {
  return cached_action_button_;
}

MotionEvent::Classification MotionEventAndroid::GetClassification() const {
  return static_cast<MotionEvent::Classification>(
      cached_gesture_classification_);
}

float MotionEventAndroid::GetTickMultiplier() const {
  return ToDips(tick_multiplier_);
}

float MotionEventAndroid::GetRawXPix(size_t pointer_index) const {
  return GetRawX(pointer_index) / pix_to_dip();
}

float MotionEventAndroid::GetXPix(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < cached_pointers_.size()) {
    return GetCachedPointerPosition(pointer_index).x() / pix_to_dip();
  }
  return source()->GetXPix(pointer_index);
}

float MotionEventAndroid::GetYPix(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < cached_pointers_.size()) {
    return GetCachedPointerPosition(pointer_index).y() / pix_to_dip();
  }
  return source()->GetYPix(pointer_index);
}

ScopedJavaLocalRef<jobject> MotionEventAndroid::GetJavaObject() const {
  DUMP_WILL_BE_NOTREACHED();
  return nullptr;
}

int MotionEventAndroid::GetActionIndex() const {
  DCHECK(cached_action_ == MotionEvent::Action::POINTER_UP ||
         cached_action_ == MotionEvent::Action::POINTER_DOWN)
      << "Invalid action for GetActionIndex(): " << cached_action_;
  DCHECK_GE(cached_action_index_, 0);
  DCHECK_LT(cached_action_index_, static_cast<int>(cached_pointer_count_));
  return cached_action_index_;
}

size_t MotionEventAndroid::GetPointerCount() const {
  return cached_pointer_count_;
}

int MotionEventAndroid::GetPointerId(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < cached_pointers_.size()) {
    return GetCachedPointerId(pointer_index);
  }
  return source()->GetPointerId(pointer_index);
}

float MotionEventAndroid::GetX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < cached_pointers_.size()) {
    return GetCachedPointerPosition(pointer_index).x();
  }
  return ToDips(source()->GetXPix(pointer_index));
}

float MotionEventAndroid::GetY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < cached_pointers_.size()) {
    return GetCachedPointerPosition(pointer_index).y();
  }
  return ToDips(source()->GetYPix(pointer_index));
}

float MotionEventAndroid::GetRawX(size_t pointer_index) const {
  return GetX(pointer_index) + cached_raw_position_offset_.x();
}

float MotionEventAndroid::GetRawY(size_t pointer_index) const {
  return GetY(pointer_index) + cached_raw_position_offset_.y();
}

float MotionEventAndroid::GetTouchMajor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < cached_pointers_.size()) {
    return GetCachedPointerTouchMajor(pointer_index);
  }
  return ToDips(source()->GetTouchMajorPix(pointer_index));
}

float MotionEventAndroid::GetTouchMinor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < cached_pointers_.size()) {
    return GetCachedPointerTouchMinor(pointer_index);
  }
  return ToDips(source()->GetTouchMinorPix(pointer_index));
}

bool MotionEventAndroid::HasNativeTouchMajor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  return true;
}

float MotionEventAndroid::GetOrientation(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < cached_pointers_.size()) {
    return GetCachedPointerOrientation(pointer_index);
  }
  return ToValidFloat(source()->GetRawOrientation(pointer_index));
}

float MotionEventAndroid::GetTwist(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  return 0.f;
}

float MotionEventAndroid::GetTiltX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < cached_pointers_.size()) {
    return GetCachedPointerTiltX(pointer_index);
  }
  float tilt_x, tilt_y;
  float tilt_rad = ToValidFloat(source()->GetRawTilt(pointer_index));
  float orientation_rad =
      ToValidFloat(source()->GetRawOrientation(pointer_index));
  ConvertTiltOrientationToTiltXY(tilt_rad, orientation_rad, &tilt_x, &tilt_y);
  return tilt_x;
}

float MotionEventAndroid::GetTiltY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < cached_pointers_.size()) {
    return GetCachedPointerTiltY(pointer_index);
  }
  float tilt_x, tilt_y;
  float tilt_rad = ToValidFloat(source()->GetRawTilt(pointer_index));
  float orientation_rad =
      ToValidFloat(source()->GetRawOrientation(pointer_index));
  ConvertTiltOrientationToTiltXY(tilt_rad, orientation_rad, &tilt_x, &tilt_y);
  return tilt_y;
}

base::TimeTicks MotionEventAndroid::GetRawDownTime() const {
  CHECK(!cached_down_time_ms_.is_null());
  return cached_down_time_ms_;
}

float MotionEventAndroid::GetPressure(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < cached_pointers_.size()) {
    return GetCachedPointerPressure(pointer_index);
  }
  return source()->GetPressure(pointer_index);
}

float MotionEventAndroid::GetTangentialPressure(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  return 0.f;
}

base::TimeTicks MotionEventAndroid::GetEventTime() const {
  return cached_oldest_event_time_;
}

base::TimeTicks MotionEventAndroid::GetLatestEventTime() const {
  return cached_latest_event_time_;
}

size_t MotionEventAndroid::GetHistorySize() const {
  return cached_history_size_;
}

base::TimeTicks MotionEventAndroid::GetHistoricalEventTime(
    size_t historical_index) const {
  return FromAndroidTime(source()->GetHistoricalEventTime(historical_index));
}

float MotionEventAndroid::GetHistoricalTouchMajor(
    size_t pointer_index,
    size_t historical_index) const {
  return ToDips(
      source()->GetHistoricalTouchMajorPix(pointer_index, historical_index));
}

float MotionEventAndroid::GetHistoricalX(size_t pointer_index,
                                         size_t historical_index) const {
  return ToDips(source()->GetHistoricalXPix(pointer_index, historical_index));
}

float MotionEventAndroid::GetHistoricalY(size_t pointer_index,
                                         size_t historical_index) const {
  return ToDips(source()->GetHistoricalYPix(pointer_index, historical_index));
}

int MotionEventAndroid::GetSourceDeviceId(size_t pointer_index) const {
  // Source device id is not supported.
  return -1;
}

ui::MotionEvent::ToolType MotionEventAndroid::GetToolType(
    size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  if (pointer_index < cached_pointers_.size()) {
    return GetCachedPointerToolType(pointer_index);
  }
  return source()->GetToolType(pointer_index);
}

int MotionEventAndroid::GetButtonState() const {
  return cached_button_state_;
}

int MotionEventAndroid::GetFlags() const {
  return cached_flags_;
}

int MotionEventAndroid::GetSource() const {
  return source()->GetSource();
}

float MotionEventAndroid::ToDips(float pixels) const {
  return pixels * pix_to_dip_;
}

int MotionEventAndroid::GetCachedPointerId(size_t pointer_index) const {
  return cached_pointers_[pointer_index].id;
}

const gfx::PointF& MotionEventAndroid::GetCachedPointerPosition(
    size_t pointer_index) const {
  return cached_pointers_[pointer_index].pointer_data.position;
}

float MotionEventAndroid::GetCachedPointerTouchMajor(
    size_t pointer_index) const {
  return cached_pointers_[pointer_index].pointer_data.touch_major;
}

float MotionEventAndroid::GetCachedPointerTouchMinor(
    size_t pointer_index) const {
  return cached_pointers_[pointer_index].touch_minor;
}

float MotionEventAndroid::GetCachedPointerPressure(size_t pointer_index) const {
  return cached_pointers_[pointer_index].pressure;
}

float MotionEventAndroid::GetCachedPointerOrientation(
    size_t pointer_index) const {
  return cached_pointers_[pointer_index].orientation;
}

float MotionEventAndroid::GetCachedPointerTiltX(size_t pointer_index) const {
  return cached_pointers_[pointer_index].tilt_x;
}

float MotionEventAndroid::GetCachedPointerTiltY(size_t pointer_index) const {
  return cached_pointers_[pointer_index].tilt_y;
}

MotionEvent::ToolType MotionEventAndroid::GetCachedPointerToolType(
    size_t pointer_index) const {
  return cached_pointers_[pointer_index].tool_type;
}

MotionEventAndroid::CachedPointer MotionEventAndroid::FromAndroidPointer(
    const Pointer& pointer) const {
  CachedPointer result;
  result.id = pointer.id;
  result.pointer_data.position =
      gfx::PointF(ToDips(pointer.pos_x_pixels), ToDips(pointer.pos_y_pixels));
  result.pointer_data.touch_major = ToDips(pointer.touch_major_pixels);
  result.touch_minor = ToDips(pointer.touch_minor_pixels);
  if (cached_action_ != Action::UP) {
    result.pressure = pointer.pressure;
  }
  result.orientation = ToValidFloat(pointer.orientation_rad);
  float tilt_rad = ToValidFloat(pointer.tilt_rad);
  ConvertTiltOrientationToTiltXY(tilt_rad, result.orientation, &result.tilt_x,
                                 &result.tilt_y);
  result.tool_type = FromAndroidToolType(pointer.tool_type);
  return result;
}

MotionEventAndroid::CachedPointer MotionEventAndroid::CreateCachedPointer(
    const CachedPointer& pointer,
    const gfx::PointF& point) const {
  CachedPointer result = pointer;
  result.pointer_data.position = point;
  return result;
}

}  // namespace ui
