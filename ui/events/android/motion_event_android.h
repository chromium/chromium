// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_H_
#define UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_H_

#include <jni.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "ui/events/events_export.h"
#include "ui/events/velocity_tracker/motion_event.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

// An abstract base class which caches Android's MotionEvent object values and
// depends on impl classes for uncached data.
// All *input* coordinates are in device pixels (as with Android MotionEvent),
// while all *output* coordinates are in DIPs (as with WebTouchEvent).
class EVENTS_EXPORT MotionEventAndroid : public MotionEvent {
 public:
  // Returns the motion event action defined in Java layer for a given
  // MotionEvent::Action.
  static int GetAndroidAction(Action action);
  static int GetAndroidToolType(ToolType tool_type);

  struct Pointer {
    Pointer(jint id,
            jfloat pos_x_pixels,
            jfloat pos_y_pixels,
            jfloat touch_major_pixels,
            jfloat touch_minor_pixels,
            jfloat orientation_rad,
            jfloat tilt_rad,
            jint tool_type);
    jint id;
    jfloat pos_x_pixels;
    jfloat pos_y_pixels;
    jfloat touch_major_pixels;
    jfloat touch_minor_pixels;
    jfloat orientation_rad;
    // Unlike the tilt angles in motion_event.h, this field matches the
    // MotionEvent spec because we get this values from Java.
    jfloat tilt_rad;
    jint tool_type;
  };

  MotionEventAndroid(float pix_to_dip,
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
                     const Pointer* const pointer1);

  ~MotionEventAndroid() override;

  // Create a new instance from |this| with its cached pointers set
  // to a given point.
  virtual std::unique_ptr<MotionEventAndroid> CreateFor(
      const gfx::PointF& point) const;

  // Convenience method returning the pointer at index 0.
  gfx::PointF GetPoint() const { return gfx::PointF(GetX(0), GetY(0)); }
  gfx::PointF GetPointPix() const {
    return gfx::PointF(GetXPix(0), GetYPix(0));
  }

  // Start ui::MotionEvent overrides
  uint32_t GetUniqueEventId() const override;
  Action GetAction() const override;
  int GetActionIndex() const override;
  size_t GetPointerCount() const override;
  float GetRawX(size_t pointer_index) const override;
  float GetRawY(size_t pointer_index) const override;
  float GetTwist(size_t pointer_index) const override;
  float GetTangentialPressure(size_t pointer_index) const override;
  // TODO(crbug.com/41493853): Cleanup GetEventTime method to have same
  // semantics as Android side of MotionEvent.GetEventTime(). On Android side
  // GetEventTime() gives timestamp of the most recent input event, while in
  // chromium it gives timestamp of the oldest input event for batched inputs.
  base::TimeTicks GetEventTime() const override;
  base::TimeTicks GetLatestEventTime() const override;
  size_t GetHistorySize() const override;
  int GetSourceDeviceId(size_t pointer_index) const override;
  int GetButtonState() const override;
  int GetFlags() const override;
  Classification GetClassification() const override;
  // End ui::MotionEvent overrides

  int GetActionButton() const;
  int GetSource() const;
  float ticks_x() const { return ticks_x_; }
  float ticks_y() const { return ticks_y_; }
  float GetTickMultiplier() const;
  bool for_touch_handle() const { return for_touch_handle_; }

  virtual float GetXPix(size_t pointer_index) const = 0;
  virtual float GetYPix(size_t pointer_index) const = 0;

  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const;

 protected:
  float pix_to_dip() const { return pix_to_dip_; }
  float ToDips(float pixels) const;

  // Cache pointer coords, id's and major lengths for the most common
  // touch-related scenarios, i.e., scrolling and pinching.  This prevents
  // redundant JNI fetches for the same bits.
  enum { MAX_POINTERS_TO_CACHE = 2 };

  MotionEventAndroid(const MotionEventAndroid& e, const gfx::PointF& point);

  struct CachedPointer {
    CachedPointer();
    int id = 0;
    gfx::PointF position;
    float touch_major = 0;
    float touch_minor = 0;
    float orientation = 0;
    float tilt_x = 0;
    float tilt_y = 0;
    ToolType tool_type = ToolType::UNKNOWN;
  } cached_pointers_[MAX_POINTERS_TO_CACHE];

  static ToolType FromAndroidToolType(int android_tool_type);
  static base::TimeTicks FromAndroidTime(base::TimeTicks time);
  static float ToValidFloat(float x);
  static void ConvertTiltOrientationToTiltXY(float tilt_rad,
                                             float orientation_rad,
                                             float* tilt_x,
                                             float* tilt_y);

 private:
  CachedPointer FromAndroidPointer(const Pointer& pointer) const;
  CachedPointer CreateCachedPointer(const CachedPointer& pointer,
                                    const gfx::PointF& point) const;

  // Used to convert pixel coordinates from the Java-backed MotionEvent to
  // DIP coordinates cached/returned by the MotionEventAndroid.
  const float pix_to_dip_;

  // Variables for mouse wheel event.
  const float ticks_x_;
  const float ticks_y_;
  const float tick_multiplier_;
  const int source_;

  const bool for_touch_handle_;

  const base::TimeTicks cached_oldest_event_time_;
  const base::TimeTicks cached_latest_event_time_;
  const Action cached_action_;
  const size_t cached_pointer_count_;
  const size_t cached_history_size_;
  const int cached_action_index_;
  const int cached_action_button_;
  const int cached_gesture_classification_;
  const int cached_button_state_;
  const int cached_flags_;
  const gfx::Vector2dF cached_raw_position_offset_;

  // A unique identifier for the Android motion event.
  const uint32_t unique_event_id_;
};

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_H_
