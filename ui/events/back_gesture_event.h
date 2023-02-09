// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BACK_GESTURE_EVENT_H_
#define UI_EVENTS_BACK_GESTURE_EVENT_H_

#include "ui/events/events_export.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace gfx {
class PointF;
}

namespace ui {

// Indicates the edge of the screen where the gesture started.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.base
enum BackGestureEventSwipeEdge { LEFT, RIGHT };

// This event provides information about gestures which start at the edge of a
// browser window and should trigger a back/forward navigation on the associated
// web contents. This maps closely to Android's concept of back gesture
// navigation.
class EVENTS_EXPORT BackGestureEvent {
 public:
  BackGestureEvent(const gfx::PointF& location,
                   float progress,
                   BackGestureEventSwipeEdge edge);

  BackGestureEvent(const BackGestureEvent&) = delete;
  BackGestureEvent& operator=(const BackGestureEvent&) = delete;

  ~BackGestureEvent() = default;

  const gfx::PointF& location() const { return location_; }
  float progress() const { return progress_; }
  BackGestureEventSwipeEdge edge() const { return edge_; }

 private:
  gfx::PointF location_;
  float progress_;
  BackGestureEventSwipeEdge edge_;
};

}  // namespace ui

#endif  // UI_EVENTS_BACK_GESTURE_EVENT_H_
