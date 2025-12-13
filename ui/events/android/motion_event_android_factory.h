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

namespace ui {

// Factory class for creating instances of MotionEventAndroid.
class EVENTS_EXPORT MotionEventAndroidFactory {
 public:
  // Creates a MotionEventAndroid from a Java MotionEvent object.
  static std::unique_ptr<MotionEventAndroid> CreateFromJava(
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
      const MotionEventAndroid::Pointer* const pointer1);

  static std::unique_ptr<MotionEventAndroid> CreateFromJava(
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
      bool is_latest_event_time_resampled);

  // TODO(crbug.com/383307455): Cleanup `event_times` once events are default
  // forwarded to Viz i.e. when `kForwardEventsSeenOnBrowserToViz` parameter is
  // default enabled for InputOnViz.
  static std::unique_ptr<MotionEventAndroid> CreateFromNative(
      base::android::ScopedInputEvent input_event,
      float pix_to_dip,
      float y_offset_pix,
      std::optional<MotionEventAndroid::EventTimes> event_times);
};

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_FACTORY_H_
