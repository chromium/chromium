// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_VELOCITY_TRACKER_MOTION_EVENT_GENERIC_H_
#define UI_EVENTS_VELOCITY_TRACKER_MOTION_EVENT_GENERIC_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "ui/events/velocity_tracker/motion_event.h"

namespace ui {

struct COMPONENT_EXPORT(VELOCITY_TRACKER) PointerProperties {
  PointerProperties();
  PointerProperties(float x, float y, float touch_major);
  PointerProperties(const MotionEvent& event, size_t pointer_index);
  PointerProperties(const PointerProperties& other);
  PointerProperties& operator=(const PointerProperties& other);

  // Sets |touch_major|, |touch_minor|, and |orientation| from the given radius
  // and rotation angle (in degrees).
  void SetAxesAndOrientation(float radius_x,
                             float radius_y,
                             float rotation_angle_degree);

  int id;
  MotionEvent::ToolType tool_type;
  float x;
  float y;
  float raw_x;
  float raw_y;
  float pressure;
  float touch_major;
  float touch_minor;
  float orientation;
  float tilt_x;
  float tilt_y;
  float twist;
  float tangential_pressure;
  // source_device_id is only used on Aura.
  int source_device_id;
};

// A generic MotionEvent implementation.
class COMPONENT_EXPORT(VELOCITY_TRACKER) MotionEventGeneric
    : public MotionEvent {
 public:
  MotionEventGeneric(Action action,
                     base::TimeTicks event_time,
                     const PointerProperties& pointer);
  MotionEventGeneric(const MotionEventGeneric& other);

  ~MotionEventGeneric() override;

  // MotionEvent implementation.
  uint32_t GetUniqueEventId() const override;
  Action GetAction() const override;
  int GetActionIndex() const override;
  size_t GetPointerCount() const override;
  int GetPointerId(size_t pointer_index) const override;
  float GetX(size_t pointer_index) const override;
  float GetY(size_t pointer_index) const override;
  float GetRawX(size_t pointer_index) const override;
  float GetRawY(size_t pointer_index) const override;
  float GetTouchMajor(size_t pointer_index) const override;
  float GetTouchMinor(size_t pointer_index) const override;
  float GetOrientation(size_t pointer_index) const override;
  float GetPressure(size_t pointer_index) const override;
  float GetTiltX(size_t pointer_index) const override;
  float GetTiltY(size_t pointer_index) const override;
  float GetTwist(size_t pointer_index) const override;
  float GetTangentialPressure(size_t pointer_index) const override;
  ToolType GetToolType(size_t pointer_index) const override;
  int GetButtonState() const override;
  int GetFlags() const override;
  base::TimeTicks GetEventTime() const override;
  size_t GetHistorySize() const override;
  base::TimeTicks GetHistoricalEventTime(
      size_t historical_index) const override;
  float GetHistoricalTouchMajor(size_t pointer_index,
                                size_t historical_index) const override;
  float GetHistoricalX(size_t pointer_index,
                       size_t historical_index) const override;
  float GetHistoricalY(size_t pointer_index,
                       size_t historical_index) const override;

  int32_t GetSourceDeviceId(size_t pointer_index) const override;

  // Adds |pointer| to the set of pointers returning the index it was added at.
  size_t PushPointer(const PointerProperties& pointer);

  // Removes the PointerProperties at |index|.
  void RemovePointerAt(size_t index);

  PointerProperties& pointer(size_t index) { return pointers_[index]; }
  const PointerProperties& pointer(size_t index) const {
    return pointers_[index];
  }

  // Add an event to the history. |this| and |event| must have the same pointer
  // count and must both have an action of ACTION_MOVE.
  void PushHistoricalEvent(std::unique_ptr<MotionEvent> event);

  void set_action(Action action) { action_ = action; }
  void set_event_time(base::TimeTicks event_time) { event_time_ = event_time; }
  void set_unique_event_id(uint32_t unique_event_id) {
    unique_event_id_ = unique_event_id;
  }
  void set_action_index(int action_index) { action_index_ = action_index; }
  void set_button_state(int button_state) { button_state_ = button_state; }
  void set_flags(int flags) { flags_ = flags; }

  static std::unique_ptr<MotionEventGeneric> CloneEvent(
      const MotionEvent& event);
  static std::unique_ptr<MotionEventGeneric> CancelEvent(
      const MotionEvent& event);

 protected:
  MotionEventGeneric();
  MotionEventGeneric(const MotionEvent& event, bool with_history);
  MotionEventGeneric& operator=(const MotionEventGeneric& other);

  void PopPointer();

 private:
  enum { kTypicalMaxPointerCount = 5 };

  Action action_;
  base::TimeTicks event_time_;
  uint32_t unique_event_id_;
  int action_index_;
  int button_state_;
  int flags_;
  absl::InlinedVector<PointerProperties, kTypicalMaxPointerCount> pointers_;
  std::vector<std::unique_ptr<MotionEvent>> historical_events_;
};

}  // namespace ui

#endif  // UI_EVENTS_VELOCITY_TRACKER_MOTION_EVENT_GENERIC_H_
