// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_FLOOD_FILL_INK_DROP_RIPPLE_H_
#define UI_VIEWS_ANIMATION_FLOOD_FILL_INK_DROP_RIPPLE_H_

#include "base/callback_list.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/views_export.h"

namespace ui {
class Layer;
}  // namespace ui

namespace views {
class CircleLayerDelegate;
class InkDropHost;

namespace test {
class FloodFillInkDropRippleTestApi;
}  // namespace test

// An ink drop ripple that starts as a small circle and flood fills a rectangle
// of the size determined by |host_size| and |clip_insets| (if provided). The
// circle is clipped to this rectangle's bounds.
// Constructors take |host_size| and |clip_insets| and calculate the effective
// bounds of the flood fill based on them. This way, the ripple's bounds are
// defined relative to the host size and can be recalculated whenever the host
// size is changed.
//
// The valid InkDropState transitions are defined below:
//
//   {All InkDropStates}      => HIDDEN
//   HIDDEN                   => ACTION_PENDING
//   HIDDEN, ACTION_PENDING   => ACTION_TRIGGERED
//   ACTION_PENDING           => ALTERNATE_ACTION_PENDING
//   ALTERNATE_ACTION_PENDING => ALTERNATE_ACTION_TRIGGERED
//   {All InkDropStates}      => ACTIVATED
//   {All InkDropStates}      => DEACTIVATED
//
class VIEWS_EXPORT FloodFillInkDropRipple : public InkDropRipple {
 public:
  FloodFillInkDropRipple(InkDropHost* ink_drop_host,
                         const gfx::Size& host_size,
                         const gfx::Insets& clip_insets,
                         const gfx::Point& center_point,
                         SkColor color,
                         float visible_opacity);
  FloodFillInkDropRipple(InkDropHost* ink_drop_host,
                         const gfx::Size& host_size,
                         const gfx::Point& center_point,
                         SkColor color,
                         float visible_opacity);
  FloodFillInkDropRipple(const FloodFillInkDropRipple&) = delete;
  FloodFillInkDropRipple& operator=(const FloodFillInkDropRipple&) = delete;
  ~FloodFillInkDropRipple() override;

  // InkDropRipple:
  ui::Layer* GetRootLayer() override;

  void set_use_hide_transform_duration_for_hide_fade_out(bool value) {
    use_hide_transform_duration_for_hide_fade_out_ = value;
  }

  void set_duration_factor(float duration_factor) {
    duration_factor_ = duration_factor;
  }

 private:
  friend class test::FloodFillInkDropRippleTestApi;

  // All the sub animations that are used to animate each of the InkDropStates.
  // These are used to get time durations with
  // GetAnimationDuration(InkDropSubAnimations). Note that in general a sub
  // animation defines the duration for either a transformation animation or an
  // opacity animation but there are some exceptions where an entire
  // InkDropState animation consists of only 1 sub animation and it defines the
  // duration for both the transformation and opacity animations.
  enum AnimationSubState {
    // HIDDEN sub animations.

    // The HIDDEN sub animation that is fading out to a hidden opacity.
    HIDDEN_FADE_OUT,

    // The HIDDEN sub animation that transform the circle to a small one.
    HIDDEN_TRANSFORM,

    // ACTION_PENDING sub animations.

    // The ACTION_PENDING sub animation that fades in to the visible opacity.
    ACTION_PENDING_FADE_IN,

    // The ACTION_PENDING sub animation that transforms the circle to fill the
    // bounds.
    ACTION_PENDING_TRANSFORM,

    // ACTION_TRIGGERED sub animations.

    // The ACTION_TRIGGERED sub animation that is fading out to a hidden
    // opacity.
    ACTION_TRIGGERED_FADE_OUT,

    // ALTERNATE_ACTION_PENDING sub animations.

    // The ALTERNATE_ACTION_PENDING animation has only one sub animation which
    // animates
    // the circleto fill the bounds at visible opacity.
    ALTERNATE_ACTION_PENDING,

    // ALTERNATE_ACTION_TRIGGERED sub animations.

    // The ALTERNATE_ACTION_TRIGGERED sub animation that is fading out to a
    // hidden opacity.
    ALTERNATE_ACTION_TRIGGERED_FADE_OUT,

    // ACTIVATED sub animations.

    // The ACTIVATED sub animation that is fading in to the visible opacity.
    ACTIVATED_FADE_IN,

    // The ACTIVATED sub animation that transforms the circle to fill the entire
    // bounds.
    ACTIVATED_TRANSFORM,

    // DEACTIVATED sub animations.

    // The DEACTIVATED sub animation that is fading out to a hidden opacity.
    DEACTIVATED_FADE_OUT,
  };

  // InkDropRipple:
  void AnimateStateChange(InkDropState old_ink_drop_state,
                          InkDropState new_ink_drop_state) override;
  void SetStateToActivated() override;
  void SetStateToHidden() override;
  void AbortAllAnimations() override;

  // Sets the opacity of the ink drop. Note that this does not perform any
  // animation.
  void SetOpacity(float opacity);

  // Returns the Transform to be applied to the |painted_layer_| for the given
  // |target_radius|.
  gfx::Transform CalculateTransform(float target_radius) const;

  // Returns the target Transform for when the ink drop is fully shown.
  gfx::Transform GetMaxSizeTargetTransform() const;

  // Returns the largest distance from |point| to the corners of the
  // |root_layer_| bounds.
  float MaxDistanceToCorners(const gfx::Point& point) const;

  // Returns the InkDropState sub animation duration for the given |state|.
  base::TimeDelta GetAnimationDuration(AnimationSubState state);

  // Called from LayerAnimator when a new LayerAnimationSequence is scheduled
  // which allows for assigning the observer to the sequence.
  void OnLayerAnimationSequenceScheduled(ui::LayerAnimationSequence* sequence);

  // Insets of the clip area relative to the host bounds.
  gfx::Insets clip_insets_;

  // The point where the Center of the ink drop's circle should be drawn.
  gfx::Point center_point_;

  // Ink drop opacity when it is visible.
  float visible_opacity_;

  // Whether the fade out animation to hidden state should have the same
  // duration as the associated scale transform animation.
  bool use_hide_transform_duration_for_hide_fade_out_;

  // The factor used to scale down/up animation duration.
  float duration_factor_;

  // The root layer that parents the animating layer. The root layer is used to
  // manipulate opacity and clipping bounds, and it child is used to manipulate
  // the different shape of the ink drop.
  ui::Layer root_layer_;

  // Sequence scheduled callback subscription for the root layer.
  base::CallbackListSubscription root_callback_subscription_;

  // ui::LayerDelegate to paint the |painted_layer_|.
  CircleLayerDelegate circle_layer_delegate_;

  // Child ui::Layer of |root_layer_|. Used to  manipulate the different size
  // and shape of the ink drop.
  ui::Layer painted_layer_;

  // Sequence scheduled callback subscriptions for the painted layer.
  base::CallbackListSubscription painted_layer_callback_subscription_;

  // The current ink drop state.
  InkDropState ink_drop_state_ = InkDropState::HIDDEN;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_FLOOD_FILL_INK_DROP_RIPPLE_H_
