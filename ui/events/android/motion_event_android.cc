// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/motion_event_android.h"

#include <android/input.h>

#include <cmath>

#include "base/android/jni_android.h"
#include "base/numerics/math_constants.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/motionevent_jni_headers/MotionEvent_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace ui {
namespace {

#define ACTION_CASE(x)              \
  case JNI_MotionEvent::ACTION_##x: \
    return MotionEventAndroid::Action::x

#define ACTION_REVERSE_CASE(x)        \
  case MotionEventAndroid::Action::x: \
    return JNI_MotionEvent::ACTION_##x

#define TOOL_TYPE_CASE(x)              \
  case JNI_MotionEvent::TOOL_TYPE_##x: \
    return MotionEventAndroid::ToolType::x

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
      NOTREACHED() << "Invalid Android MotionEvent action: " << android_action;
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
      NOTREACHED() << "Invalid MotionEvent action: " << action;
  }
  return JNI_MotionEvent::ACTION_CANCEL;
}

MotionEventAndroid::ToolType FromAndroidToolType(int android_tool_type) {
  switch (android_tool_type) {
    TOOL_TYPE_CASE(UNKNOWN);
    TOOL_TYPE_CASE(FINGER);
    TOOL_TYPE_CASE(STYLUS);
    TOOL_TYPE_CASE(MOUSE);
    TOOL_TYPE_CASE(ERASER);
    default:
      NOTREACHED() << "Invalid Android MotionEvent tool type: "
                   << android_tool_type;
  }
  return MotionEventAndroid::ToolType::UNKNOWN;
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

base::TimeTicks FromAndroidTime(int64_t time_ms) {
  base::TimeTicks timestamp =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(time_ms);
  ValidateEventTimeClock(&timestamp);
  return timestamp;
}

float ToValidFloat(float x) {
  if (std::isnan(x))
    return 0.f;

  // Wildly large orientation values have been observed in the wild after device
  // rotation. There's not much we can do in that case other than simply
  // sanitize results beyond an absurd and arbitrary threshold.
  if (std::abs(x) > 1e5f)
    return 0.f;

  return x;
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

// Convert tilt and orientation to tilt_x and tilt_y. Tilt_x and tilt_y will lie
// in [-90, 90].
void ConvertTiltOrientationToTiltXY(float tilt_rad,
                                    float orientation_rad,
                                    float* tilt_x,
                                    float* tilt_y) {
  float r = sinf(tilt_rad);
  float z = cosf(tilt_rad);
  *tilt_x = atan2f(sinf(-orientation_rad) * r, z) * 180.f / base::kPiFloat;
  *tilt_y = atan2f(cosf(-orientation_rad) * r, z) * 180.f / base::kPiFloat;
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

MotionEventAndroid::CachedPointer::CachedPointer()
    : id(0),
      touch_major(0),
      touch_minor(0),
      orientation(0),
      tilt_x(0),
      tilt_y(0),
      tool_type(ToolType::UNKNOWN) {}

MotionEventAndroid::MotionEventAndroid(JNIEnv* env,
                                       jobject event,
                                       jfloat pix_to_dip,
                                       jfloat ticks_x,
                                       jfloat ticks_y,
                                       jfloat tick_multiplier,
                                       jlong time_ms,
                                       jint android_action,
                                       jint pointer_count,
                                       jint history_size,
                                       jint action_index,
                                       jint android_action_button,
                                       jint android_button_state,
                                       jint android_meta_state,
                                       jfloat raw_offset_x_pixels,
                                       jfloat raw_offset_y_pixels,
                                       jboolean for_touch_handle,
                                       const Pointer* const pointer0,
                                       const Pointer* const pointer1)
    : pix_to_dip_(pix_to_dip),
      ticks_x_(ticks_x),
      ticks_y_(ticks_y),
      tick_multiplier_(tick_multiplier),
      time_sec_(time_ms / 1000),
      for_touch_handle_(for_touch_handle),
      cached_time_(FromAndroidTime(time_ms)),
      cached_action_(FromAndroidAction(android_action)),
      cached_pointer_count_(pointer_count),
      cached_history_size_(ToValidHistorySize(history_size, cached_action_)),
      cached_action_index_(action_index),
      cached_action_button_(android_action_button),
      cached_button_state_(FromAndroidButtonState(android_button_state)),
      cached_flags_(ToEventFlags(android_meta_state, android_button_state)),
      cached_raw_position_offset_(ToDips(raw_offset_x_pixels),
                                  ToDips(raw_offset_y_pixels)),
      unique_event_id_(ui::GetNextTouchEventId()) {
  DCHECK_GT(cached_pointer_count_, 0U);
  DCHECK(cached_pointer_count_ == 1 || pointer1);

  event_.Reset(env, event);
  if (cached_pointer_count_ > MAX_POINTERS_TO_CACHE || cached_history_size_ > 0)
    DCHECK(event_.obj());

  cached_pointers_[0] = FromAndroidPointer(*pointer0);
  if (cached_pointer_count_ > 1)
    cached_pointers_[1] = FromAndroidPointer(*pointer1);
}

MotionEventAndroid::MotionEventAndroid(const MotionEventAndroid& e)
    : event_(e.event_),
      pix_to_dip_(e.pix_to_dip_),
      ticks_x_(e.ticks_x_),
      ticks_y_(e.ticks_y_),
      tick_multiplier_(e.tick_multiplier_),
      time_sec_(e.time_sec_),
      for_touch_handle_(e.for_touch_handle_),
      cached_time_(e.cached_time_),
      cached_action_(e.cached_action_),
      cached_pointer_count_(e.cached_pointer_count_),
      cached_history_size_(e.cached_history_size_),
      cached_action_index_(e.cached_action_index_),
      cached_action_button_(e.cached_action_button_),
      cached_button_state_(e.cached_button_state_),
      cached_flags_(e.cached_flags_),
      cached_raw_position_offset_(e.cached_raw_position_offset_),
      unique_event_id_(ui::GetNextTouchEventId()) {
  cached_pointers_[0] = e.cached_pointers_[0];
  if (cached_pointer_count_ > 1)
    cached_pointers_[1] = e.cached_pointers_[1];
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
  std::unique_ptr<MotionEventAndroid> event(new MotionEventAndroid(*this));
  if (cached_pointer_count_ > 1) {
    gfx::Vector2dF diff = event->cached_pointers_[1].position -
                          event->cached_pointers_[0].position;
    event->cached_pointers_[1] =
        CreateCachedPointer(cached_pointers_[1], point + diff);
  }
  event->cached_pointers_[0] = CreateCachedPointer(cached_pointers_[0], point);
  return event;
}

MotionEventAndroid::~MotionEventAndroid() {
}

uint32_t MotionEventAndroid::GetUniqueEventId() const {
  return unique_event_id_;
}

MotionEventAndroid::Action MotionEventAndroid::GetAction() const {
  return cached_action_;
}

int MotionEventAndroid::GetActionButton() const {
  return cached_action_button_;
}

float MotionEventAndroid::GetTickMultiplier() const {
  return ToDips(tick_multiplier_);
}

ScopedJavaLocalRef<jobject> MotionEventAndroid::GetJavaObject() const {
  return ScopedJavaLocalRef<jobject>(event_);
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
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].id;
  return JNI_MotionEvent::Java_MotionEvent_getPointerId(AttachCurrentThread(),
                                                        event_, pointer_index);
}

float MotionEventAndroid::GetX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].position.x();
  return ToDips(JNI_MotionEvent::Java_MotionEvent_getXF_I(
      AttachCurrentThread(), event_, pointer_index));
}

float MotionEventAndroid::GetY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].position.y();
  return ToDips(JNI_MotionEvent::Java_MotionEvent_getYF_I(
      AttachCurrentThread(), event_, pointer_index));
}

float MotionEventAndroid::GetXPix(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].position.x() / pix_to_dip_;
  return JNI_MotionEvent::Java_MotionEvent_getXF_I(AttachCurrentThread(),
                                                   event_, pointer_index);
}

float MotionEventAndroid::GetYPix(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].position.y() / pix_to_dip_;
  return JNI_MotionEvent::Java_MotionEvent_getYF_I(AttachCurrentThread(),
                                                   event_, pointer_index);
}

float MotionEventAndroid::GetRawX(size_t pointer_index) const {
  return GetX(pointer_index) + cached_raw_position_offset_.x();
}

float MotionEventAndroid::GetRawY(size_t pointer_index) const {
  return GetY(pointer_index) + cached_raw_position_offset_.y();
}

float MotionEventAndroid::GetTouchMajor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].touch_major;
  return ToDips(JNI_MotionEvent::Java_MotionEvent_getTouchMajorF_I(
      AttachCurrentThread(), event_, pointer_index));
}

float MotionEventAndroid::GetTouchMinor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].touch_minor;
  return ToDips(JNI_MotionEvent::Java_MotionEvent_getTouchMinorF_I(
      AttachCurrentThread(), event_, pointer_index));
}

float MotionEventAndroid::GetOrientation(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].orientation;
  return ToValidFloat(JNI_MotionEvent::Java_MotionEvent_getOrientationF_I(
      AttachCurrentThread(), event_, pointer_index));
}

float MotionEventAndroid::GetPressure(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  // Note that this early return is a special case exercised only in testing, as
  // caching the pressure values is not a worthwhile optimization (they're
  // accessed at most once per event instance).
  if (!event_.obj())
    return 0.f;
  if (cached_action_ == MotionEvent::Action::UP)
    return 0.f;
  return JNI_MotionEvent::Java_MotionEvent_getPressureF_I(
      AttachCurrentThread(), event_, pointer_index);
}

float MotionEventAndroid::GetTiltX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].tilt_x;
  if (!event_.obj())
    return 0.f;
  float tilt_x, tilt_y;
  float tilt_rad = ToValidFloat(Java_MotionEvent_getAxisValueF_I_I(
      AttachCurrentThread(), event_, JNI_MotionEvent::AXIS_TILT,
      pointer_index));
  float orientation_rad =
      ToValidFloat(JNI_MotionEvent::Java_MotionEvent_getOrientationF_I(
          AttachCurrentThread(), event_, pointer_index));
  ConvertTiltOrientationToTiltXY(tilt_rad, orientation_rad, &tilt_x, &tilt_y);
  return tilt_x;
}

float MotionEventAndroid::GetTiltY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].tilt_y;
  if (!event_.obj())
    return 0.f;
  float tilt_x, tilt_y;
  float tilt_rad =
      ToValidFloat(JNI_MotionEvent::Java_MotionEvent_getAxisValueF_I_I(
          AttachCurrentThread(), event_, JNI_MotionEvent::AXIS_TILT,
          pointer_index));
  float orientation_rad =
      ToValidFloat(JNI_MotionEvent::Java_MotionEvent_getOrientationF_I(
          AttachCurrentThread(), event_, pointer_index));
  ConvertTiltOrientationToTiltXY(tilt_rad, orientation_rad, &tilt_x, &tilt_y);
  return tilt_y;
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
  return cached_time_;
}

size_t MotionEventAndroid::GetHistorySize() const {
  return cached_history_size_;
}

base::TimeTicks MotionEventAndroid::GetHistoricalEventTime(
    size_t historical_index) const {
  return FromAndroidTime(
      JNI_MotionEvent::Java_MotionEvent_getHistoricalEventTime(
          AttachCurrentThread(), event_, historical_index));
}

float MotionEventAndroid::GetHistoricalTouchMajor(
    size_t pointer_index,
    size_t historical_index) const {
  return ToDips(JNI_MotionEvent::Java_MotionEvent_getHistoricalTouchMajorF_I_I(
      AttachCurrentThread(), event_, pointer_index, historical_index));
}

float MotionEventAndroid::GetHistoricalX(size_t pointer_index,
                                         size_t historical_index) const {
  return ToDips(JNI_MotionEvent::Java_MotionEvent_getHistoricalXF_I_I(
      AttachCurrentThread(), event_, pointer_index, historical_index));
}

float MotionEventAndroid::GetHistoricalY(size_t pointer_index,
                                         size_t historical_index) const {
  return ToDips(JNI_MotionEvent::Java_MotionEvent_getHistoricalYF_I_I(
      AttachCurrentThread(), event_, pointer_index, historical_index));
}

ui::MotionEvent::ToolType MotionEventAndroid::GetToolType(
    size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].tool_type;
  return FromAndroidToolType(JNI_MotionEvent::Java_MotionEvent_getToolType(
      AttachCurrentThread(), event_, pointer_index));
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
