// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/overscroll_refresh.h"

#include <ostream>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "cc/input/overscroll_behavior.h"
#include "ui/android/overscroll_refresh_handler.h"
#include "ui/android/ui_android_features.h"
#include "ui/events/back_gesture_event.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {
namespace {

// Experimentally determined constant used to allow activation even if touch
// release results in a small upward fling (quite common during a slow scroll).
const float kMinFlingVelocityForActivation = -500.f;

// Weighted value used to determine whether a scroll should trigger vertical
// scroll or horizontal navigation.
const float kWeightAngle30 = 1.73f;

}  // namespace

OverscrollRefresh::OverscrollRefresh(OverscrollRefreshHandler* handler,
                                     float edge_width)
    : scrolled_to_top_(true),
      scrolled_to_bottom_(false),
      top_at_scroll_start_(true),
      bottom_at_scroll_start_(false),
      overflow_y_hidden_(false),
      scroll_consumption_state_(DISABLED),
      edge_width_(edge_width),
      handler_(handler) {
  DCHECK(handler);
}

OverscrollRefresh::OverscrollRefresh()
    : scrolled_to_top_(true),
      scrolled_to_bottom_(false),
      overflow_y_hidden_(false),
      scroll_consumption_state_(DISABLED),
      edge_width_(kDefaultNavigationEdgeWidth * 1.f),
      handler_(nullptr) {}

OverscrollRefresh::~OverscrollRefresh() {
}

void OverscrollRefresh::Reset() {
  scroll_consumption_state_ = DISABLED;
  cumulative_scroll_.set_x(0);
  cumulative_scroll_.set_y(0);
  handler_->PullReset();
}

void OverscrollRefresh::OnScrollBegin(const gfx::PointF& pos) {
  scroll_begin_x_ = pos.x();
  scroll_begin_y_ = pos.y();
  top_at_scroll_start_ = scrolled_to_top_;
  bottom_at_scroll_start_ = scrolled_to_bottom_;
  ReleaseWithoutActivation();
  scroll_consumption_state_ = AWAITING_SCROLL_UPDATE_ACK;
}

void OverscrollRefresh::OnScrollEnd(const gfx::Vector2dF& scroll_velocity) {
  bool allow_activation = scroll_velocity.y() > kMinFlingVelocityForActivation;
  Release(allow_activation);
}

void OverscrollRefresh::OnOverscrolled(const cc::OverscrollBehavior& behavior) {
  if (scroll_consumption_state_ != AWAITING_SCROLL_UPDATE_ACK)
    return;

  float ydelta = cumulative_scroll_.y();
  float xdelta = cumulative_scroll_.x();
  bool in_y_direction = std::abs(ydelta) > std::abs(xdelta);
  bool in_x_direction = std::abs(ydelta) * kWeightAngle30 < std::abs(xdelta);
  OverscrollAction type = OverscrollAction::NONE;
  std::optional<BackGestureEventSwipeEdge> overscroll_edge;
  if (in_y_direction) {
    if (behavior.y != cc::OverscrollBehavior::Type::kAuto) {
      Reset();
      return;
    }
    // Pull-to-refresh. Check overscroll-behavior-y
    if (ydelta > 0) {
      type = OverscrollAction::PULL_TO_REFRESH;
    } else if (scrolled_to_bottom_) {  // ydelta < 0
      type = OverscrollAction::PULL_FROM_BOTTOM_EDGE;
    }
  } else if (in_x_direction &&
             (scroll_begin_x_ < edge_width_ ||
              viewport_width_ - scroll_begin_x_ < edge_width_)) {
    // Swipe-to-navigate. Check overscroll-behavior-x
    if (behavior.x != cc::OverscrollBehavior::Type::kAuto) {
      Reset();
      return;
    }
    type = OverscrollAction::HISTORY_NAVIGATION;
    overscroll_edge = xdelta < 0 ? BackGestureEventSwipeEdge::RIGHT
                                 : BackGestureEventSwipeEdge::LEFT;
  }

  CHECK_EQ(overscroll_edge.has_value(),
           type == OverscrollAction::HISTORY_NAVIGATION);

  if (type != OverscrollAction::NONE) {
    scroll_consumption_state_ =
        handler_->PullStart(type, overscroll_edge) ? ENABLED : DISABLED;
  }
}

bool OverscrollRefresh::WillHandleScrollUpdate(
    const gfx::Vector2dF& scroll_delta) {
  switch (scroll_consumption_state_) {
    case DISABLED:
      return false;

    case AWAITING_SCROLL_UPDATE_ACK:
      if (std::abs(scroll_delta.y()) > std::abs(scroll_delta.x())) {
        // Check applies for the pull-to-refresh.
        bool is_pull_to_refresh = scroll_delta.y() > 0 && top_at_scroll_start_;
        // Check applies for the pull-from-bottom-edge.
        bool is_pull_from_bottom_edge = scroll_delta.y() < 0 &&
                                        bottom_at_scroll_start_ &&
                                        !top_at_scroll_start_;

        // If the activation shouldn't have happened, stop here.
        if (overflow_y_hidden_ ||
            (!is_pull_to_refresh && !is_pull_from_bottom_edge)) {
          scroll_consumption_state_ = DISABLED;
          return false;
        }
      }
      cumulative_scroll_.Add(scroll_delta);
      return false;

    case ENABLED:
      handler_->PullUpdate(scroll_delta.x(), scroll_delta.y());
      return true;
  }

  NOTREACHED_IN_MIGRATION()
      << "Invalid overscroll state: " << scroll_consumption_state_;
  return false;
}

void OverscrollRefresh::ReleaseWithoutActivation() {
  bool allow_activation = false;
  Release(allow_activation);
}

bool OverscrollRefresh::IsActive() const {
  return scroll_consumption_state_ == ENABLED;
}

bool OverscrollRefresh::IsAwaitingScrollUpdateAck() const {
  return scroll_consumption_state_ == AWAITING_SCROLL_UPDATE_ACK;
}

void OverscrollRefresh::OnFrameUpdated(const gfx::SizeF& viewport_size,
                                       const gfx::PointF& content_scroll_offset,
                                       const gfx::SizeF& content_size,
                                       bool root_overflow_y_hidden) {
  viewport_width_ = viewport_size.width();
  scrolled_to_top_ = content_scroll_offset.y() == 0;
  if (base::FeatureList::IsEnabled(kReportBottomOverscrolls)) {
    scrolled_to_bottom_ = content_size.height() <=
                          content_scroll_offset.y() + viewport_size.height();
  }
  overflow_y_hidden_ = root_overflow_y_hidden;
}

void OverscrollRefresh::Release(bool allow_refresh) {
  if (scroll_consumption_state_ == ENABLED)
    handler_->PullRelease(allow_refresh);
  scroll_consumption_state_ = DISABLED;
  cumulative_scroll_.set_x(0);
  cumulative_scroll_.set_y(0);
}

}  // namespace ui
