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
#include "ui/events/events_export.h"
#include "ui/events/velocity_tracker/motion_event.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

// Implementation of ui::MotionEventAndroid wrapping a java Android MotionEvent.
// All *input* coordinates are in device pixels (as with Android MotionEvent),
// while all *output* coordinates are in DIPs (as with WebTouchEvent).
class EVENTS_EXPORT MotionEventAndroidJava : public MotionEventAndroid {
 public:
  // Forcing the caller to provide all cached values upon construction
  // eliminates the need to perform a JNI call to retrieve values individually.
  MotionEventAndroidJava(JNIEnv* env,
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
                         jint meta_state,
                         jint source,
                         jfloat raw_offset_x_pixels,
                         jfloat raw_offset_y_pixels,
                         jboolean for_touch_handle,
                         const Pointer* const pointer0,
                         const Pointer* const pointer1);

  MotionEventAndroidJava(JNIEnv* env,
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
                         jint meta_state,
                         jint source,
                         jfloat raw_offset_x_pixels,
                         jfloat raw_offset_y_pixels,
                         jboolean for_touch_handle,
                         const Pointer* const pointer0,
                         const Pointer* const pointer1);

  ~MotionEventAndroidJava() override;

  // Disallow copy/assign.
  MotionEventAndroidJava(const MotionEventAndroidJava& e) = delete;
  void operator=(const MotionEventAndroidJava&) = delete;

  // Start ui::MotionEvent overrides
  int GetPointerId(size_t pointer_index) const override;
  float GetX(size_t pointer_index) const override;
  float GetY(size_t pointer_index) const override;
  float GetTouchMajor(size_t pointer_index) const override;
  float GetTouchMinor(size_t pointer_index) const override;
  float GetOrientation(size_t pointer_index) const override;
  float GetPressure(size_t pointer_index) const override;
  float GetTiltX(size_t pointer_index) const override;
  float GetTiltY(size_t pointer_index) const override;
  base::TimeTicks GetHistoricalEventTime(
      size_t historical_index) const override;
  float GetHistoricalTouchMajor(size_t pointer_index,
                                size_t historical_index) const override;
  float GetHistoricalX(size_t pointer_index,
                       size_t historical_index) const override;
  float GetHistoricalY(size_t pointer_index,
                       size_t historical_index) const override;
  ToolType GetToolType(size_t pointer_index) const override;
  // End ui::MotionEvent overrides

  // Start MotionEventAndroid overrides
  std::unique_ptr<MotionEventAndroid> CreateFor(
      const gfx::PointF& point) const override;
  float GetXPix(size_t pointer_index) const override;
  float GetYPix(size_t pointer_index) const override;
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const override;
  // End MotionEventAndroid overrides

 private:
  // The Java reference to the underlying MotionEvent.
  base::android::ScopedJavaGlobalRef<jobject> event_;

  // Makes a copy of passed object |e| such that the cached pointers are
  // translated to new coordinates where the 0th indexded pointer points to
  // |point| and other pointer is translated accordingly if it exists.
  MotionEventAndroidJava(const MotionEventAndroidJava& e,
                         const gfx::PointF& point);
};

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_JAVA_H_
