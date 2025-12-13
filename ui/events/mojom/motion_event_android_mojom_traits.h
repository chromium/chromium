// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_MOJOM_MOTION_EVENT_ANDROID_MOJOM_TRAITS_H_
#define UI_EVENTS_MOJOM_MOTION_EVENT_ANDROID_MOJOM_TRAITS_H_

#include "base/time/time.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "ui/events/android/cached_historical_event_source.h"
#include "ui/events/android/motion_event_android_java.h"
#include "ui/events/mojom/motion_event_android.mojom-shared.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<ui::mojom::PointerCoordinatesDataView,
                    ui::MotionEventAndroid::PointerCoordinates> {
  static const gfx::PointF& position(
      const ui::MotionEventAndroid::PointerCoordinates& pointer) {
    return pointer.position;
  }

  static float touch_major(
      const ui::MotionEventAndroid::PointerCoordinates& pointer) {
    return pointer.touch_major;
  }

  static bool Read(ui::mojom::PointerCoordinatesDataView data,
                   ui::MotionEventAndroid::PointerCoordinates* out);
};

template <>
struct StructTraits<ui::mojom::MotionEventAndroidCachedPointerDataView,
                    ui::MotionEventAndroid::CachedPointer> {
  static int id(const ui::MotionEventAndroid::CachedPointer& pointer) {
    return pointer.id;
  }

  static const ui::MotionEventAndroid::PointerCoordinates& pointer_data(
      const ui::MotionEventAndroid::CachedPointer& pointer) {
    return pointer.pointer_data;
  }
  static float touch_minor(
      const ui::MotionEventAndroid::CachedPointer& pointer) {
    return pointer.touch_minor;
  }
  static float pressure(const ui::MotionEventAndroid::CachedPointer& pointer) {
    return pointer.pressure;
  }
  static float orientation(
      const ui::MotionEventAndroid::CachedPointer& pointer) {
    return pointer.orientation;
  }
  static float tilt_x(const ui::MotionEventAndroid::CachedPointer& pointer) {
    return pointer.tilt_x;
  }
  static float tilt_y(const ui::MotionEventAndroid::CachedPointer& pointer) {
    return pointer.tilt_y;
  }
  static int tool_type(const ui::MotionEventAndroid::CachedPointer& pointer) {
    return static_cast<int>(pointer.tool_type);
  }
  static bool Read(ui::mojom::MotionEventAndroidCachedPointerDataView data,
                   ui::MotionEventAndroid::CachedPointer* out);
};

template <>
struct StructTraits<ui::mojom::HistoricalCachedPointerDataView,
                    ui::CachedHistoricalEventSource::HistoricalCachedPointer> {
  static const base::TimeTicks& event_time(
      const ui::CachedHistoricalEventSource::HistoricalCachedPointer& data) {
    return data.event_time;
  }
  static const std::vector<ui::MotionEventAndroid::PointerCoordinates>&
  pointers(
      const ui::CachedHistoricalEventSource::HistoricalCachedPointer& data) {
    return data.pointers;
  }
  static bool Read(
      ui::mojom::HistoricalCachedPointerDataView data,
      ui::CachedHistoricalEventSource::HistoricalCachedPointer* out);
};

template <>
struct StructTraits<ui::mojom::CachedMotionEventAndroidDataView,
                    std::unique_ptr<ui::MotionEventAndroid>> {
  static bool IsNull(std::unique_ptr<ui::MotionEventAndroid>& event) {
    return !event;
  }

  static void SetToNull(std::unique_ptr<ui::MotionEventAndroid>* event) {
    event->reset();
  }

  static float pix_to_dip(
      const std::unique_ptr<ui::MotionEventAndroid>& event) {
    return event->pix_to_dip_;
  }
  static float ticks_x(const std::unique_ptr<ui::MotionEventAndroid>& event) {
    return event->ticks_x_;
  }
  static float ticks_y(const std::unique_ptr<ui::MotionEventAndroid>& event) {
    return event->ticks_y_;
  }
  static float tick_multiplier(
      const std::unique_ptr<ui::MotionEventAndroid>& event) {
    return event->tick_multiplier_;
  }
  static int source(const std::unique_ptr<ui::MotionEventAndroid>& event) {
    return event->GetSource();
  }
  static bool for_touch_handle(
      const std::unique_ptr<ui::MotionEventAndroid>& event) {
    return event->for_touch_handle_;
  }
  static const base::TimeTicks& latest_event_time(
      const std::unique_ptr<ui::MotionEventAndroid>& event) {
    return event->cached_latest_event_time_;
  }
  static const base::TimeTicks& down_time_ms(
      const std::unique_ptr<ui::MotionEventAndroid>& event) {
    return event->cached_down_time_ms_;
  }
  static int action(const std::unique_ptr<ui::MotionEventAndroid>& event) {
    return static_cast<int>(event->cached_action_);
  }
  static int action_index(
      const std::unique_ptr<ui::MotionEventAndroid>& event) {
    return event->cached_action_index_;
  }
  static int action_button(
      const std::unique_ptr<ui::MotionEventAndroid>& event) {
    return event->cached_action_button_;
  }
  static int gesture_classification(
      const std::unique_ptr<ui::MotionEventAndroid>& event) {
    return event->cached_gesture_classification_;
  }
  static int button_state(
      const std::unique_ptr<ui::MotionEventAndroid>& event) {
    return event->cached_button_state_;
  }
  static int flags(const std::unique_ptr<ui::MotionEventAndroid>& event) {
    return event->cached_flags_;
  }
  static const gfx::Vector2dF& raw_position_offset(
      const std::unique_ptr<ui::MotionEventAndroid>& event) {
    return event->cached_raw_position_offset_;
  }
  static std::vector<ui::MotionEventAndroid::CachedPointer> pointers(
      const std::unique_ptr<ui::MotionEventAndroid>& event);
  static std::vector<ui::CachedHistoricalEventSource::HistoricalCachedPointer>
  historical_events(const std::unique_ptr<ui::MotionEventAndroid>& event);

  static bool Read(ui::mojom::CachedMotionEventAndroidDataView data,
                   std::unique_ptr<ui::MotionEventAndroid>* out);
};

}  // namespace mojo

#endif  // UI_EVENTS_MOJOM_MOTION_EVENT_ANDROID_MOJOM_TRAITS_H_
