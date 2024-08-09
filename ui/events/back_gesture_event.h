// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BACK_GESTURE_EVENT_H_
#define UI_EVENTS_BACK_GESTURE_EVENT_H_

#include "ui/events/events_export.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

// Indicates the edge of the screen where the gesture started.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.base
enum class BackGestureEventSwipeEdge { LEFT, RIGHT };

// This event provides information about gestures which start at the edge of a
// browser window and should trigger a back/forward navigation on the associated
// web contents. This maps closely to Android's concept of back gesture
// navigation.
class EVENTS_EXPORT BackGestureEvent {
 public:
  explicit BackGestureEvent(float progress);

  BackGestureEvent(const BackGestureEvent&) = default;
  BackGestureEvent& operator=(const BackGestureEvent&) = default;

  ~BackGestureEvent() = default;

  float progress() const { return progress_; }

 private:
  float progress_;
};

}  // namespace ui

#endif  // UI_EVENTS_BACK_GESTURE_EVENT_H_
