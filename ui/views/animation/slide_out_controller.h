// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_SLIDE_OUT_CONTROLLER_H_
#define UI_VIEWS_ANIMATION_SLIDE_OUT_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/events/scoped_target_handler.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

class SlideOutControllerDelegate;

// This class contains logic to control sliding out of a layer in response to
// swipes, i.e. gesture scroll events.
class VIEWS_EXPORT SlideOutController : public ui::EventHandler,
                                        public ui::ImplicitAnimationObserver {
 public:
  // Indicates how much the target layer is allowed to slide.
  enum class SlideMode {
    kFull,
    kPartial,
    kNone,
  };

  SlideOutController(ui::EventTarget* target,
                     SlideOutControllerDelegate* delegate);
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
  // Effective only when swipe control is enabled by |SetSwipeControlWidth()|.
  void CloseSwipeControl();

  // Slides the view out and closes it after the animation. The sign of
  // |direction| indicates which way the slide occurs.
  void SlideOutAndClose(int direction);

 private:
  // Positions where the slided view stays after the touch released.
  enum class SwipeControlOpenState { kClosed, kOpenOnLeft, kOpenOnRight };

  // Restores the transform and opacity of the view.
  void RestoreVisualState();

  // Decides which position the slide should go back after touch is released.
  void CaptureControlOpenState();

  // Sets the opacity of the slide out layer if |update_opacity_| is true.
  void SetOpacityIfNecessary(float opacity);

  // Sets the transform matrix and performs animation if the matrix is changed.
  void SetTransformWithAnimationIfNecessary(const gfx::Transform& transform,
                                            base::TimeDelta animation_duration);

  // Called asynchronously after the slide out animation completes to inform the
  // delegate.
  void OnSlideOut();

  ui::ScopedTargetHandler target_handling_;

  // Unowned and outlives this object.
  SlideOutControllerDelegate* delegate_;

  // Cumulative scroll amount since the beginning of current slide gesture.
  // Includes the initial shift when swipe control was open at gesture start.
  float gesture_amount_ = 0.f;

  // Whether or not this view can be slided and/or swiped out.
  SlideMode mode_ = SlideMode::kFull;

  // Whether the swipe control is enabled. See |SetSwipeControlWidth()|.
  // Effective only when |mode_| is FULL.
  bool has_swipe_control_ = false;

  // The horizontal position offset to for swipe control.
  // See |SetSwipeControlWidth()|.
  int swipe_control_width_ = 0;

  // The position where the slided view stays after the touch released.
  // Changed only when |mode_| is FULL and |has_swipe_control_| is true.
  SwipeControlOpenState control_open_state_ = SwipeControlOpenState::kClosed;

  // If false, it doesn't update the opacity.
  bool update_opacity_ = true;

  // Last opacity set by SetOpacityIfNecessary.
  float opacity_ = 1.0;

  base::WeakPtrFactory<SlideOutController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SlideOutController);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_SLIDE_OUT_CONTROLLER_H_
