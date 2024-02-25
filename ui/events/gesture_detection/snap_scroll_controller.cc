// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/snap_scroll_controller.h"

#include <algorithm>
#include <cmath>

#include "ui/events/velocity_tracker/motion_event.h"

namespace ui {
namespace {

// Minimum ratio between initial X and Y motion to allow snapping.
const float kMinSnapRatio = 1.25f;

// Size of the snap rail relative to the initial snap bound threshold.
const float kSnapBoundToChannelMultiplier = 1.5f;

float CalculateChannelDistance(float snap_bound,
                               const gfx::SizeF& display_size) {
  const float kMinChannelDistance = snap_bound * kSnapBoundToChannelMultiplier;
  const float kMaxChannelDistance = kMinChannelDistance * 3.f;
  const float kSnapChannelDipsPerScreenDip = kMinChannelDistance / 480.f;
  if (display_size.IsEmpty())
    return kMinChannelDistance;

  float screen_size =
      std::abs(hypot(static_cast<float>(display_size.width()),
                     static_cast<float>(display_size.height())));

  float snap_channel_distance = screen_size * kSnapChannelDipsPerScreenDip;
  return std::max(kMinChannelDistance,
                  std::min(kMaxChannelDistance, snap_channel_distance));
}

}  // namespace

SnapScrollController::SnapScrollController(const gfx::SizeF& display_size)
    : display_size_(display_size) {}

void SnapScrollController::SetSnapScrollMode(
    const MotionEvent& event,
    bool is_scale_gesture_detection_in_progress,
    float slop_distance) {
  switch (event.GetAction()) {
    case MotionEvent::Action::DOWN:
      mode_ = SNAP_PENDING;
      down_position_.set_x(event.GetX());
      down_position_.set_y(event.GetY());
      break;
    case MotionEvent::Action::MOVE: {
      if (is_scale_gesture_detection_in_progress)
        break;

      if (mode_ != SNAP_PENDING)
        break;

      // Set scrolling mode to SNAP_X if scroll exceeds |slop_distance| and the
      // ratio of x movement to y movement is sufficiently large. Similarly for
      // SNAP_Y and y movement.
      float dx = std::abs(event.GetX() - down_position_.x());
      float dy = std::abs(event.GetY() - down_position_.y());
      float kMinSnapBound = slop_distance;
      float kMaxSnapBound = slop_distance * 2.f;
      if (dx * dx + dy * dy > kMinSnapBound * kMinSnapBound) {
        if (!dy || (dx / dy > kMinSnapRatio && dy < kMaxSnapBound))
          mode_ = SNAP_HORIZ;
        else if (!dx || (dy / dx > kMinSnapRatio && dx < kMaxSnapBound))
          mode_ = SNAP_VERT;
      }

      if (mode_ == SNAP_PENDING && dx > kMaxSnapBound && dy > kMaxSnapBound)
        mode_ = SNAP_NONE;
    } break;
    case MotionEvent::Action::UP:
    case MotionEvent::Action::CANCEL:
      down_position_ = gfx::PointF();
      accumulated_distance_ = gfx::Vector2dF();
      break;
    default:
      break;
  }
}

void SnapScrollController::UpdateSnapScrollMode(float distance_x,
                                                float distance_y,
                                                float slop_distance) {
  if (!IsSnappingScrolls())
    return;

  float channel_distance =
      CalculateChannelDistance(slop_distance, display_size_);
  accumulated_distance_ +=
      gfx::Vector2dF(std::abs(distance_x), std::abs(distance_y));
  if (mode_ == SNAP_HORIZ) {
    if (accumulated_distance_.y() > channel_distance) {
      mode_ = SNAP_NONE;
    } else if (accumulated_distance_.x() > channel_distance) {
      accumulated_distance_ = gfx::Vector2dF();
    }
  } else if (mode_ == SNAP_VERT) {
    if (accumulated_distance_.x() > channel_distance) {
      mode_ = SNAP_NONE;
    } else if (accumulated_distance_.y() > channel_distance) {
      accumulated_distance_ = gfx::Vector2dF();
    }
  }
}

bool SnapScrollController::IsSnapVertical() const {
  return mode_ == SNAP_VERT;
}

bool SnapScrollController::IsSnapHorizontal() const {
  return mode_ == SNAP_HORIZ;
}

bool SnapScrollController::IsSnappingScrolls() const {
  return IsSnapHorizontal() || IsSnapVertical();
}

}  // namespace ui
