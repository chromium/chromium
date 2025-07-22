// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_NATIVE_H_
#define UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_NATIVE_H_

#include <android/input.h>
#include <jni.h>
#include <stddef.h>
#include <stdint.h>

#include "ui/events/android/motion_event_android.h"
#include "ui/events/android/motion_event_android_source.h"
#include "ui/events/events_export.h"
#include "ui/events/velocity_tracker/motion_event.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

class MotionEventAndroidFactory;

class EVENTS_EXPORT MotionEventAndroidNative : public MotionEventAndroid {
 public:
  ~MotionEventAndroidNative() override;

  // Disallow copy/assign.
  MotionEventAndroidNative(const MotionEventAndroidNative& e) = delete;
  void operator=(const MotionEventAndroidNative&) = delete;

  // Start ui::MotionEvent overrides
  float GetPressure(size_t pointer_index) const override;
  // End ui::MotionEvent overrides

  // Start MotionEventAndroid overrides
  float GetXPix(size_t pointer_index) const override;
  float GetYPix(size_t pointer_index) const override;
  // End MotionEventAndroid overrides

 private:
  friend class MotionEventAndroidFactory;

  MotionEventAndroidNative(float pix_to_dip,
                           float ticks_x,
                           float ticks_y,
                           float tick_multiplier,
                           base::TimeTicks oldest_event_time,
                           base::TimeTicks latest_event_time,
                           base::TimeTicks cached_down_time_ms,
                           int android_action,
                           int pointer_count,
                           int history_size,
                           int action_index,
                           int android_action_button,
                           int android_gesture_classification,
                           int android_button_state,
                           int meta_state,
                           float raw_offset_x_pixels,
                           float raw_offset_y_pixels,
                           bool for_touch_handle,
                           const Pointer* const pointer0,
                           const Pointer* const pointer1,
                           std::unique_ptr<MotionEventAndroidSource> source);
};

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_NATIVE_H_
