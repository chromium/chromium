// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_SNAP_SCROLL_CONTROLLER_H_
#define UI_EVENTS_GESTURE_DETECTION_SNAP_SCROLL_CONTROLLER_H_

#include "ui/events/gesture_detection/gesture_detection_export.h"
#include "ui/events/velocity_tracker/motion_event.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ui {

// Port of SnapScrollController.java from Chromium
// Controls the scroll snapping behavior based on scroll updates.
class GESTURE_DETECTION_EXPORT SnapScrollController {
 public:
  explicit SnapScrollController(const gfx::SizeF& display_size);

  SnapScrollController(const SnapScrollController&) = delete;
  SnapScrollController& operator=(const SnapScrollController&) = delete;

  ~SnapScrollController() = default;

  // Sets the snap scroll mode based on the event type.
  void SetSnapScrollMode(const MotionEvent& event,
                         bool is_scale_gesture_detection_in_progress,
                         float slop_distance);

  // Updates the snap scroll mode based on the given X and Y distance to be
  // moved on scroll.  If the scroll update is above a threshold, the snapping
  // behavior is reset.
  void UpdateSnapScrollMode(float distance_x,
                            float distance_y,
                            float slop_distance);

  bool IsSnapVertical() const;
  bool IsSnapHorizontal() const;
  bool IsSnappingScrolls() const;

 private:
  enum SnapMode { SNAP_NONE, SNAP_PENDING, SNAP_HORIZ, SNAP_VERT };

  gfx::SizeF display_size_;
  SnapMode mode_ = SNAP_NONE;
  gfx::PointF down_position_;
  gfx::Vector2dF accumulated_distance_;
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_SNAP_SCROLL_CONTROLLER_H_
