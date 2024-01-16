// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_MOTION_EVENT_TEST_UTILS_H_
#define UI_EVENTS_TEST_MOTION_EVENT_TEST_UTILS_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/time/time.h"
#include "ui/events/velocity_tracker/motion_event_generic.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {
namespace test {

class MockMotionEvent : public MotionEventGeneric {
 public:
  enum { TOUCH_MAJOR = 10 };

  MockMotionEvent();
  explicit MockMotionEvent(Action action);
  MockMotionEvent(Action action, base::TimeTicks time, float x, float y);
  MockMotionEvent(Action action,
                  base::TimeTicks time,
                  float x0,
                  float y0,
                  float x1,
                  float y1);
  MockMotionEvent(Action action,
                  base::TimeTicks time,
                  float x0,
                  float y0,
                  float x1,
                  float y1,
                  float x2,
                  float y2);
  MockMotionEvent(Action action,
                  base::TimeTicks time,
                  const std::vector<gfx::PointF>& positions);
  MockMotionEvent(const MockMotionEvent& other);

  MotionEvent::Classification GetClassification() const override;

  void SetClassification(MotionEvent::Classification classification) {
    gesture_classification_ = classification;
  }

  ~MockMotionEvent() override;

  // Utility methods.
  MockMotionEvent& PressPoint(float x, float y);
  MockMotionEvent& MovePoint(size_t index, float x, float y);
  MockMotionEvent& ReleasePoint();
  MockMotionEvent& ReleasePointAtIndex(size_t index);
  MockMotionEvent& CancelPoint();
  MockMotionEvent& SetTouchMajor(float new_touch_major);
  MockMotionEvent& SetRawOffset(float raw_offset_x, float raw_offset_y);
  MockMotionEvent& SetToolType(size_t index, ToolType tool_type);
  MockMotionEvent& SetPrimaryPointerId(int id);

 private:
  void PushPointer(float x, float y);
  void UpdatePointersAndID();

  MotionEvent::Classification gesture_classification_ =
      MotionEvent::Classification::NONE;
};

std::string ToString(const MotionEvent& event);

}  // namespace test
}  // namespace ui

#endif  // UI_EVENTS_TEST_MOTION_EVENT_TEST_UTILS_H_
