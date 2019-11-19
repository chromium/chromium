// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/slide_out_controller.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/transform.h"
#include "ui/views/animation/slide_out_controller_delegate.h"

namespace views {

namespace {

constexpr base::TimeDelta kSwipeRestoreDuration =
    base::TimeDelta::FromMilliseconds(150);
constexpr int kSwipeOutTotalDurationMs = 150;
gfx::Tween::Type kSwipeTweenType = gfx::Tween::EASE_IN;

// When we have a swipe control, we will close the target if it is slid more
// than this amount plus the width of the swipe control.
constexpr int kSwipeCloseMargin = 64;

}  // anonymous namespace

SlideOutController::SlideOutController(ui::EventTarget* target,
                                       SlideOutControllerDelegate* delegate)
    : target_handling_(target, this), delegate_(delegate) {}

SlideOutController::~SlideOutController() {}

void SlideOutController::CaptureControlOpenState() {
  if (!has_swipe_control_)
    return;
  if (mode_ == SlideMode::kFull &&
      fabs(gesture_amount_) >= swipe_control_width_) {
    control_open_state_ = gesture_amount_ < 0
                              ? SwipeControlOpenState::kOpenOnRight
                              : SwipeControlOpenState::kOpenOnLeft;
  } else {
    control_open_state_ = SwipeControlOpenState::kClosed;
  }
}

void SlideOutController::OnGestureEvent(ui::GestureEvent* event) {
  ui::Layer* layer = delegate_->GetSlideOutLayer();
  int width = layer->bounds().width();
  float scroll_amount_for_closing_notification =
      has_swipe_control_ ? swipe_control_width_ + kSwipeCloseMargin
                         : width * 0.5;

  if (event->type() == ui::ET_SCROLL_FLING_START) {
    // The threshold for the fling velocity is computed empirically.
    // The unit is in pixels/second.
    const float kFlingThresholdForClose = 800.f;
    if (mode_ == SlideMode::kFull &&
        fabsf(event->details().velocity_x()) > kFlingThresholdForClose) {
      SlideOutAndClose(event->details().velocity_x());
      event->StopPropagation();
      return;
    }
    CaptureControlOpenState();
    RestoreVisualState();
    return;
  }

  if (!event->IsScrollGestureEvent())
    return;

  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    switch (control_open_state_) {
      case SwipeControlOpenState::kClosed:
        gesture_amount_ = 0.f;
        break;
      case SwipeControlOpenState::kOpenOnRight:
        gesture_amount_ = -swipe_control_width_;
        break;
      case SwipeControlOpenState::kOpenOnLeft:
        gesture_amount_ = swipe_control_width_;
        break;
      default:
        NOTREACHED();
    }
    delegate_->OnSlideStarted();
  } else if (event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    // The scroll-update events include the incremental scroll amount.
    gesture_amount_ += event->details().scroll_x();

    float scroll_amount;
    float opacity;
    switch (mode_) {
      case SlideMode::kFull:
        scroll_amount = gesture_amount_;
        opacity = 1.f - std::min(fabsf(scroll_amount) / width, 1.f);
        break;
      case SlideMode::kNone:
        scroll_amount = 0.f;
        opacity = 1.f;
        break;
      case SlideMode::kPartial:
        if (gesture_amount_ >= 0) {
          scroll_amount = std::min(0.5f * gesture_amount_,
                                   scroll_amount_for_closing_notification);
        } else {
          scroll_amount =
              std::max(0.5f * gesture_amount_,
                       -1.f * scroll_amount_for_closing_notification);
        }
        opacity = 1.f;
        break;
    }

    SetOpacityIfNecessary(opacity);
    gfx::Transform transform;
    transform.Translate(scroll_amount, 0.0);
    layer->SetTransform(transform);
    delegate_->OnSlideChanged(true);
  } else if (event->type() == ui::ET_GESTURE_SCROLL_END) {
    float scrolled_ratio = fabsf(gesture_amount_) / width;
    if (mode_ == SlideMode::kFull &&
        scrolled_ratio >= scroll_amount_for_closing_notification / width) {
      SlideOutAndClose(gesture_amount_);
      event->StopPropagation();
      return;
    }
    CaptureControlOpenState();
    RestoreVisualState();
  }

  event->SetHandled();
}

void SlideOutController::RestoreVisualState() {
  // Restore the layer state.
  gfx::Transform transform;
  switch (control_open_state_) {
    case SwipeControlOpenState::kClosed:
      gesture_amount_ = 0.f;
      break;
    case SwipeControlOpenState::kOpenOnRight:
      gesture_amount_ = -swipe_control_width_;
      transform.Translate(-swipe_control_width_, 0);
      break;
    case SwipeControlOpenState::kOpenOnLeft:
      gesture_amount_ = swipe_control_width_;
      transform.Translate(swipe_control_width_, 0);
      break;
  }

  SetOpacityIfNecessary(1.f);
  SetTransformWithAnimationIfNecessary(transform, kSwipeRestoreDuration);
}

void SlideOutController::SlideOutAndClose(int direction) {
  ui::Layer* layer = delegate_->GetSlideOutLayer();
  gfx::Transform transform;
  int width = layer->bounds().width();
  transform.Translate(direction < 0 ? -width : width, 0.0);

  int swipe_out_duration = kSwipeOutTotalDurationMs * opacity_;
  SetOpacityIfNecessary(0.f);
  SetTransformWithAnimationIfNecessary(
      transform, base::TimeDelta::FromMilliseconds(swipe_out_duration));
}

void SlideOutController::SetOpacityIfNecessary(float opacity) {
  if (update_opacity_)
    delegate_->GetSlideOutLayer()->SetOpacity(opacity);
  opacity_ = opacity;
}

void SlideOutController::SetTransformWithAnimationIfNecessary(
    const gfx::Transform& transform,
    base::TimeDelta animation_duration) {
  ui::Layer* layer = delegate_->GetSlideOutLayer();
  if (layer->transform() != transform) {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(animation_duration);
    settings.SetTweenType(kSwipeTweenType);
    settings.AddObserver(this);

    // An animation starts. OnImplicitAnimationsCompleted will be called just
    // after the animation finishes.
    layer->SetTransform(transform);

    // Notify slide changed with inprogress=true, since the element will slide
    // with animation. OnSlideChanged(false) will be called after animation.
    delegate_->OnSlideChanged(true);
  } else {
    // Notify slide changed after the animation finishes.
    // The argument in_progress is true if the target view is back at the
    // origin or has been gone. False if the target is visible but not at
    // the origin. False if the target is visible but not at
    // the origin.
    const bool in_progress = !layer->transform().IsIdentity();
    delegate_->OnSlideChanged(in_progress);
  }
}

void SlideOutController::OnImplicitAnimationsCompleted() {
  // Here the situation is either of:
  // 1) Notification is slided out and is about to be removed
  //      => |in_progress| is false, calling OnSlideOut
  // 2) Notification is at the origin => |in_progress| is false
  // 3) Notification is snapped to the swipe control => |in_progress| is true

  const bool is_completely_slid_out = (opacity_ == 0);
  const bool in_progress =
      !delegate_->GetSlideOutLayer()->transform().IsIdentity() &&
      !is_completely_slid_out;
  delegate_->OnSlideChanged(in_progress);

  if (!is_completely_slid_out)
    return;

  // Call SlideOutControllerDelegate::OnSlideOut() if this animation came from
  // SlideOutAndClose().

  // OnImplicitAnimationsCompleted is called from BeginMainFrame, so we should
  // delay operation that might result in deletion of LayerTreeHost.
  // https://crbug.com/895883
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SlideOutController::OnSlideOut,
                                weak_ptr_factory_.GetWeakPtr()));
}

void SlideOutController::OnSlideOut() {
  delegate_->OnSlideOut();
}

void SlideOutController::SetSwipeControlWidth(int swipe_control_width) {
  swipe_control_width_ = swipe_control_width;
  has_swipe_control_ = (swipe_control_width != 0);
}

void SlideOutController::CloseSwipeControl() {
  if (!has_swipe_control_)
    return;
  gesture_amount_ = 0;
  CaptureControlOpenState();
  RestoreVisualState();
}

}  // namespace views
