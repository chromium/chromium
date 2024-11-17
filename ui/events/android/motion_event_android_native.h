// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_NATIVE_H_
#define UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_NATIVE_H_

#include <android/input.h>
#include <jni.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/events/events_export.h"
#include "ui/events/velocity_tracker/motion_event.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

class EVENTS_EXPORT MotionEventAndroidNative : public MotionEventAndroid {
 public:
  ~MotionEventAndroidNative() override;

  // Disallow copy/assign.
  MotionEventAndroidNative(const MotionEventAndroidNative& e) = delete;
  void operator=(const MotionEventAndroidNative&) = delete;

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
  float GetXPix(size_t pointer_index) const override;
  float GetYPix(size_t pointer_index) const override;
  // End MotionEventAndroid overrides

  static std::unique_ptr<MotionEventAndroid> Create(const AInputEvent* event,
                                                    float pix_to_dip,
                                                    int y_offset_pix);

 private:
  MotionEventAndroidNative(const AInputEvent* event,
                           float pix_to_dip,
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
                           int meta_state,
                           int source,
                           float raw_offset_x_pixels,
                           float raw_offset_y_pixels,
                           bool for_touch_handle,
                           const Pointer* const pointer0,
                           const Pointer* const pointer1,
                           int y_offset);

  raw_ptr<const AInputEvent> native_event_;
  // Amount of value to offset Y axis values by to accommodate for top controls.
  int y_offset_pix_;
};

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_NATIVE_H_
