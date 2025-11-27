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
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "ui/events/android/motion_event_android_source.h"
#include "ui/events/events_export.h"
#include "ui/events/velocity_tracker/motion_event.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

namespace mojom {
class CachedMotionEventAndroidDataView;
}

// When adding new fields to this class, update the corresponding Mojo struct
// traits to ensure the new fields are propagated correctly. Ignore the lint
// warning if no new fields are added.
// LINT.IfChange(MotionEventAndroid)

// An abstract base class which caches Android's MotionEvent object values and
// depends on impl classes for uncached data.
// All *input* coordinates are in device pixels (as with Android MotionEvent),
// while all *output* coordinates are in DIPs (as with WebTouchEvent).
class EVENTS_EXPORT MotionEventAndroid : public MotionEvent {
 public:
  // A struct to hold the oldest and latest event times.
  struct EventTimes {
    base::TimeTicks oldest;
    base::TimeTicks latest;
  };

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
            jfloat pressure,
            jfloat orientation_rad,
            jfloat tilt_rad,
            jint tool_type);
    jint id;
    jfloat pos_x_pixels;
    jfloat pos_y_pixels;
    jfloat touch_major_pixels;
    jfloat touch_minor_pixels;
    jfloat pressure;
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
                     base::TimeTicks cached_down_time_ms,
                     int android_action,
                     int pointer_count,
                     int history_size,
                     int action_index,
                     int android_action_button,
                     int android_gesture_classification,
                     int android_button_state,
                     int android_meta_state,
                     float raw_offset_x_pixels,
                     float raw_offset_y_pixels,
                     bool for_touch_handle,
                     const Pointer* const pointer0,
                     const Pointer* const pointer1,
                     std::unique_ptr<MotionEventAndroidSource> source);

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
  // Pointer count as returned by Android's `MotionEvent.getPointerCount()`.
  // This could be different than `cached_pointers_.size()` which by default
  // only caches two pointers.
  size_t GetPointerCount() const override;
  int GetPointerId(size_t pointer_index) const override;
  float GetX(size_t pointer_index) const override;
  float GetY(size_t pointer_index) const override;
  float GetRawX(size_t pointer_index) const override;
  float GetRawY(size_t pointer_index) const override;
  float GetTouchMajor(size_t pointer_index) const override;
  float GetTouchMinor(size_t pointer_index) const override;
  bool HasNativeTouchMajor(size_t pointer_index) const override;
  float GetOrientation(size_t pointer_index) const override;
  float GetTwist(size_t pointer_index) const override;
  float GetTiltX(size_t pointer_index) const override;
  float GetTiltY(size_t pointer_index) const override;
  float GetPressure(size_t pointer_index) const override;
  float GetTangentialPressure(size_t pointer_index) const override;
  // TODO(crbug.com/41493853): Cleanup GetEventTime method to have same
  // semantics as Android side of MotionEvent.GetEventTime(). On Android side
  // GetEventTime() gives timestamp of the most recent input event, while in
  // chromium it gives timestamp of the oldest input event for batched inputs.
  base::TimeTicks GetEventTime() const override;
  base::TimeTicks GetLatestEventTime() const override;
  base::TimeTicks GetRawDownTime() const override;
  size_t GetHistorySize() const override;
  base::TimeTicks GetHistoricalEventTime(
      size_t historical_index) const override;
  float GetHistoricalTouchMajor(size_t pointer_index,
                                size_t historical_index) const override;
  float GetHistoricalX(size_t pointer_index,
                       size_t historical_index) const override;
  float GetHistoricalY(size_t pointer_index,
                       size_t historical_index) const override;
  int GetSourceDeviceId(size_t pointer_index) const override;
  ToolType GetToolType(size_t pointer_index) const override;
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

  float GetRawXPix(size_t pointer_index) const;
  float GetXPix(size_t pointer_index) const;
  float GetYPix(size_t pointer_index) const;

  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const;

  struct PointerCoordinates {
    gfx::PointF position;
    float touch_major = 0;
  };

  struct CachedPointer {
    CachedPointer();
    int id = 0;
    PointerCoordinates pointer_data;
    float touch_minor = 0;
    float pressure = 0;
    float orientation = 0;
    float tilt_x = 0;
    float tilt_y = 0;
    ToolType tool_type = ToolType::UNKNOWN;
  };

  MotionEventAndroid();

 protected:
  float pix_to_dip() const { return pix_to_dip_; }

  // Returns the id of the pointer at `pointer_index` from the cache.
  int GetCachedPointerId(size_t pointer_index) const;

  // Returns the position of the pointer at `pointer_index` from the cache.
  const gfx::PointF& GetCachedPointerPosition(size_t pointer_index) const;

  // Returns the touch major/minor of the pointer at `pointer_index` from the
  // cache.
  float GetCachedPointerTouchMajor(size_t pointer_index) const;
  float GetCachedPointerTouchMinor(size_t pointer_index) const;

  // Returns the pressure/orientation of the pointer at `pointer_index` from the
  // cache.
  float GetCachedPointerPressure(size_t pointer_index) const;
  float GetCachedPointerOrientation(size_t pointer_index) const;

  // Returns the tilt of the pointer at `pointer_index` from the cache.
  float GetCachedPointerTiltX(size_t pointer_index) const;
  float GetCachedPointerTiltY(size_t pointer_index) const;

  // Returns the tool type of the pointer at `pointer_index` from the cache.
  ToolType GetCachedPointerToolType(size_t pointer_index) const;

  MotionEventAndroid(const MotionEventAndroid& e, const gfx::PointF& point);

  const MotionEventAndroidSource* source() const { return source_.get(); }

 private:
  friend struct mojo::StructTraits<ui::mojom::CachedMotionEventAndroidDataView,
                                   std::unique_ptr<ui::MotionEventAndroid>>;
  FRIEND_TEST_ALL_PREFIXES(MotionEventAndroidTraitsTest,
                           MultiplePointersWithHistory);

  float ToDips(float pixels) const;

  // Cache pointer coords, id's and major lengths for the most common
  // touch-related scenarios, i.e., scrolling and pinching which has at most two
  // active pointers. This prevents redundant JNI fetches for the same bits.
  static constexpr const size_t kDefaultCachedPointers = 2;
  absl::InlinedVector<CachedPointer, kDefaultCachedPointers> cached_pointers_;

  CachedPointer FromAndroidPointer(const Pointer& pointer) const;
  CachedPointer CreateCachedPointer(const CachedPointer& pointer,
                                    const gfx::PointF& point) const;

  // Used to convert pixel coordinates from the Java-backed MotionEvent to
  // DIP coordinates cached/returned by the MotionEventAndroid.
  float pix_to_dip_;

  // Variables for mouse wheel event.
  float ticks_x_;
  float ticks_y_;
  float tick_multiplier_;

  bool for_touch_handle_;

  // |cached_oldest_event_time_| and |cached_latest_event_time_| are same when
  // history size is 0, in presence of historical events
  // |cached_oldest_event_time_| is the event time of oldest coalesced event.
  base::TimeTicks cached_oldest_event_time_;
  base::TimeTicks cached_latest_event_time_;
  // This stores the event time of first down event in touch sequence, it is
  // obtained from MotionEvent.getDownTime for java backed events and
  // from AMotionEvent_getDowntime for native backed events.
  base::TimeTicks cached_down_time_ms_;
  Action cached_action_;
  size_t cached_pointer_count_;
  size_t cached_history_size_;
  int cached_action_index_;
  int cached_action_button_;
  int cached_gesture_classification_;
  int cached_button_state_;
  int cached_flags_;
  gfx::Vector2dF cached_raw_position_offset_;

  // A unique identifier for the Android motion event.
  const uint32_t unique_event_id_;

  std::unique_ptr<MotionEventAndroidSource> source_;
};

// LINT.ThenChange(//ui/events/mojom/motion_event_android_mojom_traits.h:MotionEventAndroid)

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_H_
