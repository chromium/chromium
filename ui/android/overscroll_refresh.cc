// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/overscroll_refresh.h"

#include <ostream>
#include <utility>

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
// release results in a small fling in the opposite direction of current active
// overscroll action (e.g. a small upward fling after pulling down to refresh,
// quite common during a slow scroll)
const float kMinFlingVelocityForActivation = -500.f;

// Minimum velocity in the active navigation direction required to force-trigger
// navigation on gesture end. According to UX, the most common scale factor
// is 1.625, so 1100 dp(fling-to-start threshold used on chrome desktop) is
// about 1788 pixel.
const float kMinFlingVelocityForForceActivation = 1788.f;

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
      edge_width_(edge_width),
      handler_(handler) {
  DCHECK(handler);
}

OverscrollRefresh::OverscrollRefresh()
    : scrolled_to_top_(true),
      scrolled_to_bottom_(false),
      overflow_y_hidden_(false),
      edge_width_(kDefaultNavigationEdgeWidth * 1.f),
      handler_(nullptr) {}

OverscrollRefresh::~OverscrollRefresh() {
}

void OverscrollRefresh::Reset() {
  scroll_state_.Reset();
  handler_->PullReset();
}

void OverscrollRefresh::OnScrollBegin(const gfx::PointF& pos) {
  scroll_begin_x_ = pos.x();
  scroll_begin_y_ = pos.y();
  top_at_scroll_start_ = scrolled_to_top_;
  bottom_at_scroll_start_ = scrolled_to_bottom_;
  ReleaseWithoutActivation();
  scroll_state_.StartAwaitingAck();
}

void OverscrollRefresh::OnScrollEnd(const gfx::Vector2dF& scroll_velocity) {
  // Reached when a user scrolls but not overscrolls
  if (!IsActive()) {
    Release(OverscrollActivationStatus::kReset);
    return;
  }
  Release(GetActivationStatus(scroll_velocity));
}

void OverscrollRefresh::OnOverscrolled(const cc::OverscrollBehavior& behavior,
                                       gfx::Vector2dF accumulated_overscroll,
                                       blink::WebGestureDevice source_device) {
  // `accumulated_overscroll` is in the opposite direction of the scroll_deltas
  // sent to the renderer.
  MaybeDisableScrollConsumption(-accumulated_overscroll);
  if (!IsAwaitingScrollUpdateAck()) {
    return;
  }
  float ydelta = -accumulated_overscroll.y();
  float xdelta = -accumulated_overscroll.x();
  bool in_y_direction = std::abs(ydelta) > std::abs(xdelta);
  bool in_x_direction = std::abs(ydelta) * kWeightAngle30 < std::abs(xdelta);
  OverscrollAction type = OverscrollAction::kNone;
  std::optional<BackGestureEventSwipeEdge> overscroll_edge;
  if (in_y_direction) {
    // Check overscroll-behavior-y and source device: pull-to-refresh should
    // only work on touchscreen overscrolls, in particular, not by touchpad or
    // mousewheel scrolls.
    if (!behavior.PropagatesYScroll() ||
        source_device != blink::WebGestureDevice::kTouchscreen) {
      Reset();
      return;
    }
    // Pull-to-refresh
    if (ydelta > 0) {
      type = OverscrollAction::kPullToRefresh;
    } else if (scrolled_to_bottom_) {  // ydelta < 0
      type = OverscrollAction::kPullFromBottomEdge;
    }
  } else if (in_x_direction) {
    DCHECK_GE(viewport_width_, 0);
    DCHECK_LE(scroll_begin_x_, viewport_width_);
    DCHECK_GE(scroll_begin_x_, 0);
    bool scroll_from_edge = scroll_begin_x_ < edge_width_ ||
                            viewport_width_ - scroll_begin_x_ < edge_width_;
    bool touchpad_swipe_to_navigate =
        (source_device == blink::WebGestureDevice::kTouchpad &&
         touchpad_overscroll_history_navigation_enabled_ &&
         base::FeatureList::IsEnabled(
             ui::kAndroidTouchpadOverscrollHistoryNavigation));
    // Check overscroll-behavior-x and other activation conditions for history
    // navigation depending on the input device:
    //   - touchscreen: iff system is not in gesture navigation mode;
    //     only activated by swipes near the horizontal edges
    //   - touchpad (possibly converted from mousewheel): iff the feature is
    //     enabled; activated by swipes everywhere
    if (!(behavior.PropagatesXScroll() &&
          ((scroll_from_edge && !is_gesture_navigation_mode_) ||
           touchpad_swipe_to_navigate))) {
      Reset();
      return;
    }
    // Swipe-to-navigate.
    type = OverscrollAction::kHistoryNavigation;
    overscroll_edge = xdelta < 0 ? BackGestureEventSwipeEdge::RIGHT
                                 : BackGestureEventSwipeEdge::LEFT;
  }

  CHECK_EQ(overscroll_edge.has_value(),
           type == OverscrollAction::kHistoryNavigation);

  if (type != OverscrollAction::kNone) {
    if (handler_->PullStart(type, overscroll_edge)) {
      scroll_state_.SetEnabled(
          ActiveAction{type, overscroll_edge, source_device});
    } else {
      scroll_state_.Reset();
    }
  }
}

void OverscrollRefresh::MaybeDisableScrollConsumption(
    const gfx::Vector2dF& scroll_delta) {
  if (IsActive() && scroll_state_.GetAction().action ==
                        OverscrollAction::kHistoryNavigation) {
    return;
  }
  // This check is meant for kPullToRefresh or kPullFromBottomEdge.
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
      scroll_state_.Reset();
    }
  }
}

bool OverscrollRefresh::WillHandleScrollUpdate(
    const gfx::Vector2dF& scroll_delta) {
  if (scroll_state_.IsDisabled()) {
    return false;
  }
  if (IsAwaitingScrollUpdateAck()) {
    MaybeDisableScrollConsumption(scroll_delta);
    return false;
  }
  if (IsActive()) {
    handler_->PullUpdate(scroll_delta.x(), scroll_delta.y());
    return true;
  }

  NOTREACHED() << "Invalid overscroll state";
}

void OverscrollRefresh::ReleaseWithoutActivation() {
  Release(OverscrollActivationStatus::kReset);
}

bool OverscrollRefresh::IsActive() const {
  return scroll_state_.IsEnabled();
}

bool OverscrollRefresh::IsAwaitingScrollUpdateAck() const {
  return scroll_state_.IsAwaitingAck();
}

void OverscrollRefresh::OnFrameUpdated(float view_width,
                                       float scrollable_viewport_height,
                                       const gfx::PointF& content_scroll_offset,
                                       const gfx::SizeF& content_size,
                                       bool root_overflow_y_hidden) {
  viewport_width_ = view_width;
  scrolled_to_top_ = content_scroll_offset.y() == 0;
  if (base::FeatureList::IsEnabled(kReportBottomOverscrolls)) {
    scrolled_to_bottom_ =
        content_size.height() <=
        content_scroll_offset.y() + scrollable_viewport_height;
  }
  overflow_y_hidden_ = root_overflow_y_hidden;
}

void OverscrollRefresh::SetTouchpadOverscrollHistoryNavigation(bool enabled) {
  touchpad_overscroll_history_navigation_enabled_ = enabled;
}

void OverscrollRefresh::SetIsGestureNavigationMode(
    bool is_gesture_navigation_mode) {
  is_gesture_navigation_mode_ = is_gesture_navigation_mode;
}

void OverscrollRefresh::Release(OverscrollActivationStatus activation_status) {
  if (scroll_state_.IsEnabled()) {
    handler_->PullRelease(activation_status);
  }
  scroll_state_.Reset();
}

float OverscrollRefresh::GetVelocityInActiveActionDirection(
    const gfx::Vector2dF& velocity) {
  const ActiveAction& active_action = scroll_state_.GetAction();
  switch (active_action.action) {
    case OverscrollAction::kPullToRefresh:
      return velocity.y();
    case OverscrollAction::kPullFromBottomEdge:
      return -velocity.y();
    case OverscrollAction::kHistoryNavigation:
      if (active_action.edge == BackGestureEventSwipeEdge::LEFT) {
        return velocity.x();
      } else {
        return -velocity.x();
      }
    default:
      NOTREACHED();
  }
}

OverscrollActivationStatus OverscrollRefresh::GetActivationStatus(
    const gfx::Vector2dF& velocity) {
  float velocity_in_direction = GetVelocityInActiveActionDirection(velocity);
  const ActiveAction& active_action = scroll_state_.GetAction();
  switch (active_action.action) {
    case OverscrollAction::kHistoryNavigation: {
      if (active_action.device == blink::WebGestureDevice::kTouchpad &&
          velocity_in_direction > kMinFlingVelocityForForceActivation) {
        return OverscrollActivationStatus::kForceActivation;
      }
      [[fallthrough]];
    }
    case OverscrollAction::kPullToRefresh:
    case OverscrollAction::kPullFromBottomEdge:
      return velocity_in_direction > kMinFlingVelocityForActivation
                 ? OverscrollActivationStatus::kAllowActivation
                 : OverscrollActivationStatus::kDisallowActivation;
    default:
      NOTREACHED();
  }
}

}  // namespace ui
