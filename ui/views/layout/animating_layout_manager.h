// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_ANIMATING_LAYOUT_MANAGER_H_
#define UI_VIEWS_LAYOUT_ANIMATING_LAYOUT_MANAGER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/views_export.h"

namespace gfx {
class AnimationContainer;
}  // namespace gfx

namespace views {

// Layout manager which explicitly animates its child views and/or its preferred
// size when the target layout changes (the target layout being provided by a
// separate, non-animating layout manager; typically a FlexLayout).
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
//   animating_layout->SetBoundsAnimationMode(
//       AnimatingLayoutManager::BoundsAnimationMode::kAnimateMainAxis);
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

  // Describes if and how the bounds of the host view can be animated as part of
  // layout animations, if the preferred size of the layout changes.
  enum class BoundsAnimationMode {
    // Default behavior: the host view will always take the space given to it by
    // its parent view and child views will animate within those bounds. Useful
    // for cases where the layout is in a fixed-size container or dialog, but
    // we want child views to be able to animate.
    kUseHostBounds,
    // The host view will request more or less space within the available space
    // offered by its parent view, allowing its main axis size to animate, but
    // will use exactly the cross-axis space provided, as it would with
    // kUseHostBounds. Useful if the host view is in a toolbar or a dialog with
    // fixed width but variable height or vice-versa.
    kAnimateMainAxis,
    // The host view will request more space or less space in both axes within
    // the available space offered by its parent view. Useful if the host view
    // is in e.g. a dialog that can vary in size.
    kAnimateBothAxes
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
    // A view fading in will slide out from the trailing edge and fade in. If
    // the view does not paint to a layer (which is necessary to perform an
    // opacity animation) we fall back to |kSlideFromTrailingEdge|.
    kFadeAndSlideFromTrailingEdge,
  };

  AnimatingLayoutManager();

  AnimatingLayoutManager(const AnimatingLayoutManager&) = delete;
  AnimatingLayoutManager& operator=(const AnimatingLayoutManager&) = delete;

  ~AnimatingLayoutManager() override;

  BoundsAnimationMode bounds_animation_mode() const {
    return bounds_animation_mode_;
  }
  AnimatingLayoutManager& SetBoundsAnimationMode(
      BoundsAnimationMode bounds_animation_mode);

  base::TimeDelta animation_duration() const { return animation_duration_; }
  AnimatingLayoutManager& SetAnimationDuration(
      base::TimeDelta animation_duration);

  gfx::Tween::Type tween_type() const { return tween_type_; }
  AnimatingLayoutManager& SetTweenType(gfx::Tween::Type tween_type);

  base::TimeDelta opacity_animation_duration() const {
    return opacity_animation_duration_;
  }
  // Note this is only needed if using kFadeAndSlideFromTrailingEdge. The
  // duration will not run longer than |animation_duration_| and if shorter than
  // |animation_duration_| the opacity animation will run during the latter part
  // of the fade in the the start of the fade out.
  AnimatingLayoutManager& SetOpacityAnimationDuration(
      base::TimeDelta animation_duration);

  gfx::Tween::Type opacity_tween_type() const { return opacity_tween_type_; }
  AnimatingLayoutManager& SetOpacityTweenType(gfx::Tween::Type tween_type);

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
    return result;
  }
  LayoutManagerBase* target_layout_manager() {
    return num_owned_layouts() ? owned_layout(0) : nullptr;
  }
  const LayoutManagerBase* target_layout_manager() const {
    return num_owned_layouts() ? owned_layout(0) : nullptr;
  }

  // Clears any previous layout, stops any animation, and re-loads the proposed
  // layout from the embedded layout manager. Also invalidates the host view.
  void ResetLayout();

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
  gfx::Size GetPreferredSize(const View* host,
                             const SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize(const View* host) const override;
  int GetPreferredHeightForWidth(const View* host, int width) const override;
  std::vector<raw_ptr<View, VectorExperimental>> GetChildViewsInPaintOrder(
      const View* host) const override;
  bool OnViewRemoved(View* host, View* view) override;

  // Queues an action to take place after the current animation completes.
  // If |action| needs access to external resources, views, etc. then it must
  // check that those resources are still available and valid when it is run. If
  // the layout is not animating the action is posted immediately.
  // There is no guarantee that this action runs as the AnimatingLayoutManager
  // may get torn down before the task runs.
  void PostOrQueueAction(base::OnceClosure action);

  // Returns a flex rule for the host view that will work in the vast majority
  // of cases where the host view is embedded in a FlexLayout.
  FlexRule GetDefaultFlexRule() const;

  // Returns the animation container being used by the layout manager, creating
  // one if one has not yet been created. Implicitly enables animation on this
  // layout, so you do not need to also call EnableAnimationForTesting().
  gfx::AnimationContainer* GetAnimationContainerForTesting();

  // Enables animation of this layout even if the host view does not yet meet
  // the normal requirements for being able to animate (e.g. being added to a
  // widget).
  void EnableAnimationForTesting();

  const ProposedLayout& starting_layout_for_testing() const {
    return starting_layout_;
  }

  const ProposedLayout& target_layout() const { return target_layout_; }

 protected:
  // LayoutManagerBase:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override;
  void OnInstalled(View* host) override;
  bool OnViewAdded(View* host, View* view) override;
  void OnLayoutChanged() override;
  void LayoutImpl() override;

 private:
  struct LayoutFadeInfo;
  class AnimationDelegate;
  friend class AnimationDelegate;

  // Cleans up after an animation and readies actions to be posted.
  void EndAnimation();

  // Equivalent to calling ResetLayoutToSize(GetAvailableTargetLayoutSize()).
  // Convenience method.
  void ResetLayoutToTargetSize();

  // Does the work of ResetLayout(), with the resulting layout snapped to
  // |target_size|.
  void ResetLayoutToSize(const gfx::Size& target_size);

  // Calculates the new target layout and returns true if it has changed.
  bool RecalculateTarget();

  // Called by the animation logic every time a new frame happens.
  void AnimateTo(double value, double fade_in_opacity, double fade_out_opacity);

  // Notifies all observers that the animation state has changed.
  void NotifyIsAnimatingChanged();

  // Runs actions from earlier PostTask() calls.
  void RunQueuedActions();

  // Moves actions from |queued_actions_| to |actions_to_run_| and posts to
  // RunDelayedTasks.
  void PostQueuedActions();

  // Updates the current layout to |percent| interpolated between the starting
  // and target layouts.
  void UpdateCurrentLayout(double percent,
                           double fade_in_opacity,
                           double fade_out_opacity);

  // Updates information about which views are fading in or out during the
  // current animation.
  void CalculateFadeInfos();

  // Called when resetting the layout; resolves any in-progress fades so that a
  // view that should be rendered invisible actually is.
  void ResolveFades();

  // Calculates a kScaleFrom[Minimum|Zero] fade and returns the resulting child
  // layout info.
  ChildLayout CalculateScaleFade(const LayoutFadeInfo& fade_info,
                                 double scale_percent,
                                 bool scale_from_zero) const;

  // Calculates a kSlideFrom[Leading|Trailing]Edge fade and returns the
  // resulting child layout info.
  ChildLayout CalculateSlideFade(const LayoutFadeInfo& fade_info,
                                 double scale_percent,
                                 bool slide_from_leading) const;

  ChildLayout CalculateFadeAndSlideFade(const LayoutFadeInfo& fade_info,
                                        double scale_percent,
                                        double opacity_value,
                                        bool slide_from_leading) const;

  // Returns the space in which to calculate the target layout.
  gfx::Size GetAvailableTargetLayoutSize();

  // Implementation of the default flex rule for animating layout manager.
  // See GetDefaultFlexRule() above.
  static gfx::Size DefaultFlexRuleImpl(
      const AnimatingLayoutManager* animating_layout,
      const View* view,
      const SizeBounds& size_bounds);

  // How to animate bounds of the host view when the preferred size of the
  // layout changes.
  BoundsAnimationMode bounds_animation_mode_ =
      BoundsAnimationMode::kUseHostBounds;

  // How long each animation takes. Depending on how far along an animation is,
  // a new target layout will either cause the animation to restart or redirect.
  base::TimeDelta animation_duration_ = base::Milliseconds(250);

  // The motion curve of the animation to perform.
  gfx::Tween::Type tween_type_ = gfx::Tween::EASE_IN_OUT;

  // How long each opacity animation takes. Note this is only used if using the
  // kFadeAndSlideFromTrailingEdge FadeInOutMode. And is capped at the
  // |animation_duraction_|.
  base::TimeDelta opacity_animation_duration_ = base::Milliseconds(0);

  // The motion curve of the opacity animation to perform.
  gfx::Tween::Type opacity_tween_type_ = gfx::Tween::LINEAR;

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

  // The restrictions on the layout's size the last time we recalculated our
  // target layout. If they have changed, we may need to recalculate the target
  // of the current animation.
  //
  // Contrast with LayoutManagerBase::cached_available_size_, which tracks
  // changes from one layout application to the next and affects re-layout of
  // children; this value tracks changes from one layout *calculation* to
  // the next and affects recalculation of *this* layout.
  SizeBounds last_available_host_size_;

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

  // Actions to be run as animations finish. This is split between queued
  // actions and queued actions to be run as a result of a pending PostTask().
  // This prevents a race condition where PostTask() would pick up queued
  // actions from future delayed actions during animations that were added after
  // PostTask() ran, even if the layout is animating.
  // For example: PostTask() due to finished layout -> start layout animation ->
  // queue action -> the posted task runs while still animating.
  // Without this division of actions + actions to run PostTask would pick up
  // the queued task even though it belonged to a later animation that hasn't
  // yet finished.
  std::vector<base::OnceClosure> queued_actions_;
  std::vector<base::OnceClosure> queued_actions_to_run_;

  // Signal that we want to post queued actions at the end of the next layout
  // cycle.
  bool hold_queued_actions_for_layout_ = false;

  // True when there's a pending PostTask() to RunQueuedActions(). Used to avoid
  // scheduling redundant tasks.
  bool run_queued_actions_is_pending_ = false;

  base::WeakPtrFactory<AnimatingLayoutManager> weak_ptr_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_ANIMATING_LAYOUT_MANAGER_H_
