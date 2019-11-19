// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_ANIMATING_LAYOUT_MANAGER_H_
#define UI_VIEWS_LAYOUT_ANIMATING_LAYOUT_MANAGER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/views_export.h"

namespace gfx {
class AnimationContainer;
}  // namespace gfx

namespace views {

// Layout manager which explicitly animates its child views and/or its preferred
// size when the target layout changes (the target layout being provided by a
// separate, non-animating layout manager; typically a FlexLayout or
// InterpolatingLayoutManager).
//
// For example, consider a view in which multiple buttons can be displayed
// depending on context, in a horizontal row. When we add a button, we want all
// the buttons to the left to slide over and the new button to appear in the
// gap:
//     | [a] [b] [c] |
//    | [a] [b]  [c] |
//   | [a] [b] . [c] |
//  | [a] [b] .. [c] |
// | [a] [b] [x] [c] |
//
// Without AnimatingLayoutManager you would have to explicitly animate the
// bounds of the host view and the layout elements each frame, calculating which
// go where. With AnimatingLayout you create a single declarative layout for the
// whole thing and just insert the button where you want it. Here's the setup:
//
//   auto* animating_layout = button_container->SetLayoutManager(
//       std::make_unique<AnimatingLayoutManager>());
//   animating_layout->SetShouldAnimateBounds(true);
//   auto* flex_layout = animating_layout->SetTargetLayoutManager(
//       std::make_unique<FlexLayout>());
//   flex_layout->SetOrientation(LayoutOrientation::kHorizontal)
//              .SetCollapseMargins(true)
//              .SetDefault(kMarginsKey, gfx::Insets(5));
//
// Now, when you want to add (or remove) a button and animate, you just call:
//
//   button_container->AddChildViewAt(button, position);
//   button_container->RemoveChildView(button);
//
// The bounds of |button_container| will then animate appropriately and |button|
// will either appear or disappear in the appropriate location.
//
// Note that under normal operation, any changes made to the host view before
// being added to a widget will not result in animation. If initial setup of the
// host view happens after being added to a widget, you can call ResetLayout()
// to prevent changes made during setup from animating.
class VIEWS_EXPORT AnimatingLayoutManager : public LayoutManagerBase {
 public:
  class VIEWS_EXPORT Observer : public base::CheckedObserver {
   public:
    virtual void OnLayoutIsAnimatingChanged(AnimatingLayoutManager* source,
                                            bool is_animating) = 0;
  };

  // Describes how a view which is appearing or disappearing during an animation
  // behaves. Child views which are removed from the parent view always simply
  // disappear; use one of the Fade methods below to cause a view to fade out.
  //
  // TODO(dfried): break this out into layout_types and make a view class
  // property so that it can be set separately on each child view.
  enum class FadeInOutMode {
    // Default behavior: a view fading in or out is hidden during the animation.
    kHide,
    // A view fading in or out shrinks to or from nothing.
    kScaleFromZero,
    // A view fading in or out appears or disappears when it hits its minimum
    // size, and scales the rest of the way in or out.
    kScaleFromMinimum,
    // A view fading in will slide out from under the view on its leading edge;
    // if no view is present a suitable substitute fade is chosen.
    kSlideFromLeadingEdge,
    // A view fading in will slide out from under the view on its trailing edge;
    // if no view is present a suitable substitute fade is chosen.
    kSlideFromTrailingEdge,
  };

  // Call QueueDelayedAction() to queue up an action to be performed when the
  // current animation ends.
  using DelayedAction = base::OnceCallback<void()>;

  AnimatingLayoutManager();
  ~AnimatingLayoutManager() override;

  bool should_animate_bounds() const { return should_animate_bounds_; }
  AnimatingLayoutManager& SetShouldAnimateBounds(bool should_animate_bounds);

  base::TimeDelta animation_duration() const { return animation_duration_; }
  AnimatingLayoutManager& SetAnimationDuration(
      base::TimeDelta animation_duration);

  gfx::Tween::Type tween_type() const { return tween_type_; }
  AnimatingLayoutManager& SetTweenType(gfx::Tween::Type tween_type);

  LayoutOrientation orientation() const { return orientation_; }
  AnimatingLayoutManager& SetOrientation(LayoutOrientation orientation);

  FadeInOutMode default_fade_mode() const { return default_fade_mode_; }
  AnimatingLayoutManager& SetDefaultFadeMode(FadeInOutMode default_fade_mode);

  bool is_animating() const { return is_animating_; }

  // Sets the owned (non-animating) layout manager which defines the target
  // layout that will be animated to when it changes. This layout manager can
  // only be set once.
  template <class T>
  T* SetTargetLayoutManager(std::unique_ptr<T> layout_manager) {
    DCHECK_EQ(0U, num_owned_layouts());
    T* const result = AddOwnedLayout(std::move(layout_manager));
    ResetLayout();
    InvalidateHost(false);
    return result;
  }

  // Clears any previous layout, stops any animation, and re-loads the proposed
  // layout from the embedded layout manager.
  void ResetLayout();

  // Does the work of ResetLayout() or FreezeLayout(), with the resulting layout
  // snapped to |target_size|.
  void ResetLayoutToSize(const gfx::Size& target_size);

  // Cleans up after an animation, runs delayed actions, and sends
  // notifications.
  void OnAnimationEnded();

  // Causes the specified child view to fade out and become hidden. Alternative
  // to directly hiding the view (which will have the same effect, but could
  // cause visual flicker if the view paints before it can re-layout.
  void FadeOut(View* child_view);

  // Causes the specified child view to fade in and become visible. Alternative
  // to directly showing the view (which will have the same effect, but could
  // cause visual flicker if the view paints before it can re-layout.
  void FadeIn(View* child_view);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer) const;

  // LayoutManagerBase:
  gfx::Size GetPreferredSize(const View* host) const override;
  gfx::Size GetMinimumSize(const View* host) const override;
  int GetPreferredHeightForWidth(const View* host, int width) const override;
  std::vector<View*> GetChildViewsInPaintOrder(const View* host) const override;

  // Queues an action to take place after the current animation completes.
  // Must be called during an animation. If |delayed_action| needs access to
  // external resources, views, etc. then it must check that those resources are
  // still available and valid when it is run.
  void QueueDelayedAction(DelayedAction delayed_action);

  // Identical to QueueDelayedAction() except that if the layout is not
  // animating the action is run immediately.
  void RunOrQueueAction(DelayedAction action);

  // Returns the animation container being used by the layout manager, creating
  // one if one has not yet been created. Implicitly enables animation on this
  // layout, so you do not need to also call EnableAnimationForTesting().
  gfx::AnimationContainer* GetAnimationContainerForTesting();

  // Enables animation of this layout even if the host view does not yet meet
  // the normal requirements for being able to animate (e.g. being added to a
  // widget).
  void EnableAnimationForTesting();

 protected:
  // LayoutManagerBase:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override;
  void OnInstalled(View* host) override;
  void OnLayoutChanged() override;
  void LayoutImpl() override;

 private:
  struct LayoutFadeInfo;
  class AnimationDelegate;
  friend class AnimationDelegate;

  LayoutManagerBase* target_layout_manager() {
    return num_owned_layouts() ? owned_layout(0) : nullptr;
  }
  const LayoutManagerBase* target_layout_manager() const {
    return num_owned_layouts() ? owned_layout(0) : nullptr;
  }

  // Calculates the new target layout and returns true if it has changed.
  bool RecalculateTarget();

  // Called by the animation logic every time a new frame happens.
  void AnimateTo(double value);

  // Notifies all observers that the animation state has changed.
  void NotifyIsAnimatingChanged();

  // Runs all delayed actions. See QueueDelayedAction() for more information.
  void RunDelayedActions();

  // Updates the current layout to |percent| interpolated between the starting
  // and target layouts.
  void UpdateCurrentLayout(double percent);

  // Updates information about which views are fading in or out during the
  // current animation.
  void CalculateFadeInfos();

  // Calculates a kScaleFrom[Minimum|Zero] fade and returns the resulting child
  // layout info.
  ChildLayout CalculateScaleFade(const LayoutFadeInfo& fade_info,
                                 base::Optional<size_t> prev_index,
                                 base::Optional<size_t> next_index,
                                 double scale_percent,
                                 bool scale_from_zero) const;

  // Calculates a kSlideFrom[Leading|Trailing]Edge fade and returns the
  // resulting child layout info.
  ChildLayout CalculateSlideFade(const LayoutFadeInfo& fade_info,
                                 base::Optional<size_t> prev_index,
                                 base::Optional<size_t> next_index,
                                 double scale_percent,
                                 bool slide_from_leading) const;

  // Whether or not to animate the bounds of the host view when the preferred
  // size of the layout changes. If false, the size will have to be set
  // explicitly by the host view's owner. Bounds animation is done by changing
  // the preferred size and invalidating the layout.
  bool should_animate_bounds_ = false;

  // How long each animation takes. Depending on how far along an animation is,
  // a new target layout will either cause the animation to restart or redirect.
  base::TimeDelta animation_duration_ = base::TimeDelta::FromMilliseconds(250);

  // The motion curve of the animation to perform.
  gfx::Tween::Type tween_type_ = gfx::Tween::EASE_IN_OUT;

  // The layout orientation, used for side and scale fades.
  LayoutOrientation orientation_ = LayoutOrientation::kHorizontal;

  // The default fade mode.
  FadeInOutMode default_fade_mode_ = FadeInOutMode::kHide;

  // Used to determine when to fire animation events.
  bool is_animating_ = false;

  // Where in the animation the last layout recalculation happened.
  double starting_offset_ = 0.0;

  // The current animation progress.
  double current_offset_ = 1.0;

  // The layout being animated away from.
  ProposedLayout starting_layout_;

  // The current state of the layout, possibly between |starting_layout_| and
  // |target_layout_|.
  ProposedLayout current_layout_;

  // The desired layout being animated to. When the animation is complete,
  // |current_layout_| will match |target_layout_|.
  ProposedLayout target_layout_;

  // Stores information about elements fading in or out of the layout.
  std::vector<LayoutFadeInfo> fade_infos_;

  std::unique_ptr<AnimationDelegate> animation_delegate_;
  base::ObserverList<Observer, true> observers_;
  std::vector<DelayedAction> delayed_actions_;

  DISALLOW_COPY_AND_ASSIGN(AnimatingLayoutManager);
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_ANIMATING_LAYOUT_MANAGER_H_
