// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_JAVA_H_
#define UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_JAVA_H_

#include <jni.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/events/android/motion_event_android_source.h"
#include "ui/events/events_export.h"
#include "ui/events/velocity_tracker/motion_event.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

class MotionEventAndroidFactory;

// Implementation of ui::MotionEventAndroid wrapping a java Android MotionEvent.
// All *input* coordinates are in device pixels (as with Android MotionEvent),
// while all *output* coordinates are in DIPs (as with WebTouchEvent).
class EVENTS_EXPORT MotionEventAndroidJava : public MotionEventAndroid {
 public:
  ~MotionEventAndroidJava() override;
  friend class MotionEventAndroidFactory;
  // Disallow copy/assign.
  MotionEventAndroidJava(const MotionEventAndroidJava& e) = delete;
  void operator=(const MotionEventAndroidJava&) = delete;

  // Start ui::MotionEvent overrides
  bool IsLatestEventTimeResampled() const override;
  // End ui::MotionEvent overrides

  // Start MotionEventAndroid overrides
  std::unique_ptr<MotionEventAndroid> CreateFor(
      const gfx::PointF& point) const override;
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const override;
  // End MotionEventAndroid overrides

 private:
  // Forcing the caller to provide all cached values upon construction
  // eliminates the need to perform a JNI call to retrieve values individually.
  MotionEventAndroidJava(jfloat pix_to_dip,
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
                         std::unique_ptr<MotionEventAndroidSource> source);

  // Makes a copy of passed object |e| such that the cached pointers are
  // translated to new coordinates where the 0th indexded pointer points to
  // |point| and other pointer is translated accordingly if it exists.
  MotionEventAndroidJava(const MotionEventAndroidJava& e,
                         const gfx::PointF& point);
};

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_JAVA_H_
