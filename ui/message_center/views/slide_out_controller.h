// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_SLIDE_OUT_CONTROLLER_H_
#define UI_MESSAGE_CENTER_VIEWS_SLIDE_OUT_CONTROLLER_H_

#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/events/scoped_target_handler.h"
#include "ui/message_center/message_center_export.h"
#include "ui/views/view.h"

namespace message_center {

// This class contains logic to control sliding out of a layer in response to
// swipes, i.e. gesture scroll events.
class MESSAGE_CENTER_EXPORT SlideOutController
    : public ui::EventHandler,
      public ui::ImplicitAnimationObserver {
 public:
  enum class SlideMode {
    FULL,
    PARTIALLY,
    NO_SLIDE,
  };

  class Delegate {
   public:
    // Returns the layer for slide operations.
    virtual ui::Layer* GetSlideOutLayer() = 0;

    // Called when a slide starts, ends, or is updated.
    // The argument is true if the slide starts or in progress, false if it
    // ends.
    virtual void OnSlideChanged(bool in_progress) = 0;

    // Called when user intends to close the View by sliding it out.
    virtual void OnSlideOut() = 0;
  };

  SlideOutController(ui::EventTarget* target, Delegate* delegate);
  ~SlideOutController() override;

  void set_update_opacity(bool update_opacity) {
    update_opacity_ = update_opacity;
  }
  void set_slide_mode(SlideMode mode) {
    // TODO(yoshiki): Close the slide when the slide mode sets to NO_SLIDE.
    mode_ = mode;
  }
  float gesture_amount() const { return gesture_amount_; }
  SlideMode mode() const { return mode_; }

  // ui::EventHandler
  void OnGestureEvent(ui::GestureEvent* event) override;

  // ui::ImplicitAnimationObserver
  void OnImplicitAnimationsCompleted() override;

  // Enables the swipe control with specifying the width of buttons. Buttons
  // will appear behind the view as user slides it partially and it's kept open
  // after the gesture.
  void SetSwipeControlWidth(int swipe_control_width);
  float GetGestureAmount() const { return gesture_amount_; }

  // Moves slide back to the center position to closes the swipe control.
  // Effective only when swipe control is enabled by EnableSwipeControl().
  void CloseSwipeControl();

 private:
  // Positions where the slided view stays after the touch released.
  enum class SwipeControlOpenState { CLOSED, OPEN_ON_LEFT, OPEN_ON_RIGHT };

  // Restores the transform and opacity of the view.
  void RestoreVisualState();

  // Decides which position the slide should go back after touch is released.
  void CaptureControlOpenState();

  // Slides the view out and closes it after the animation. The sign of
  // |direction| indicates which way the slide occurs.
  void SlideOutAndClose(int direction);

  // Sets the opacity of the slide out layer if |update_opacity_| is true.
  void SetOpacityIfNecessary(float opacity);

  ui::ScopedTargetHandler target_handling_;
  Delegate* delegate_;

  // Cumulative scroll amount since the beginning of current slide gesture.
  // Includes the initial shift when swipe control was open at gesture start.
  float gesture_amount_ = 0.f;

  // Whether or not this view can be slided and/or swiped out.
  SlideMode mode_ = SlideMode::FULL;

  // Whether the swipe control is enabled. See EnableSwipeControl().
  // Effective only when |mode_| is FULL.
  bool has_swipe_control_ = false;

  // The horizontal position offset to for swipe control.
  // See |EnableSwipeControl|.
  int swipe_control_width_ = 0;

  // The position where the slided view stays after the touch released.
  // Changed only when |mode_| is FULL and |has_swipe_control_| is true.
  SwipeControlOpenState control_open_state_ = SwipeControlOpenState::CLOSED;

  // If false, it doesn't update the opacity.
  bool update_opacity_ = true;

  // Last opacity set by SetOpacityIfNecessary.
  float opacity_ = 1.0;

  DISALLOW_COPY_AND_ASSIGN(SlideOutController);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_SLIDE_OUT_CONTROLLER_H_
