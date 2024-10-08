// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/android/motion_event_android.h"

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

using base::android::ScopedJavaLocalRef;

namespace ui {
namespace {

#define ACTION_CASE(x)              \
  case JNI_MotionEvent::ACTION_##x: \
    return MotionEventAndroid::Action::x

#define ACTION_REVERSE_CASE(x)        \
  case MotionEventAndroid::Action::x: \
    return JNI_MotionEvent::ACTION_##x

#define TOOL_TYPE_REVERSE_CASE(x)       \
  case MotionEventAndroid::ToolType::x: \
    return JNI_MotionEvent::TOOL_TYPE_##x

MotionEventAndroid::Action FromAndroidAction(int android_action) {
  switch (android_action) {
    ACTION_CASE(DOWN);
    ACTION_CASE(UP);
    ACTION_CASE(MOVE);
    ACTION_CASE(CANCEL);
    ACTION_CASE(POINTER_DOWN);
    ACTION_CASE(POINTER_UP);
    ACTION_CASE(HOVER_ENTER);
    ACTION_CASE(HOVER_EXIT);
    ACTION_CASE(HOVER_MOVE);
    ACTION_CASE(BUTTON_PRESS);
    ACTION_CASE(BUTTON_RELEASE);
    default:
      NOTREACHED_IN_MIGRATION()
          << "Invalid Android MotionEvent action: " << android_action;
  }
  return MotionEventAndroid::Action::CANCEL;
}

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
      NOTREACHED_IN_MIGRATION() << "Invalid MotionEvent action: " << action;
  }
  return JNI_MotionEvent::ACTION_CANCEL;
}

int ToAndroidToolType(MotionEventAndroid::ToolType tool_type) {
  switch (tool_type) {
    TOOL_TYPE_REVERSE_CASE(UNKNOWN);
    TOOL_TYPE_REVERSE_CASE(FINGER);
    TOOL_TYPE_REVERSE_CASE(STYLUS);
    TOOL_TYPE_REVERSE_CASE(MOUSE);
    TOOL_TYPE_REVERSE_CASE(ERASER);
    default:
      NOTREACHED_IN_MIGRATION()
          << "Invalid MotionEvent tool type: " << tool_type;
  }
  return JNI_MotionEvent::TOOL_TYPE_UNKNOWN;
}

#undef ACTION_CASE
#undef ACTION_REVERSE_CASE
#undef TOOL_TYPE_CASE
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
  int flags = ui::EF_NONE;

  if ((meta_state & AMETA_SHIFT_ON) != 0)
    flags |= ui::EF_SHIFT_DOWN;
  if ((meta_state & AMETA_CTRL_ON) != 0)
    flags |= ui::EF_CONTROL_DOWN;
  if ((meta_state & AMETA_ALT_ON) != 0)
    flags |= ui::EF_ALT_DOWN;
  if ((meta_state & AMETA_META_ON) != 0)
    flags |= ui::EF_COMMAND_DOWN;
  if ((meta_state & AMETA_CAPS_LOCK_ON) != 0)
    flags |= ui::EF_CAPS_LOCK_ON;

  if ((button_state & JNI_MotionEvent::BUTTON_BACK) != 0)
    flags |= ui::EF_BACK_MOUSE_BUTTON;
  if ((button_state & JNI_MotionEvent::BUTTON_FORWARD) != 0)
    flags |= ui::EF_FORWARD_MOUSE_BUTTON;
  if ((button_state & JNI_MotionEvent::BUTTON_PRIMARY) != 0)
    flags |= ui::EF_LEFT_MOUSE_BUTTON;
  if ((button_state & JNI_MotionEvent::BUTTON_SECONDARY) != 0)
    flags |= ui::EF_RIGHT_MOUSE_BUTTON;
  if ((button_state & JNI_MotionEvent::BUTTON_TERTIARY) != 0)
    flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
  if ((button_state & JNI_MotionEvent::BUTTON_STYLUS_PRIMARY) != 0)
    flags |= ui::EF_LEFT_MOUSE_BUTTON;
  if ((button_state & JNI_MotionEvent::BUTTON_STYLUS_SECONDARY) != 0)
    flags |= ui::EF_RIGHT_MOUSE_BUTTON;

  return flags;
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
                                     jfloat orientation_rad,
                                     jfloat tilt_rad,
                                     jint tool_type)
    : id(id),
      pos_x_pixels(pos_x_pixels),
      pos_y_pixels(pos_y_pixels),
      touch_major_pixels(touch_major_pixels),
      touch_minor_pixels(touch_minor_pixels),
      orientation_rad(orientation_rad),
      tilt_rad(tilt_rad),
      tool_type(tool_type) {
}

MotionEventAndroid::CachedPointer::CachedPointer() = default;

MotionEventAndroid::MotionEventAndroid(float pix_to_dip,
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
                                       const Pointer* const pointer1)
    : pix_to_dip_(pix_to_dip),
      ticks_x_(ticks_x),
      ticks_y_(ticks_y),
      tick_multiplier_(tick_multiplier),
      source_(source),
      for_touch_handle_(for_touch_handle),
      cached_oldest_event_time_(FromAndroidTime(oldest_event_time)),
      cached_latest_event_time_(FromAndroidTime(latest_event_time)),
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
      unique_event_id_(ui::GetNextTouchEventId()) {
  DCHECK_GT(cached_pointer_count_, 0U);
  DCHECK(cached_pointer_count_ == 1 || pointer1);

  cached_pointers_[0] = FromAndroidPointer(*pointer0);
  if (cached_pointer_count_ > 1)
    cached_pointers_[1] = FromAndroidPointer(*pointer1);
}

MotionEventAndroid::MotionEventAndroid(const MotionEventAndroid& e,
                                       const gfx::PointF& point)
    : pix_to_dip_(e.pix_to_dip_),
      ticks_x_(e.ticks_x_),
      ticks_y_(e.ticks_y_),
      tick_multiplier_(e.tick_multiplier_),
      source_(e.source_),
      for_touch_handle_(e.for_touch_handle_),
      cached_oldest_event_time_(e.cached_oldest_event_time_),
      cached_latest_event_time_(e.cached_latest_event_time_),
      cached_action_(e.cached_action_),
      cached_pointer_count_(e.cached_pointer_count_),
      cached_history_size_(e.cached_history_size_),
      cached_action_index_(e.cached_action_index_),
      cached_action_button_(e.cached_action_button_),
      cached_gesture_classification_(e.cached_gesture_classification_),
      cached_button_state_(e.cached_button_state_),
      cached_flags_(e.cached_flags_),
      cached_raw_position_offset_(e.cached_raw_position_offset_),
      unique_event_id_(ui::GetNextTouchEventId()) {
  if (cached_pointer_count_ > 1) {
    gfx::Vector2dF diff =
        e.cached_pointers_[1].position - e.cached_pointers_[0].position;
    cached_pointers_[1] =
        CreateCachedPointer(e.cached_pointers_[1], point + diff);
  }
  cached_pointers_[0] = CreateCachedPointer(e.cached_pointers_[0], point);
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

int MotionEventAndroid::GetSource() const {
  return source_;
}

MotionEvent::Classification MotionEventAndroid::GetClassification() const {
  return static_cast<MotionEvent::Classification>(
      cached_gesture_classification_);
}

float MotionEventAndroid::GetTickMultiplier() const {
  return ToDips(tick_multiplier_);
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

float MotionEventAndroid::GetRawX(size_t pointer_index) const {
  return GetX(pointer_index) + cached_raw_position_offset_.x();
}

float MotionEventAndroid::GetRawY(size_t pointer_index) const {
  return GetY(pointer_index) + cached_raw_position_offset_.y();
}

float MotionEventAndroid::GetTwist(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  return 0.f;
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

int MotionEventAndroid::GetSourceDeviceId(size_t pointer_index) const {
  // Source device id is not supported.
  return -1;
}

int MotionEventAndroid::GetButtonState() const {
  return cached_button_state_;
}

int MotionEventAndroid::GetFlags() const {
  return cached_flags_;
}

float MotionEventAndroid::ToDips(float pixels) const {
  return pixels * pix_to_dip_;
}

#define TOOL_TYPE_CASE(x)              \
  case JNI_MotionEvent::TOOL_TYPE_##x: \
    return MotionEventAndroid::ToolType::x

MotionEventAndroid::ToolType MotionEventAndroid::FromAndroidToolType(
    int android_tool_type) {
  switch (android_tool_type) {
    TOOL_TYPE_CASE(UNKNOWN);
    TOOL_TYPE_CASE(FINGER);
    TOOL_TYPE_CASE(STYLUS);
    TOOL_TYPE_CASE(MOUSE);
    TOOL_TYPE_CASE(ERASER);
    default:
      NOTREACHED_IN_MIGRATION()
          << "Invalid Android MotionEvent tool type: " << android_tool_type;
  }
  return MotionEventAndroid::ToolType::UNKNOWN;
}

#undef TOOL_TYPE_CASE

base::TimeTicks MotionEventAndroid::FromAndroidTime(base::TimeTicks time) {
  ValidateEventTimeClock(&time);
  return time;
}

float MotionEventAndroid::ToValidFloat(float x) {
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
void MotionEventAndroid::ConvertTiltOrientationToTiltXY(float tilt_rad,
                                                        float orientation_rad,
                                                        float* tilt_x,
                                                        float* tilt_y) {
  float r = sinf(tilt_rad);
  float z = cosf(tilt_rad);
  *tilt_x = base::RadToDeg(atan2f(sinf(-orientation_rad) * r, z));
  *tilt_y = base::RadToDeg(atan2f(cosf(-orientation_rad) * r, z));
}

MotionEventAndroid::CachedPointer MotionEventAndroid::FromAndroidPointer(
    const Pointer& pointer) const {
  CachedPointer result;
  result.id = pointer.id;
  result.position =
      gfx::PointF(ToDips(pointer.pos_x_pixels), ToDips(pointer.pos_y_pixels));
  result.touch_major = ToDips(pointer.touch_major_pixels);
  result.touch_minor = ToDips(pointer.touch_minor_pixels);
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
  CachedPointer result;
  result.id = pointer.id;
  result.position = point;
  result.touch_major = pointer.touch_major;
  result.touch_minor = pointer.touch_minor;
  result.orientation = pointer.orientation;
  result.tilt_x = pointer.tilt_x;
  result.tilt_y = pointer.tilt_y;
  result.tool_type = pointer.tool_type;
  return result;
}

}  // namespace ui
