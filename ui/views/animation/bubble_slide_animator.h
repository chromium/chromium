// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_BUBBLE_SLIDE_ANIMATOR_H_
#define UI_VIEWS_ANIMATION_BUBBLE_SLIDE_ANIMATOR_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

class BubbleDialogDelegateView;
class View;

// Animates a bubble between anchor views on demand. Must be used with
// BubbleDialogDelegateView because of its reliance on the anchoring system.
class VIEWS_EXPORT BubbleSlideAnimator : public AnimationDelegateViews,
                                         public WidgetObserver {
 public:
  // Slide complete callback is called when a slide completes and the bubble is
  // safely anchored to the new view.
  using SlideCompleteCallbackSignature = void(BubbleSlideAnimator*);
  using SlideCompleteCallback =
      base::RepeatingCallback<SlideCompleteCallbackSignature>;

  // Slide progressed callback is called for each animation frame,
  // |animation_value| will be between 0 and 1 and will scale according to the
  // |tween_type| parameter.
  using SlideProgressedCallbackSignature = void(BubbleSlideAnimator*,
                                                double animation_value);
  using SlideProgressedCallback =
      base::RepeatingCallback<SlideProgressedCallbackSignature>;

  // Constructs a new BubbleSlideAnimator associated with the specified
  // |bubble_view|, which must already have a widget. If the bubble's widget is
  // destroyed, any animations will be canceled and this animator will no longer
  // be able to be used.
  explicit BubbleSlideAnimator(BubbleDialogDelegateView* bubble_view);
  BubbleSlideAnimator(const BubbleSlideAnimator&) = delete;
  BubbleSlideAnimator& operator=(const BubbleSlideAnimator&) = delete;
  ~BubbleSlideAnimator() override;

  bool is_animating() const { return slide_animation_.is_animating(); }

  // Sets the animation duration (a default is used if not set).
  void SetSlideDuration(base::TimeDelta duration);

  View* desired_anchor_view() { return desired_anchor_view_; }
  const View* desired_anchor_view() const { return desired_anchor_view_; }

  gfx::Tween::Type tween_type() const { return tween_type_; }
  void set_tween_type(gfx::Tween::Type tween_type) { tween_type_ = tween_type; }

  // Animates to a new anchor view.
  void AnimateToAnchorView(View* desired_anchor_view);

  // Ends any ongoing animation and immediately snaps the bubble to its target
  // bounds.
  void SnapToAnchorView(View* desired_anchor_view);

  // Retargets the current animation or snaps the bubble to its correct size
  // and position if there is no current animation.
  //
  // Call if the bubble contents change size in a way that would require the
  // bubble to be resized/repositioned. If you would like a new animation to
  // always play to the new bounds, call AnimateToAnchorView() instead.
  //
  // Note: This method expects the bubble to have a valid anchor view.
  void UpdateTargetBounds();

  // Stops the animation without snapping the widget to a particular anchor
  // view.
  void StopAnimation();

  // Adds a listener for slide progressed events.
  base::CallbackListSubscription AddSlideProgressedCallback(
      SlideProgressedCallback callback);

  // Adds a listener for slide complete events.
  base::CallbackListSubscription AddSlideCompleteCallback(
      SlideCompleteCallback callback);

 private:
  // AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override;

  // Determines where to animate the bubble to during an animation.
  gfx::Rect CalculateTargetBounds(const View* desired_anchor_view) const;

  const raw_ptr<BubbleDialogDelegateView, DanglingUntriaged> bubble_delegate_;
  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
  gfx::LinearAnimation slide_animation_{this};

  // The desired anchor view, which is valid during a slide animation. When not
  // animating, this value is null.
  raw_ptr<View> desired_anchor_view_ = nullptr;

  // The tween type to use when animating. The default should be aesthetically
  // pleasing for most applications.
  gfx::Tween::Type tween_type_ = gfx::Tween::FAST_OUT_SLOW_IN;

  gfx::Rect starting_bubble_bounds_;
  gfx::Rect target_bubble_bounds_;
  base::RepeatingCallbackList<SlideProgressedCallbackSignature>
      slide_progressed_callbacks_;
  base::RepeatingCallbackList<SlideCompleteCallbackSignature>
      slide_complete_callbacks_;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_BUBBLE_SLIDE_ANIMATOR_H_
