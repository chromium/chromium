// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/motion_event_android_factory.h"

#include <android/input.h>

#include "base/memory/ptr_util.h"
#include "ui/events/android/motion_event_android_java.h"
#include "ui/events/android/motion_event_android_source_java.h"
#include "ui/events/android/motion_event_android_source_native.h"

namespace ui {

// static
std::unique_ptr<MotionEventAndroid> MotionEventAndroidFactory::CreateFromJava(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& event,
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
    jfloat raw_offset_x_pixels,
    jfloat raw_offset_y_pixels,
    jboolean for_touch_handle,
    const MotionEventAndroid::Pointer* const pointer0,
    const MotionEventAndroid::Pointer* const pointer1) {
  return CreateFromJava(
      env, event, pix_to_dip, ticks_x, ticks_y, tick_multiplier,
      oldest_event_time, /*latest_event_time=*/oldest_event_time,
      /*down_time_ms=*/base::TimeTicks(), android_action, pointer_count,
      history_size, action_index, android_action_button,
      android_gesture_classification, android_button_state, raw_offset_x_pixels,
      raw_offset_y_pixels, for_touch_handle, pointer0, pointer1,
      /*is_latest_event_time_resampled=*/false);
}

// static
std::unique_ptr<MotionEventAndroid> MotionEventAndroidFactory::CreateFromJava(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& event,
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
    jfloat raw_offset_x_pixels,
    jfloat raw_offset_y_pixels,
    jboolean for_touch_handle,
    const MotionEventAndroid::Pointer* const pointer0,
    const MotionEventAndroid::Pointer* const pointer1,
    bool is_latest_event_time_resampled) {
  auto source = MotionEventAndroidSourceJava::Create(
      event, is_latest_event_time_resampled);
  int meta_state = source->GetMetaState();
  return base::WrapUnique<MotionEventAndroid>(new MotionEventAndroidJava(
      pix_to_dip, ticks_x, ticks_y, tick_multiplier, oldest_event_time,
      latest_event_time, down_time_ms, android_action, pointer_count,
      history_size, action_index, android_action_button,
      android_gesture_classification, android_button_state, meta_state,
      raw_offset_x_pixels, raw_offset_y_pixels, for_touch_handle, pointer0,
      pointer1, std::move(source)));
}

// static
std::unique_ptr<MotionEventAndroid> MotionEventAndroidFactory::CreateFromNative(
    base::android::ScopedInputEvent input_event,
    float pix_to_dip,
    float y_offset_pix,
    std::optional<MotionEventAndroid::EventTimes> event_times) {
  const AInputEvent* event = input_event.a_input_event();

  CHECK(event != nullptr);
  CHECK(AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION);

  const size_t history_size = AMotionEvent_getHistorySize(event);
  // AMotionEvent_getEventTime and AMotionEvent_getHistoricalEventTime returns
  // the time with nanoseconds precision.
  if (!event_times) {
    event_times = MotionEventAndroid::EventTimes();
    event_times->latest =
        base::TimeTicks::FromJavaNanoTime(AMotionEvent_getEventTime(event));
    event_times->oldest =
        (history_size == 0)
            ? event_times->latest
            : base::TimeTicks::FromJavaNanoTime(
                  AMotionEvent_getHistoricalEventTime(event,
                                                      /*history_index=*/0));
  }
  const jlong down_time_ms =
      base::TimeTicks::FromJavaNanoTime(AMotionEvent_getDownTime(event))
          .ToUptimeMillis();
  // Native side doesn't have MotionEvent.getActionMasked() or
  // MotionEvent.getActionIndex counterparts.
  const int action = AMotionEvent_getAction(event);
  const int masked_action = action & AMOTION_EVENT_ACTION_MASK;
  const int action_index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                           AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

  const size_t pointer_count = AMotionEvent_getPointerCount(event);

  const std::unique_ptr<ui::MotionEventAndroid::Pointer> pointer0 =
      std::make_unique<ui::MotionEventAndroid::Pointer>(
          /*id=*/AMotionEvent_getPointerId(event, 0),
          /*pos_x_pixels=*/AMotionEvent_getX(event, 0),
          /*pos_y_pixels=*/AMotionEvent_getY(event, 0) + y_offset_pix,
          /*touch_major_pixels=*/AMotionEvent_getTouchMajor(event, 0),
          /*touch_minor_pixels=*/AMotionEvent_getTouchMinor(event, 0),
          /*pressure=*/AMotionEvent_getPressure(event, 0),
          /*orientation_rad=*/AMotionEvent_getOrientation(event, 0),
          /*tilt_rad=*/
          AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_TILT, 0),
          /*tool_type=*/AMotionEvent_getToolType(event, 0));

  std::unique_ptr<ui::MotionEventAndroid::Pointer> pointer1 = nullptr;
  if (pointer_count > 1) {
    pointer1 = std::make_unique<ui::MotionEventAndroid::Pointer>(
        /*id=*/AMotionEvent_getPointerId(event, 1),
        /*pos_x_pixels=*/AMotionEvent_getX(event, 1),
        /*pos_y_pixels=*/AMotionEvent_getY(event, 1) + y_offset_pix,
        /*touch_major_pixels=*/AMotionEvent_getTouchMajor(event, 1),
        /*touch_minor_pixels=*/AMotionEvent_getTouchMinor(event, 1),
        /*pressure=*/AMotionEvent_getPressure(event, 1),
        /*orientation_rad=*/AMotionEvent_getOrientation(event, 1),
        /*tilt_rad=*/
        AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_TILT, 1),
        /*tool_type=*/AMotionEvent_getToolType(event, 1));
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

  auto source = std::make_unique<MotionEventAndroidSourceNative>(
      std::move(input_event), y_offset_pix);

  return base::WrapUnique<MotionEventAndroid>(new MotionEventAndroid(
      pix_to_dip,
      /*ticks_x=*/0.f,
      /*ticks_y=*/0.f,
      /*tick_multiplier=*/0.f, event_times->oldest, event_times->latest,
      base::TimeTicks::FromUptimeMillis(down_time_ms), masked_action,
      pointer_count, history_size, action_index,
      /*android_action_button=*/0, gesture_classification,
      AMotionEvent_getButtonState(event), AMotionEvent_getMetaState(event),
      raw_offset_x_pixels, raw_offset_y_pixels,
      /*for_touch_handle=*/false, pointer0.get(), pointer1.get(),
      std::move(source)));
}

}  // namespace ui
