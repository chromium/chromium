// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/motion_event_android_factory.h"

#include "base/memory/ptr_util.h"
#include "ui/events/android/motion_event_android_java.h"

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
  return base::WrapUnique(new MotionEventAndroidJava(
      env, event, pix_to_dip, ticks_x, ticks_y, tick_multiplier,
      oldest_event_time, android_action, pointer_count, history_size,
      action_index, android_action_button, android_gesture_classification,
      android_button_state, raw_offset_x_pixels, raw_offset_y_pixels,
      for_touch_handle, pointer0, pointer1));
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
  return base::WrapUnique(new MotionEventAndroidJava(
      env, event, pix_to_dip, ticks_x, ticks_y, tick_multiplier,
      oldest_event_time, latest_event_time, down_time_ms, android_action,
      pointer_count, history_size, action_index, android_action_button,
      android_gesture_classification, android_button_state, raw_offset_x_pixels,
      raw_offset_y_pixels, for_touch_handle, pointer0, pointer1,
      is_latest_event_time_resampled));
}

}  // namespace ui
