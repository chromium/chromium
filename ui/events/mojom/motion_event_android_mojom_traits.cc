// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/mojom/motion_event_android_mojom_traits.h"

#include "base/time/time.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "ui/events/android/cached_historical_event_source.h"
#include "ui/events/android/motion_event_android_java.h"
#include "ui/events/mojom/motion_event_android.mojom-shared.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<ui::mojom::PointerCoordinatesDataView,
                  ui::MotionEventAndroid::PointerCoordinates>::
    Read(ui::mojom::PointerCoordinatesDataView data,
         ui::MotionEventAndroid::PointerCoordinates* out) {
  if (!data.ReadPosition(&out->position)) {
    return false;
  }
  out->touch_major = data.touch_major();
  return true;
}

// static
bool StructTraits<ui::mojom::MotionEventAndroidCachedPointerDataView,
                  ui::MotionEventAndroid::CachedPointer>::
    Read(ui::mojom::MotionEventAndroidCachedPointerDataView data,
         ui::MotionEventAndroid::CachedPointer* out) {
  out->id = data.id();
  if (!data.ReadPointerData(&out->pointer_data)) {
    return false;
  }
  out->touch_minor = data.touch_minor();
  out->pressure = data.pressure();
  out->orientation = data.orientation();
  out->tilt_x = data.tilt_x();
  out->tilt_y = data.tilt_y();
  if (data.tool_type() < 0 ||
      data.tool_type() > static_cast<int>(ui::MotionEvent::ToolType::LAST)) {
    return false;
  }
  out->tool_type = static_cast<ui::MotionEvent::ToolType>(data.tool_type());
  return true;
}

// static
bool StructTraits<ui::mojom::HistoricalCachedPointerDataView,
                  ui::CachedHistoricalEventSource::HistoricalCachedPointer>::
    Read(ui::mojom::HistoricalCachedPointerDataView data,
         ui::CachedHistoricalEventSource::HistoricalCachedPointer* out) {
  if (!data.ReadPointers(&out->pointers)) {
    return false;
  }
  if (!data.ReadEventTime(&out->event_time)) {
    return false;
  }
  return true;
}

// static
std::vector<ui::MotionEventAndroid::CachedPointer>
StructTraits<ui::mojom::CachedMotionEventAndroidDataView,
             std::unique_ptr<ui::MotionEventAndroid>>::
    pointers(const std::unique_ptr<ui::MotionEventAndroid>& event) {
  std::vector<ui::MotionEventAndroid::CachedPointer> cached_pointers;
  cached_pointers.reserve(event->GetPointerCount());

  for (size_t i = 0; i < event->GetPointerCount(); i++) {
    ui::MotionEventAndroid::PointerCoordinates historical_pointer{
        .position = gfx::PointF(event->GetX(i), event->GetY(i)),
        .touch_major = event->GetTouchMajor(i)};
    cached_pointers.emplace_back();
    ui::MotionEventAndroid::CachedPointer& pointer = cached_pointers.back();
    pointer.id = event->GetPointerId(i);
    pointer.pointer_data = historical_pointer;
    pointer.touch_minor = event->GetTouchMinor(i);
    pointer.pressure = event->GetPressure(i);
    pointer.orientation = event->GetOrientation(i);
    pointer.tilt_x = event->GetTiltX(i);
    pointer.tilt_y = event->GetTiltY(i);
    pointer.tool_type = event->GetToolType(i);
  }
  return cached_pointers;
}

// static
std::vector<ui::CachedHistoricalEventSource::HistoricalCachedPointer>
StructTraits<ui::mojom::CachedMotionEventAndroidDataView,
             std::unique_ptr<ui::MotionEventAndroid>>::
    historical_events(const std::unique_ptr<ui::MotionEventAndroid>& event) {
  std::vector<ui::CachedHistoricalEventSource::HistoricalCachedPointer>
      historical_events;
  historical_events.reserve(event->GetHistorySize());
  for (size_t history_index = 0; history_index < event->GetHistorySize();
       history_index++) {
    historical_events.emplace_back();
    ui::CachedHistoricalEventSource::HistoricalCachedPointer& historical_event =
        historical_events.back();
    historical_event.event_time = event->GetHistoricalEventTime(history_index);

    historical_event.pointers.reserve(event->GetPointerCount());
    for (size_t pointer_index = 0; pointer_index < event->GetPointerCount();
         pointer_index++) {
      historical_event.pointers.emplace_back(
          gfx::PointF(event->GetHistoricalX(pointer_index, history_index),
                      event->GetHistoricalY(pointer_index, history_index)),
          event->GetHistoricalTouchMajor(pointer_index, history_index));
    }
  }
  return historical_events;
}
// static
bool StructTraits<ui::mojom::CachedMotionEventAndroidDataView,
                  std::unique_ptr<ui::MotionEventAndroid>>::
    Read(ui::mojom::CachedMotionEventAndroidDataView data,
         std::unique_ptr<ui::MotionEventAndroid>* out) {
  auto cached_source = std::make_unique<ui::CachedHistoricalEventSource>();
  auto event = std::make_unique<ui::MotionEventAndroid>();

  std::vector<ui::MotionEventAndroid::CachedPointer> pointers;
  if (!data.ReadPointers(&pointers)) {
    return false;
  }

  if (pointers.empty()) {
    return false;
  }

  event->cached_pointer_count_ = pointers.size();

  for (const auto& cached_pointer : pointers) {
    event->cached_pointers_.push_back(cached_pointer);
  }

  event->pix_to_dip_ = data.pix_to_dip();
  cached_source->pix_to_dip_ = event->pix_to_dip_;
  event->ticks_x_ = data.ticks_x();
  event->ticks_y_ = data.ticks_y();
  event->tick_multiplier_ = data.tick_multiplier();
  event->for_touch_handle_ = data.for_touch_handle();
  if (!data.ReadLatestEventTime(&event->cached_latest_event_time_)) {
    return false;
  }
  if (!data.ReadDownTimeMs(&event->cached_down_time_ms_)) {
    return false;
  }
  if (!data.ReadHistoricalEvents(&cached_source->historical_events_)) {
    return false;
  }
  if (cached_source->historical_events_.empty()) {
    event->cached_oldest_event_time_ = event->cached_latest_event_time_;
  } else {
    for (const auto& historical_event : cached_source->historical_events_) {
      if (historical_event.pointers.size() != event->cached_pointer_count_) {
        return false;
      }
    }
    event->cached_oldest_event_time_ =
        cached_source->historical_events_[0].event_time;
  }
  cached_source->input_source_ = data.source();
  event->cached_action_ = static_cast<ui::MotionEvent::Action>(data.action());
  event->cached_history_size_ = cached_source->historical_events_.size();
  event->cached_action_index_ = data.action_index();
  event->cached_action_button_ = data.action_button();
  event->cached_gesture_classification_ = data.gesture_classification();
  event->cached_button_state_ = data.button_state();
  event->cached_flags_ = data.flags();
  if (!data.ReadRawPositionOffset(&event->cached_raw_position_offset_)) {
    return false;
  }
  event->source_ = std::move(cached_source);
  *out = std::move(event);
  return true;
}

}  // namespace mojo
