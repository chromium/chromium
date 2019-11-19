// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_BOUNDS_ANIMATOR_H_
#define UI_VIEWS_ANIMATION_BOUNDS_ANIMATOR_H_

#include <map>
#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/animation_container_observer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/views_export.h"

namespace gfx {
class SlideAnimation;
}

namespace views {

class BoundsAnimatorObserver;
class View;

// Bounds animator is responsible for animating the bounds of a view from the
// the views current location and size to a target position and size. To use
// BoundsAnimator invoke AnimateViewTo for the set of views you want to
// animate.
//
// BoundsAnimator internally creates an animation for each view. If you need
// a specific animation invoke SetAnimationForView after invoking AnimateViewTo.
// You can attach an AnimationDelegate to the individual animation for a view
// by way of SetAnimationDelegate. Additionally you can attach an observer to
// the BoundsAnimator that is notified when all animations are complete.
class VIEWS_EXPORT BoundsAnimator : public AnimationDelegateViews {
 public:
  explicit BoundsAnimator(View* view);
  ~BoundsAnimator() override;

  // Starts animating |view| from its current bounds to |target|. If there is
  // already an animation running for the view it's stopped and a new one
  // started. If an AnimationDelegate has been set for |view| it is removed
  // (after being notified that the animation was canceled).
  void AnimateViewTo(
      View* view,
      const gfx::Rect& target,
      std::unique_ptr<gfx::AnimationDelegate> delegate = nullptr);

  // Similar to |AnimateViewTo|, but does not reset the animation, only the
  // target bounds. If |view| is not being animated this is the same as
  // invoking |AnimateViewTo|.
  void SetTargetBounds(View* view, const gfx::Rect& target);

  // Returns the target bounds for the specified view. If |view| is not
  // animating its current bounds is returned.
  gfx::Rect GetTargetBounds(const View* view) const;

  // Sets the animation for the specified view.
  void SetAnimationForView(View* view,
                           std::unique_ptr<gfx::SlideAnimation> animation);

  // Returns the animation for the specified view. BoundsAnimator owns the
  // returned Animation.
  const gfx::SlideAnimation* GetAnimationForView(View* view);

  // Stops animating the specified view.
  void StopAnimatingView(View* view);

  // Sets the delegate for the animation for the specified view.
  void SetAnimationDelegate(View* view,
                            std::unique_ptr<gfx::AnimationDelegate> delegate);

  // Returns true if BoundsAnimator is animating the bounds of |view|.
  bool IsAnimating(View* view) const;

  // Returns true if BoundsAnimator is animating any view.
  bool IsAnimating() const;

  // Cancels all animations, leaving the views at their current location and
  // size. Any views marked for deletion are deleted.
  void Cancel();

  // Overrides default animation duration.
  void SetAnimationDuration(base::TimeDelta duration);

  // Gets the currently used animation duration.
  base::TimeDelta GetAnimationDuration() const { return animation_duration_; }

  // Sets the tween type for new animations. Default is EASE_OUT.
  void set_tween_type(gfx::Tween::Type type) { tween_type_ = type; }

  void AddObserver(BoundsAnimatorObserver* observer);
  void RemoveObserver(BoundsAnimatorObserver* observer);

  gfx::AnimationContainer* container() { return container_.get(); }

 protected:
  // Creates the animation to use for animating views.
  virtual std::unique_ptr<gfx::SlideAnimation> CreateAnimation();

 private:
  // Tracks data about the view being animated.
  struct Data {
    Data();
    Data(Data&&);
    Data& operator=(Data&&);
    ~Data();

    // The initial bounds.
    gfx::Rect start_bounds;

    // Target bounds.
    gfx::Rect target_bounds;

    // The animation.
    std::unique_ptr<gfx::SlideAnimation> animation;

    // Delegate for the animation, may be nullptr.
    std::unique_ptr<gfx::AnimationDelegate> delegate;
  };

  // Used by AnimationEndedOrCanceled.
  enum class AnimationEndType { kEnded, kCanceled };

  typedef std::map<const View*, Data> ViewToDataMap;

  typedef std::map<const gfx::Animation*, View*> AnimationToViewMap;

  // Removes references to |view| and its animation. Returns the data for the
  // caller to handle cleanup.
  Data RemoveFromMaps(View* view);

  // Does the necessary cleanup for |data|. If |send_cancel| is true and a
  // delegate has been installed on |data| AnimationCanceled is invoked on it.
  void CleanupData(bool send_cancel, Data* data);

  // Used when changing the animation for a view. This resets the maps for
  // the animation used by view and returns the current animation. Ownership
  // of the returned animation passes to the caller.
  std::unique_ptr<gfx::Animation> ResetAnimationForView(View* view);

  // Invoked from AnimationEnded and AnimationCanceled.
  void AnimationEndedOrCanceled(const gfx::Animation* animation,
                                AnimationEndType type);

  // AnimationDelegateViews overrides.
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;
  void AnimationContainerProgressed(
      gfx::AnimationContainer* container) override;
  void AnimationContainerEmpty(gfx::AnimationContainer* container) override;

  // Parent of all views being animated.
  View* parent_;

  base::ObserverList<BoundsAnimatorObserver>::Unchecked observers_;

  // All animations we create up with the same container.
  scoped_refptr<gfx::AnimationContainer> container_;

  // Maps from view being animated to info about the view.
  ViewToDataMap data_;

  // Maps from animation to view.
  AnimationToViewMap animation_to_view_;

  // As the animations we create update (AnimationProgressed is invoked) this
  // is updated. When all the animations have completed for a given tick of
  // the timer (AnimationContainerProgressed is invoked) the parent_ is asked
  // to repaint these bounds.
  gfx::Rect repaint_bounds_;

  base::TimeDelta animation_duration_ = base::TimeDelta::FromMilliseconds(200);

  gfx::Tween::Type tween_type_ = gfx::Tween::EASE_OUT;

  DISALLOW_COPY_AND_ASSIGN(BoundsAnimator);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_BOUNDS_ANIMATOR_H_
