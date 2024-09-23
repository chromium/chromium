// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/velocity_tracker/motion_event_generic.h"

#include <numbers>
#include <ostream>
#include <utility>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/angle_conversions.h"
#include "ui/events/base_event_utils.h"

namespace ui {

PointerProperties::PointerProperties()
    : PointerProperties(0, 0, 0) {
}

PointerProperties::PointerProperties(float x, float y, float touch_major)
    : id(0),
      tool_type(MotionEvent::ToolType::UNKNOWN),
      x(x),
      y(y),
      raw_x(x),
      raw_y(y),
      pressure(0),
      touch_major(touch_major),
      touch_minor(0),
      orientation(0),
      tilt_x(0),
      tilt_y(0),
      twist(0),
      tangential_pressure(0),
      source_device_id(0) {}

PointerProperties::PointerProperties(const MotionEvent& event,
                                     size_t pointer_index)
    : id(event.GetPointerId(pointer_index)),
      tool_type(event.GetToolType(pointer_index)),
      x(event.GetX(pointer_index)),
      y(event.GetY(pointer_index)),
      raw_x(event.GetRawX(pointer_index)),
      raw_y(event.GetRawY(pointer_index)),
      pressure(event.GetPressure(pointer_index)),
      touch_major(event.GetTouchMajor(pointer_index)),
      touch_minor(event.GetTouchMinor(pointer_index)),
      orientation(event.GetOrientation(pointer_index)),
      tilt_x(event.GetTiltX(pointer_index)),
      tilt_y(event.GetTiltY(pointer_index)),
      twist(event.GetTwist(pointer_index)),
      tangential_pressure(event.GetTangentialPressure(pointer_index)),
      source_device_id(0) {}

PointerProperties::PointerProperties(const PointerProperties& other) = default;

PointerProperties& PointerProperties::operator=(
    const PointerProperties& other) = default;

void PointerProperties::SetAxesAndOrientation(float radius_x,
                                              float radius_y,
                                              float rotation_angle_degree) {
  DCHECK(!touch_major && !touch_minor && !orientation);
  float rotation_angle_rad = base::DegToRad(rotation_angle_degree);
  DCHECK_GE(radius_x, 0) << "Unexpected x-radius < 0 (" << radius_x << ")";
  DCHECK_GE(radius_y, 0) << "Unexpected y-radius < 0 (" << radius_y << ")";
  DCHECK(0 <= rotation_angle_rad &&
         rotation_angle_rad < std::numbers::pi_v<float>)
      << "Unexpected touch rotation angle " << rotation_angle_rad << " rad";

  // Make the angle acute to ease subsequent logic. The angle range effectively
  // changes from [0, pi) to [0, pi/2).
  if (rotation_angle_rad >= std::numbers::pi_v<float> / 2) {
    rotation_angle_rad -= std::numbers::pi_v<float> / 2;
    std::swap(radius_x, radius_y);
  }

  if (radius_x > radius_y) {
    // The case radius_x == radius_y is omitted from here on purpose: for
    // circles, we want to pass the angle (which could be any value in such
    // cases but always seem to be set to zero) unchanged.
    touch_major = 2.f * radius_x;
    touch_minor = 2.f * radius_y;
    orientation = rotation_angle_rad - std::numbers::pi_v<float> / 2;
  } else {
    touch_major = 2.f * radius_y;
    touch_minor = 2.f * radius_x;
    orientation = rotation_angle_rad;
  }
}

MotionEventGeneric::MotionEventGeneric(Action action,
                                       base::TimeTicks event_time,
                                       const PointerProperties& pointer)
    : action_(action),
      event_time_(event_time),
      unique_event_id_(ui::GetNextTouchEventId()),
      action_index_(0),
      button_state_(0),
      flags_(0) {
  PushPointer(pointer);
}

MotionEventGeneric::MotionEventGeneric(const MotionEventGeneric& other)
    : action_(other.action_),
      event_time_(other.event_time_),
      unique_event_id_(other.unique_event_id_),
      action_index_(other.action_index_),
      button_state_(other.button_state_),
      flags_(other.flags_),
      pointers_(other.pointers_) {
  const size_t history_size = other.GetHistorySize();
  for (size_t h = 0; h < history_size; ++h)
    PushHistoricalEvent(other.historical_events_[h]->Clone());
}

MotionEventGeneric::~MotionEventGeneric() {
}

uint32_t MotionEventGeneric::GetUniqueEventId() const {
  return unique_event_id_;
}

MotionEvent::Action MotionEventGeneric::GetAction() const {
  return action_;
}

int MotionEventGeneric::GetActionIndex() const {
  DCHECK(action_ == Action::POINTER_DOWN || action_ == Action::POINTER_UP);
  DCHECK_GE(action_index_, 0);
  DCHECK_LT(action_index_, static_cast<int>(pointers_.size()));
  return action_index_;
}

size_t MotionEventGeneric::GetPointerCount() const {
  return pointers_.size();
}

int MotionEventGeneric::GetPointerId(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_.size());
  return pointers_[pointer_index].id;
}

float MotionEventGeneric::GetX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_.size());
  return pointers_[pointer_index].x;
}

float MotionEventGeneric::GetY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_.size());
  return pointers_[pointer_index].y;
}

float MotionEventGeneric::GetRawX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_.size());
  return pointers_[pointer_index].raw_x;
}

float MotionEventGeneric::GetRawY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_.size());
  return pointers_[pointer_index].raw_y;
}

float MotionEventGeneric::GetTouchMajor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_.size());
  return pointers_[pointer_index].touch_major;
}

float MotionEventGeneric::GetTouchMinor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_.size());
  return pointers_[pointer_index].touch_minor;
}

float MotionEventGeneric::GetOrientation(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_.size());
  return pointers_[pointer_index].orientation;
}

float MotionEventGeneric::GetPressure(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_.size());
  return pointers_[pointer_index].pressure;
}

float MotionEventGeneric::GetTiltX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_.size());
  return pointers_[pointer_index].tilt_x;
}

float MotionEventGeneric::GetTiltY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_.size());
  return pointers_[pointer_index].tilt_y;
}

float MotionEventGeneric::GetTwist(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_.size());
  return pointers_[pointer_index].twist;
}

float MotionEventGeneric::GetTangentialPressure(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_.size());
  return pointers_[pointer_index].tangential_pressure;
}

MotionEvent::ToolType MotionEventGeneric::GetToolType(
    size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_.size());
  return pointers_[pointer_index].tool_type;
}

int MotionEventGeneric::GetButtonState() const {
  return button_state_;
}

int MotionEventGeneric::GetFlags() const {
  return flags_;
}

base::TimeTicks MotionEventGeneric::GetEventTime() const {
  return event_time_;
}

size_t MotionEventGeneric::GetHistorySize() const {
  return historical_events_.size();
}

base::TimeTicks MotionEventGeneric::GetHistoricalEventTime(
    size_t historical_index) const {
  DCHECK_LT(historical_index, historical_events_.size());
  return historical_events_[historical_index]->GetEventTime();
}

float MotionEventGeneric::GetHistoricalTouchMajor(
    size_t pointer_index,
    size_t historical_index) const {
  DCHECK_LT(historical_index, historical_events_.size());
  return historical_events_[historical_index]->GetTouchMajor(pointer_index);
}

float MotionEventGeneric::GetHistoricalX(size_t pointer_index,
                                         size_t historical_index) const {
  DCHECK_LT(historical_index, historical_events_.size());
  return historical_events_[historical_index]->GetX(pointer_index);
}

float MotionEventGeneric::GetHistoricalY(size_t pointer_index,
                                         size_t historical_index) const {
  DCHECK_LT(historical_index, historical_events_.size());
  return historical_events_[historical_index]->GetY(pointer_index);
}

int32_t MotionEventGeneric::GetSourceDeviceId(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_.size());
  return pointers_[pointer_index].source_device_id;
}

// static
std::unique_ptr<MotionEventGeneric> MotionEventGeneric::CloneEvent(
    const MotionEvent& event) {
  bool with_history = true;
  return base::WrapUnique(new MotionEventGeneric(event, with_history));
}

// static
std::unique_ptr<MotionEventGeneric> MotionEventGeneric::CancelEvent(
    const MotionEvent& event) {
  bool with_history = false;
  std::unique_ptr<MotionEventGeneric> cancel_event(
      new MotionEventGeneric(event, with_history));
  cancel_event->set_action(Action::CANCEL);
  cancel_event->set_unique_event_id(ui::GetNextTouchEventId());
  return cancel_event;
}

size_t MotionEventGeneric::PushPointer(const PointerProperties& pointer) {
  DCHECK_EQ(0U, GetHistorySize());
  pointers_.push_back(pointer);
  return pointers_.size() - 1;
}

void MotionEventGeneric::RemovePointerAt(size_t index) {
  DCHECK_LT(index, pointers_.size());
  pointers_.erase(pointers_.begin() + index);
}

void MotionEventGeneric::PushHistoricalEvent(
    std::unique_ptr<MotionEvent> event) {
  DCHECK(event);
  DCHECK_EQ(event->GetAction(), Action::MOVE);
  DCHECK_EQ(event->GetPointerCount(), GetPointerCount());
  DCHECK_EQ(event->GetAction(), GetAction());
  DCHECK_LE(event->GetEventTime(), GetEventTime());
  historical_events_.push_back(std::move(event));
}

MotionEventGeneric::MotionEventGeneric()
    : action_(Action::NONE),
      unique_event_id_(ui::GetNextTouchEventId()),
      action_index_(-1),
      button_state_(0) {}

MotionEventGeneric::MotionEventGeneric(const MotionEvent& event,
                                       bool with_history)
    : action_(event.GetAction()),
      event_time_(event.GetEventTime()),
      unique_event_id_(event.GetUniqueEventId()),
      action_index_(
          (action_ == Action::POINTER_UP || action_ == Action::POINTER_DOWN)
              ? event.GetActionIndex()
              : 0),
      button_state_(event.GetButtonState()),
      flags_(event.GetFlags()) {
  const size_t pointer_count = event.GetPointerCount();
  for (size_t i = 0; i < pointer_count; ++i)
    PushPointer(PointerProperties(event, i));

  if (!with_history)
    return;

  const size_t history_size = event.GetHistorySize();
  for (size_t h = 0; h < history_size; ++h) {
    std::unique_ptr<MotionEventGeneric> historical_event(
        new MotionEventGeneric());
    historical_event->set_action(Action::MOVE);
    historical_event->set_event_time(event.GetHistoricalEventTime(h));
    for (size_t i = 0; i < pointer_count; ++i) {
      historical_event->PushPointer(
          PointerProperties(event.GetHistoricalX(i, h),
                            event.GetHistoricalY(i, h),
                            event.GetHistoricalTouchMajor(i, h)));
    }
    PushHistoricalEvent(std::move(historical_event));
  }
}

MotionEventGeneric& MotionEventGeneric::operator=(
    const MotionEventGeneric& other) {
  action_ = other.action_;
  event_time_ = other.event_time_;
  unique_event_id_ = other.unique_event_id_;
  action_index_ = other.action_index_;
  button_state_ = other.button_state_;
  flags_ = other.flags_;
  pointers_ = other.pointers_;
  const size_t history_size = other.GetHistorySize();
  for (size_t h = 0; h < history_size; ++h)
    PushHistoricalEvent(other.historical_events_[h]->Clone());
  return *this;
}

void MotionEventGeneric::PopPointer() {
  DCHECK_GT(pointers_.size(), 0U);
  pointers_.pop_back();
}

}  // namespace ui
