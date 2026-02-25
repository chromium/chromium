// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_FACTORY_H_
#define UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_FACTORY_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_input_event.h"
#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/events/events_export.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

// Factory class for creating instances of MotionEventAndroid.
class EVENTS_EXPORT MotionEventAndroidFactory {
 public:
  // Creates a MotionEventAndroid from a Java MotionEvent object.
  static std::unique_ptr<MotionEventAndroid> CreateFromJava(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& event,
      float pix_to_dip,
      float ticks_x,
      float ticks_y,
      float tick_multiplier,
      base::TimeTicks oldest_event_time,
      int32_t android_action,
      int32_t pointer_count,
      int32_t history_size,
      int32_t action_index,
      int32_t android_action_button,
      int32_t android_gesture_classification,
      int32_t android_button_state,
      float raw_offset_x_pixels,
      float raw_offset_y_pixels,
      bool for_touch_handle,
      const MotionEventAndroid::Pointer* const pointer0,
      const MotionEventAndroid::Pointer* const pointer1);

  static std::unique_ptr<MotionEventAndroid> CreateFromJava(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& event,
      float pix_to_dip,
      float ticks_x,
      float ticks_y,
      float tick_multiplier,
      base::TimeTicks oldest_event_time,
      base::TimeTicks latest_event_time,
      base::TimeTicks down_time_ms,
      int32_t android_action,
      int32_t pointer_count,
      int32_t history_size,
      int32_t action_index,
      int32_t android_action_button,
      int32_t android_gesture_classification,
      int32_t android_button_state,
      float raw_offset_x_pixels,
      float raw_offset_y_pixels,
      bool for_touch_handle,
      const MotionEventAndroid::Pointer* const pointer0,
      const MotionEventAndroid::Pointer* const pointer1,
      bool is_latest_event_time_resampled);

  // TODO(crbug.com/383307455): Cleanup `event_times` once events are default
  // forwarded to Viz i.e. when `kForwardEventsSeenOnBrowserToViz` parameter is
  // default enabled for InputOnViz.
  static std::unique_ptr<MotionEventAndroid> CreateFromNative(
      base::android::ScopedInputEvent input_event,
      float pix_to_dip,
      gfx::PointF offset,
      std::optional<MotionEventAndroid::EventTimes> event_times);
};

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_FACTORY_H_
