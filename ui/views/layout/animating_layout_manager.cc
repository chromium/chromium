// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/animating_layout_manager.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/layout/normalized_geometry.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

namespace {

// Returns the ChildLayout data for the child view in the proposed layout, or
// nullptr if not found.
const ChildLayout* FindChildViewInLayout(const ProposedLayout& layout,
                                         const View* view) {
  if (!view)
    return nullptr;

  // The number of children should be small enough that this is more efficient
  // than caching a lookup set.
  for (auto& child_layout : layout.child_layouts) {
    if (child_layout.child_view == view)
      return &child_layout;
  }
  return nullptr;
}

ChildLayout* FindChildViewInLayout(ProposedLayout* layout, const View* view) {
  return const_cast<ChildLayout*>(FindChildViewInLayout(*layout, view));
}

// Describes the type of fade, used by LayoutFadeInfo (see below).
enum class LayoutFadeType {
  // This view is fading in as part of the current animation.
  kFadingIn,
  // This view is fading out as part of the current animation.
  kFadingOut,
  // This view was fading as part of a previous animation that was interrupted
  // and redirected. No child views in the current animation should base their
  // position off of it.
  kContinuingFade
};

// Makes a copy of the given layout with only visible child views (non-visible
// children are omitted).
ProposedLayout WithOnlyVisibleViews(const ProposedLayout layout) {
  ProposedLayout result;
  result.host_size = layout.host_size;
  base::ranges::copy_if(layout.child_layouts,
                        std::back_inserter(result.child_layouts),
                        &ChildLayout::visible);
  return result;
}

// Returns true if the two proposed layouts have the same visible views, with
// the same parameters, in the same order.
bool HaveSameVisibleViews(const ProposedLayout& l1, const ProposedLayout& l2) {
  // There is an approach that uses nested loops and dual iterators that is more
  // efficient than copying, but since this method is only currently called when
  // views are added to the layout, clarity is more important than speed.
  return WithOnlyVisibleViews(l1) == WithOnlyVisibleViews(l2);
}

}  // namespace

// Holds data about a view that is fading in or out as part of an animation.
struct AnimatingLayoutManager::LayoutFadeInfo {
  // How the child view is fading.
  LayoutFadeType fade_type;
  // The child view which is fading.
  raw_ptr<View> child_view = nullptr;
  // The view previous (leading side) to the fading view which is in both the
  // starting and target layout, or null if none.
  raw_ptr<View> prev_view = nullptr;
  // The view next (trailing side) to the fading view which is in both the
  // starting and target layout, or null if none.
  raw_ptr<View> next_view = nullptr;
  // The full-size bounds, normalized to the orientation of the layout manager,
  // that |child_view| starts with, if fading out, or ends with, if fading in.
  NormalizedRect reference_bounds;
  // The offset from the end of |prev_view| and the start of |next_view|. Insets
  // may be negative if the views overlap.
  Inset1D offsets;
};

// Manages the animation and various callbacks from the animation system that
// are required to update the layout during animations.
class AnimatingLayoutManager::AnimationDelegate
    : public AnimationDelegateViews {
 public:
  explicit AnimationDelegate(AnimatingLayoutManager* layout_manager);
  ~AnimationDelegate() override = default;

  // Returns true after the host view is added to a widget or animation has been
  // enabled by a unit test.
  //
  // Before that, animation is not possible, so all changes to the host view
  // should result in the host view's layout being snapped directly to the
  // target layout.
  bool ready_to_animate() const { return ready_to_animate_; }

  // Pushes animation configuration (tween type, duration) through to the
  // animation itself.
  void UpdateAnimationParameters();

  // Starts the animation.
  void Animate();

  // Cancels and resets the current animation (if any).
  void Reset();

  // If the current layout is not yet ready to animate, transitions into the
  // ready-to-animate state, possibly resetting the current layout and
  // invalidating the host to make sure the layout is up to date.
  void MakeReadyForAnimation();

  // Overrides the default animation container with |container|.
  void SetAnimationContainerForTesting(gfx::AnimationContainer* container) {
    animation_->SetContainer(container);
  }

 private:
  // Observer used to watch for the host view being parented to a widget.
  class ViewWidgetObserver : public ViewObserver {
   public:
    explicit ViewWidgetObserver(AnimationDelegate* animation_delegate)
        : animation_delegate_(animation_delegate) {}

    void OnViewAddedToWidget(View* observed_view) override {
      animation_delegate_->MakeReadyForAnimation();
    }

    void OnViewIsDeleting(View* observed_view) override {
      animation_delegate_->scoped_observation_.Reset();
    }

   private:
    const raw_ptr<AnimationDelegate> animation_delegate_;
  };
  friend class Observer;

  // AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  bool ready_to_animate_ = false;
  bool resetting_animation_ = false;
  const raw_ptr<AnimatingLayoutManager> target_layout_manager_;
  std::unique_ptr<gfx::MultiAnimation> fade_in_opacity_animation_;
  std::unique_ptr<gfx::MultiAnimation> fade_out_opacity_animation_;
  std::unique_ptr<gfx::SlideAnimation> animation_;
  const raw_ptr<gfx::AnimationContainer> container_;
  ViewWidgetObserver view_widget_observer_{this};
  base::ScopedObservation<View, ViewObserver> scoped_observation_{
      &view_widget_observer_};
};

AnimatingLayoutManager::AnimationDelegate::AnimationDelegate(
    AnimatingLayoutManager* layout_manager)
    : AnimationDelegateViews(layout_manager->host_view()),
      target_layout_manager_(layout_manager),
      animation_(std::make_unique<gfx::SlideAnimation>(this)),
      container_(new gfx::AnimationContainer()) {
  animation_->SetContainer(container_);
  View* const host_view = layout_manager->host_view();
  DCHECK(host_view);
  if (host_view->GetWidget())
    MakeReadyForAnimation();
  else
    scoped_observation_.Observe(host_view);
  UpdateAnimationParameters();
}

void AnimatingLayoutManager::AnimationDelegate::UpdateAnimationParameters() {
  animation_->SetTweenType(target_layout_manager_->tween_type());
  animation_->SetSlideDuration(target_layout_manager_->animation_duration());

  base::TimeDelta opacity_animation_duration =
      std::min(target_layout_manager_->animation_duration(),
               target_layout_manager_->opacity_animation_duration());
  if (!opacity_animation_duration.is_zero()) {
    fade_in_opacity_animation_ = std::make_unique<gfx::MultiAnimation>(
        std::vector<gfx::MultiAnimation::Part>{
            gfx::MultiAnimation::Part(
                target_layout_manager_->animation_duration() -
                    opacity_animation_duration,
                gfx::Tween::Type::LINEAR, 0.0, 0.0),
            gfx::MultiAnimation::Part(
                opacity_animation_duration,
                target_layout_manager_->opacity_tween_type(), 0.0, 1.0)});
    fade_in_opacity_animation_->SetContainer(container_);
    fade_in_opacity_animation_->set_continuous(false);
    const base::TimeDelta fade_out_opacity_duration =
        target_layout_manager_->animation_duration() ==
                opacity_animation_duration
            ? opacity_animation_duration
            : target_layout_manager_->animation_duration() -
                  opacity_animation_duration;
    fade_out_opacity_animation_ = std::make_unique<gfx::MultiAnimation>(
        std::vector<gfx::MultiAnimation::Part>{
            gfx::MultiAnimation::Part(
                fade_out_opacity_duration,
                target_layout_manager_->opacity_tween_type(), 1.0, 0.0),
            gfx::MultiAnimation::Part(
                target_layout_manager_->animation_duration() -
                    fade_out_opacity_duration,
                gfx::Tween::Type::LINEAR, 0.0, 0.0)});
    fade_out_opacity_animation_->SetContainer(container_);
    fade_out_opacity_animation_->set_continuous(false);
  }
}

void AnimatingLayoutManager::AnimationDelegate::Animate() {
  DCHECK(ready_to_animate_);
  Reset();
  animation_->Show();
  if (target_layout_manager_->opacity_animation_duration() >
      base::Milliseconds(0)) {
    if (fade_in_opacity_animation_.get()) {
      fade_in_opacity_animation_->Start();
    }
    if (fade_out_opacity_animation_.get()) {
      fade_out_opacity_animation_->Start();
    }
  }
}

void AnimatingLayoutManager::AnimationDelegate::Reset() {
  if (!ready_to_animate_)
    return;
  base::AutoReset<bool> setter(&resetting_animation_, true);
  animation_->Reset();
  if (fade_in_opacity_animation_.get()) {
    fade_in_opacity_animation_->Stop();
  }
  if (fade_out_opacity_animation_.get()) {
    fade_out_opacity_animation_->Stop();
  }
}

void AnimatingLayoutManager::AnimationDelegate::MakeReadyForAnimation() {
  if (!ready_to_animate_) {
    target_layout_manager_->ResetLayout();
    ready_to_animate_ = true;
    scoped_observation_.Reset();
  }
}

void AnimatingLayoutManager::AnimationDelegate::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK(animation_.get() == animation);
  double fade_in_opacity = fade_in_opacity_animation_.get()
                               ? fade_in_opacity_animation_->GetCurrentValue()
                               : 1.0;
  double fade_out_opacity = fade_out_opacity_animation_.get()
                                ? fade_out_opacity_animation_->GetCurrentValue()
                                : 0.0;
  target_layout_manager_->AnimateTo(animation->GetCurrentValue(),
                                    fade_in_opacity, fade_out_opacity);
}

void AnimatingLayoutManager::AnimationDelegate::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void AnimatingLayoutManager::AnimationDelegate::AnimationEnded(
    const gfx::Animation* animation) {
  if (resetting_animation_)
    return;
  DCHECK(animation_.get() == animation);
  target_layout_manager_->AnimateTo(1.0, 1.0, 0.0);
}

// AnimatingLayoutManager:

AnimatingLayoutManager::AnimatingLayoutManager() = default;
AnimatingLayoutManager::~AnimatingLayoutManager() = default;

AnimatingLayoutManager& AnimatingLayoutManager::SetBoundsAnimationMode(
    BoundsAnimationMode bounds_animation_mode) {
  if (bounds_animation_mode_ != bounds_animation_mode) {
    bounds_animation_mode_ = bounds_animation_mode;
    ResetLayout();
  }
  return *this;
}

AnimatingLayoutManager& AnimatingLayoutManager::SetAnimationDuration(
    base::TimeDelta animation_duration) {
  DCHECK_GE(animation_duration, base::TimeDelta());
  animation_duration_ = animation_duration;
  if (animation_delegate_)
    animation_delegate_->UpdateAnimationParameters();
  return *this;
}

AnimatingLayoutManager& AnimatingLayoutManager::SetTweenType(
    gfx::Tween::Type tween_type) {
  tween_type_ = tween_type;
  if (animation_delegate_)
    animation_delegate_->UpdateAnimationParameters();
  return *this;
}

AnimatingLayoutManager& AnimatingLayoutManager::SetOpacityAnimationDuration(
    base::TimeDelta animation_duration) {
  DCHECK_GE(animation_duration, base::TimeDelta());
  opacity_animation_duration_ = animation_duration;
  if (animation_delegate_) {
    animation_delegate_->UpdateAnimationParameters();
  }
  return *this;
}

AnimatingLayoutManager& AnimatingLayoutManager::SetOpacityTweenType(
    gfx::Tween::Type tween_type) {
  opacity_tween_type_ = tween_type;
  if (animation_delegate_) {
    animation_delegate_->UpdateAnimationParameters();
  }
  return *this;
}

AnimatingLayoutManager& AnimatingLayoutManager::SetOrientation(
    LayoutOrientation orientation) {
  if (orientation_ != orientation) {
    orientation_ = orientation;
    ResetLayout();
  }
  return *this;
}

AnimatingLayoutManager& AnimatingLayoutManager::SetDefaultFadeMode(
    FadeInOutMode default_fade_mode) {
  default_fade_mode_ = default_fade_mode;
  return *this;
}

void AnimatingLayoutManager::ResetLayout() {
  if (!target_layout_manager())
    return;
  ResetLayoutToTargetSize();
  InvalidateHost(false);
}

void AnimatingLayoutManager::FadeOut(View* child_view) {
  DCHECK(child_view);
  DCHECK(child_view->parent());
  DCHECK_EQ(host_view(), child_view->parent());

  // If the view in question is already incapable of being visible, either:
  // 1. the view wasn't capable of being visible in the first place
  // 2. the view is already invisible because the layout has chosen to hide it
  // In either case, it is generally useful to recalculate the layout just in
  // case the caller has made other changes that won't directly cause a layout -
  // for example, the user has changed a layout-affecting class property. Worst
  // case this ends up being a slightly costly no-op but we don't expect this
  // method to be called very often.
  if (!CanBeVisible(child_view)) {
    InvalidateHost(true);
    return;
  }

  // This handles a case where we are in the middle of an animation where we
  // would have hidden the target view, but haven't laid out yet, so haven't
  // actually hidden it yet. Because we plan fade-outs off of the current layout
  // if the view the child view is visible it will not get a proper fade-out and
  // will remain visible but not properly laid out. We remedy this by hiding the
  // view immediately.
  const ChildLayout* const current_layout =
      FindChildViewInLayout(current_layout_, child_view);
  if ((!current_layout || !current_layout->visible) && child_view->GetVisible())
    SetViewVisibility(child_view, false);

  // Indicate that the view should become hidden in the layout without
  // immediately changing its visibility. Instead, this triggers an animation
  // which results in the view being hidden.
  //
  // This method is typically only called from View and has a private final
  // implementation in LayoutManagerBase so we have to cast to call it.
  static_cast<LayoutManager*>(this)->ViewVisibilitySet(
      host_view(), child_view, child_view->GetVisible(), false);
}

void AnimatingLayoutManager::FadeIn(View* child_view) {
  DCHECK(child_view);
  DCHECK(child_view->parent());
  DCHECK_EQ(host_view(), child_view->parent());

  // If the view in question is already capable of being visible, either:
  // 1. the view is already visible so this is a no-op
  // 2. the view is not visible because the target layout has chosen to hide it
  // In either case, it is generally useful to recalculate the layout just in
  // case the caller has made other changes that won't directly cause a layout -
  // for example, the user has changed a layout-affecting class property. Worst
  // case this ends up being a slightly costly no-op but we don't expect this
  // method to be called very often.
  if (CanBeVisible(child_view)) {
    InvalidateHost(true);
    return;
  }

  // Indicate that the view should become visible in the layout without
  // immediately changing its visibility. Instead, this triggers an animation
  // which results in the view being shown.
  //
  // This method is typically only called from View and has a private final
  // implementation in LayoutManagerBase so we have to cast to call it.
  static_cast<LayoutManager*>(this)->ViewVisibilitySet(
      host_view(), child_view, child_view->GetVisible(), true);
}

void AnimatingLayoutManager::AddObserver(Observer* observer) {
  if (!observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void AnimatingLayoutManager::RemoveObserver(Observer* observer) {
  if (observers_.HasObserver(observer))
    observers_.RemoveObserver(observer);
}

bool AnimatingLayoutManager::HasObserver(Observer* observer) const {
  return observers_.HasObserver(observer);
}

gfx::Size AnimatingLayoutManager::GetPreferredSize(const View* host) const {
  if (!target_layout_manager())
    return gfx::Size();

  // If animation is disabled, preferred size does not change with current
  // animation state.
  if (!gfx::Animation::ShouldRenderRichAnimation())
    return target_layout_manager()->GetPreferredSize(host);

  switch (bounds_animation_mode_) {
    case BoundsAnimationMode::kUseHostBounds:
      return target_layout_manager()->GetPreferredSize(host);
    case BoundsAnimationMode::kAnimateMainAxis: {
      // Animating only main axis, so cross axis is preferred size.
      gfx::Size result = current_layout_.host_size;
      SetCrossAxis(
          &result, orientation(),
          GetCrossAxis(orientation(),
                       target_layout_manager()->GetPreferredSize(host)));
      return result;
    }
    case BoundsAnimationMode::kAnimateBothAxes:
      return current_layout_.host_size;
  }
}

gfx::Size AnimatingLayoutManager::GetPreferredSize(
    const View* host,
    const SizeBounds& available_size) const {
  if (!target_layout_manager()) {
    return gfx::Size();
  }

  // If animation is disabled, preferred size does not change with current
  // animation state.
  if (!gfx::Animation::ShouldRenderRichAnimation()) {
    return target_layout_manager()->GetPreferredSize(host, available_size);
  }

  switch (bounds_animation_mode_) {
    case BoundsAnimationMode::kUseHostBounds:
      return target_layout_manager()->GetPreferredSize(host, available_size);
    case BoundsAnimationMode::kAnimateMainAxis: {
      // Animating only main axis, so cross axis is preferred size.
      gfx::Size result = current_layout_.host_size;
      SetCrossAxis(
          &result, orientation(),
          GetCrossAxis(orientation(), target_layout_manager()->GetPreferredSize(
                                          host, available_size)));
      return result;
    }
    case BoundsAnimationMode::kAnimateBothAxes:
      return current_layout_.host_size;
  }
}

gfx::Size AnimatingLayoutManager::GetMinimumSize(const View* host) const {
  if (!target_layout_manager())
    return gfx::Size();
  // TODO(dfried): consider cases where the minimum size might not be just the
  // minimum size of the embedded layout.
  gfx::Size minimum_size = target_layout_manager()->GetMinimumSize(host);
  switch (bounds_animation_mode_) {
    case BoundsAnimationMode::kUseHostBounds:
      // No modification required.
      break;
    case BoundsAnimationMode::kAnimateMainAxis:
      SetMainAxis(
          &minimum_size, orientation(),
          std::min(GetMainAxis(orientation(), minimum_size),
                   GetMainAxis(orientation(), current_layout_.host_size)));
      break;
    case BoundsAnimationMode::kAnimateBothAxes:
      minimum_size.SetToMin(current_layout_.host_size);
      break;
  }
  return minimum_size;
}

int AnimatingLayoutManager::GetPreferredHeightForWidth(const View* host,
                                                       int width) const {
  if (!target_layout_manager())
    return 0;

  // TODO(dfried): revisit this computation.
  if (bounds_animation_mode_ == BoundsAnimationMode::kAnimateBothAxes ||
      (bounds_animation_mode_ == BoundsAnimationMode::kAnimateMainAxis &&
       orientation() == LayoutOrientation::kVertical)) {
    return current_layout_.host_size.height();
  }
  return target_layout_manager()->GetPreferredHeightForWidth(host, width);
}

std::vector<raw_ptr<View, VectorExperimental>>
AnimatingLayoutManager::GetChildViewsInPaintOrder(const View* host) const {
  DCHECK_EQ(host_view(), host);

  if (!is_animating())
    return LayoutManagerBase::GetChildViewsInPaintOrder(host);

  std::vector<raw_ptr<View, VectorExperimental>> result;
  std::set<View*> fading;

  // Put all fading views to the front of the list (back of the Z-order).
  for (const LayoutFadeInfo& fade_info : fade_infos_) {
    result.push_back(fade_info.child_view.get());
    fading.insert(fade_info.child_view);
  }

  // Add the result of the views.
  for (View* child : host->children()) {
    if (!base::Contains(fading, child))
      result.push_back(child);
  }

  return result;
}

bool AnimatingLayoutManager::OnViewRemoved(View* host, View* view) {
  // Remove any fade infos corresponding to the removed view.
  std::erase_if(fade_infos_, [view](const LayoutFadeInfo& fade_info) {
    return fade_info.child_view == view;
  });

  // Also delete any references from other fade infos. This prevents dangling
  // partition pointers when the layout is invariably invalidated later.
  for (auto& fade_info : fade_infos_) {
    if (fade_info.next_view == view) {
      fade_info.next_view = nullptr;
    }
    if (fade_info.prev_view == view) {
      fade_info.prev_view = nullptr;
    }
  }

  // Remove any elements in the current layout corresponding to the removed
  // view.
  std::erase_if(current_layout_.child_layouts,
                [view](const ChildLayout& child_layout) {
                  return child_layout.child_view == view;
                });

  return LayoutManagerBase::OnViewRemoved(host, view);
}

void AnimatingLayoutManager::PostOrQueueAction(base::OnceClosure action) {
  queued_actions_.push_back(std::move(action));
  if (!is_animating() && !hold_queued_actions_for_layout_)
    PostQueuedActions();
}

FlexRule AnimatingLayoutManager::GetDefaultFlexRule() const {
  return base::BindRepeating(&AnimatingLayoutManager::DefaultFlexRuleImpl,
                             base::Unretained(this));
}

gfx::AnimationContainer*
AnimatingLayoutManager::GetAnimationContainerForTesting() {
  DCHECK(animation_delegate_);
  animation_delegate_->MakeReadyForAnimation();
  DCHECK(animation_delegate_->ready_to_animate());
  return animation_delegate_->container();
}

void AnimatingLayoutManager::EnableAnimationForTesting() {
  DCHECK(animation_delegate_);
  animation_delegate_->MakeReadyForAnimation();
  DCHECK(animation_delegate_->ready_to_animate());
}

ProposedLayout AnimatingLayoutManager::CalculateProposedLayout(
    const SizeBounds& size_bounds) const {
  // This class directly overrides Layout() so GetProposedLayout() and
  // CalculateProposedLayout() are not called.
  NOTREACHED();
}

void AnimatingLayoutManager::OnInstalled(View* host) {
  DCHECK(!animation_delegate_);
  animation_delegate_ = std::make_unique<AnimationDelegate>(this);
}

bool AnimatingLayoutManager::OnViewAdded(View* host, View* view) {
  // Handle a case where we add a visible view that shouldn't be visible in the
  // layout. In this case, there is no animation, no invalidation, and we just
  // set the view to not be visible.
  if (IsChildIncludedInLayout(view) && cached_layout_size() && !is_animating_) {
    const gfx::Size target_size = GetAvailableTargetLayoutSize();
    ProposedLayout proposed_layout =
        target_layout_manager()->GetProposedLayout(target_size);
    if (HaveSameVisibleViews(current_layout_, proposed_layout)) {
      SetViewVisibility(view, false);
      current_layout_ = target_layout_ = proposed_layout;
      return false;
    }
  }

  return RecalculateTarget();
}

void AnimatingLayoutManager::OnLayoutChanged() {
  // This replaces the normal behavior of clearing cached layouts.
  RecalculateTarget();
}

void AnimatingLayoutManager::LayoutImpl() {
  // Changing the size of a view directly will lead to a layout call rather
  // than an invalidation. This should reset the layout (but see the note in
  // RecalculateTarget() below).
  const gfx::Size host_size = host_view()->size();

  if (bounds_animation_mode_ == BoundsAnimationMode::kUseHostBounds) {
    if (!cached_layout_size()) {
      // No previous layout, so snap to the target.
      ResetLayoutToTargetSize();
    } else if (host_size != *cached_layout_size()) {
      // Host size changed, so animate.
      RecalculateTarget();
    }
  } else {
    const SizeBounds available_size = GetAvailableHostSize();

    if (bounds_animation_mode_ == BoundsAnimationMode::kAnimateMainAxis &&
        (!cached_layout_size() ||
         GetCrossAxis(orientation(), host_size) !=
             GetCrossAxis(orientation(), *cached_layout_size()))) {
      // If we're fixed to the cross-axis size of the host and that size
      // changes, we need to reset the layout.
      last_available_host_size_ = available_size;
      ResetLayoutToSize(host_size);
    } else {
      // Either both axes are animating or only the main axis is animating or
      // the cross axis hasn't changed (because otherwise the previous condition
      // would have executed instead).
      const SizeBound bounds_main = GetMainAxis(orientation(), available_size);
      const int host_main = GetMainAxis(orientation(), host_size);
      const int current_main =
          GetMainAxis(orientation(), current_layout_.host_size);
      if ((current_main > host_main) || (current_main > bounds_main)) {
        // Reset the layout immediately if the current layout exceeds the host
        // size or the available space.
        last_available_host_size_ = available_size;
        ResetLayoutToSize(host_size);
      } else if (available_size != last_available_host_size_) {
        // May need to re-trigger animation if our bounds were relaxed; let us
        // expand into the new available space.
        RecalculateTarget();
      }
    }

    // Verify that the last available size has been updated.
    DCHECK_EQ(available_size, last_available_host_size_);
  }

  ApplyLayout(current_layout_);

  // Send animating stopped events on layout so the current layout during the
  // event represents the final state instead of an intermediate state.
  if (is_animating_ && current_offset_ == 1.0)
    EndAnimation();

  if (hold_queued_actions_for_layout_ && !is_animating_) {
    hold_queued_actions_for_layout_ = false;
    PostQueuedActions();
  }
}

void AnimatingLayoutManager::EndAnimation() {
  // Make sure an opacity animation is in the correct state before clearing
  // |fade_infos_|.
  for (auto fade_info : fade_infos_) {
    if (fade_info.fade_type != LayoutFadeType::kFadingOut &&
        fade_info.child_view->layer()) {
      fade_info.child_view->layer()->SetOpacity(1);
    }
  }
  fade_infos_.clear();
  hold_queued_actions_for_layout_ = true;
  if (std::exchange(is_animating_, false))
    NotifyIsAnimatingChanged();
}

void AnimatingLayoutManager::ResetLayoutToTargetSize() {
  ResetLayoutToSize(GetAvailableTargetLayoutSize());
}

void AnimatingLayoutManager::ResetLayoutToSize(const gfx::Size& target_size) {
  if (animation_delegate_)
    animation_delegate_->Reset();

  ResolveFades();

  target_layout_ = target_layout_manager()->GetProposedLayout(target_size);
  current_layout_ = target_layout_;
  starting_layout_ = current_layout_;
  current_offset_ = 1.0;
  set_cached_layout_size(target_size);
  EndAnimation();
}

bool AnimatingLayoutManager::RecalculateTarget() {
  constexpr double kResetAnimationThreshold = 0.8;

  if (!target_layout_manager())
    return false;

  if (!cached_layout_size() || !animation_delegate_ ||
      !animation_delegate_->ready_to_animate()) {
    ResetLayoutToTargetSize();
    return true;
  }

  const gfx::Size target_size = GetAvailableTargetLayoutSize();
  set_cached_layout_size(target_size);

  // If there has been no appreciable change in layout, there's no reason to
  // start or update an animation.
  const ProposedLayout proposed_layout =
      target_layout_manager()->GetProposedLayout(target_size);

  if (target_layout_ == proposed_layout)
    return false;

  target_layout_ = proposed_layout;
  if (current_offset_ > kResetAnimationThreshold) {
    starting_layout_ = current_layout_;
    starting_offset_ = 0.0;
    current_offset_ = 0.0;
    animation_delegate_->Animate();
    if (!is_animating_) {
      is_animating_ = true;
      NotifyIsAnimatingChanged();
    }
  } else if (current_offset_ > starting_offset_) {
    // Only update the starting layout if the animation has progressed. This has
    // the effect of "batching up" changes that all happen on the same frame,
    // keeping the same starting point. (A common example of this is multiple
    // child views' visibility changing.)
    starting_layout_ = current_layout_;
    starting_offset_ = current_offset_;
  } else if (starting_layout_ == target_layout_) {
    // If we initiated but did not show any frames of an animation, and we are
    // redirected to our starting layout then just reset the layout.
    ResetLayoutToSize(target_size);
    return false;
  }
  CalculateFadeInfos();

  // We've calculated all of the targets and fades. Start the layout process if
  // we are animating, but if animations are disabled, snap to the final
  // layout.
  if (gfx::Animation::ShouldRenderRichAnimation()) {
    UpdateCurrentLayout(0.0, 0.0, 1.0);
  } else {
    ResetLayoutToSize(target_size);
  }

  return true;
}

void AnimatingLayoutManager::AnimateTo(double value,
                                       double fade_in_opacity,
                                       double fade_out_opacity) {
  DCHECK_GE(value, 0.0);
  DCHECK_LE(value, 1.0);
  DCHECK_GE(value, starting_offset_);
  DCHECK_GE(value, current_offset_);
  if (current_offset_ == value)
    return;
  current_offset_ = value;
  const double percent =
      (current_offset_ - starting_offset_) / (1.0 - starting_offset_);
  UpdateCurrentLayout(percent, fade_in_opacity, fade_out_opacity);
  InvalidateHost(false);
}

void AnimatingLayoutManager::NotifyIsAnimatingChanged() {
  observers_.Notify(&Observer::OnLayoutIsAnimatingChanged, this,
                    is_animating());
}

void AnimatingLayoutManager::RunQueuedActions() {
  run_queued_actions_is_pending_ = false;
  std::vector<base::OnceClosure> actions = std::move(queued_actions_to_run_);
  for (auto& action : actions)
    std::move(action).Run();
}

void AnimatingLayoutManager::PostQueuedActions() {
  // Move queued actions over to actions that should run during the next
  // PostTask(). This prevents a race between old PostTask() calls and new
  // delayed actions. See the header for more detail.
  for (auto& action : queued_actions_)
    queued_actions_to_run_.push_back(std::move(action));
  queued_actions_.clear();

  // Early return to prevent multiple RunQueuedAction() tasks.
  if (run_queued_actions_is_pending_)
    return;

  // Post to self (instead of posting the queued actions directly) which lets
  // us:
  // * Keep "AnimatingLayoutManager::RunQueuedActions" in the stack frame.
  // * Tie the task lifetimes to AnimatingLayoutManager.
  run_queued_actions_is_pending_ =
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&AnimatingLayoutManager::RunQueuedActions,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void AnimatingLayoutManager::UpdateCurrentLayout(double percent,
                                                 double fade_in_opacity,
                                                 double fade_out_opacity) {
  // This drops out any child view elements that don't exist in the target
  // layout. We'll add them back in later.
  current_layout_ =
      ProposedLayoutBetween(percent, starting_layout_, target_layout_);

  for (const LayoutFadeInfo& fade_info : fade_infos_) {
    // This shouldn't happen but we should ensure that with a check.
    DCHECK(host_view()->GetIndexOf(fade_info.child_view).has_value());

    // Views that were previously fading are animated as normal, so nothing to
    // do here.
    if (fade_info.fade_type == LayoutFadeType::kContinuingFade) {
      continue;
    }

    ChildLayout child_layout;

    if (percent == 1.0) {
      // At the end of the animation snap to the final state of the child view.
      child_layout.child_view = fade_info.child_view;
      switch (fade_info.fade_type) {
        case LayoutFadeType::kFadingIn:
          child_layout.visible = true;
          child_layout.bounds =
              Denormalize(orientation(), fade_info.reference_bounds);
          if (fade_info.child_view->layer()) {
            fade_info.child_view->layer()->SetOpacity(1);
          }
          break;
        case LayoutFadeType::kFadingOut:
          child_layout.visible = false;
          if (child_layout.child_view->layer()) {
            child_layout.child_view->layer()->SetOpacity(0);
          }
          break;
        case LayoutFadeType::kContinuingFade:
          NOTREACHED();
      }
    } else if (default_fade_mode_ == FadeInOutMode::kHide) {
      child_layout.child_view = fade_info.child_view;
      child_layout.visible = false;
    } else {
      const double scale_percent =
          fade_info.fade_type == LayoutFadeType::kFadingIn ? percent
                                                           : 1.0 - percent;
      double opacity_value = fade_info.fade_type == LayoutFadeType::kFadingIn
                                 ? fade_in_opacity
                                 : fade_out_opacity;

      switch (default_fade_mode_) {
        case FadeInOutMode::kHide:
          NOTREACHED();
        case FadeInOutMode::kScaleFromMinimum:
          child_layout = CalculateScaleFade(fade_info, scale_percent,
                                            /* scale_from_zero */ false);
          break;
        case FadeInOutMode::kScaleFromZero:
          child_layout = CalculateScaleFade(fade_info, scale_percent,
                                            /* scale_from_zero */ true);
          break;
        case FadeInOutMode::kSlideFromLeadingEdge:
          child_layout = CalculateSlideFade(fade_info, scale_percent,
                                            /* slide_from_leading */ true);
          break;
        case FadeInOutMode::kSlideFromTrailingEdge:
          child_layout = CalculateSlideFade(fade_info, scale_percent,
                                            /* slide_from_leading */ false);
          break;
        case FadeInOutMode::kFadeAndSlideFromTrailingEdge:
          child_layout = CalculateFadeAndSlideFade(fade_info, scale_percent,
                                                   opacity_value, false);
          break;
      }
    }

    ChildLayout* const to_overwrite =
        FindChildViewInLayout(&current_layout_, fade_info.child_view);
    if (to_overwrite)
      *to_overwrite = child_layout;
    else
      current_layout_.child_layouts.push_back(child_layout);
  }
}

void AnimatingLayoutManager::CalculateFadeInfos() {
  // Save any views that were previously fading so we don't try to key off of
  // them when calculating leading/trailing edge.
  std::set<const View*> previously_fading;
  for (const auto& fade_info : fade_infos_)
    previously_fading.insert(fade_info.child_view);

  fade_infos_.clear();

  struct ChildInfo {
    std::optional<size_t> start;
    NormalizedRect start_bounds;
    bool start_visible = false;
    std::optional<size_t> target;
    NormalizedRect target_bounds;
    bool target_visible = false;
  };

  std::map<View*, ChildInfo> child_to_info;
  std::map<int, View*> start_leading_edges;
  std::map<int, View*> target_leading_edges;

  // Collect some bookkeping info to prevent linear searches later.

  for (View* child : host_view()->children()) {
    if (IsChildIncludedInLayout(child, /* include hidden */ true))
      child_to_info.emplace(child, ChildInfo());
  }

  for (size_t i = 0; i < starting_layout_.child_layouts.size(); ++i) {
    const auto& child_layout = starting_layout_.child_layouts[i];
    auto it = child_to_info.find(child_layout.child_view);
    if (it != child_to_info.end()) {
      it->second.start = i;
      it->second.start_bounds = Normalize(orientation(), child_layout.bounds);
      it->second.start_visible = child_layout.visible;
    }
  }

  for (size_t i = 0; i < target_layout_.child_layouts.size(); ++i) {
    const auto& child_layout = target_layout_.child_layouts[i];
    auto it = child_to_info.find(child_layout.child_view);
    if (it != child_to_info.end()) {
      it->second.target = i;
      it->second.target_bounds = Normalize(orientation(), child_layout.bounds);
      it->second.target_visible = child_layout.visible;
    }
  }

  for (View* child : host_view()->children()) {
    const auto& index = child_to_info[child];
    if (index.start_visible && index.target_visible &&
        !base::Contains(previously_fading, child)) {
      start_leading_edges.emplace(index.start_bounds.origin_main(), child);
      target_leading_edges.emplace(index.target_bounds.origin_main(), child);
    }
  }

  // Build the LayoutFadeInfo data.

  const NormalizedSize start_host_size =
      Normalize(orientation(), starting_layout_.host_size);
  const NormalizedSize target_host_size =
      Normalize(orientation(), target_layout_.host_size);

  for (View* child : host_view()->children()) {
    const auto& current = child_to_info[child];
    if (current.start_visible && !current.target_visible) {
      LayoutFadeInfo fade_info;
      fade_info.fade_type = LayoutFadeType::kFadingOut;
      fade_info.child_view = child;
      fade_info.reference_bounds = current.start_bounds;
      auto next =
          start_leading_edges.upper_bound(current.start_bounds.origin_main());
      if (next == start_leading_edges.end()) {
        fade_info.next_view = nullptr;
        fade_info.offsets.set_trailing(start_host_size.main() -
                                       current.start_bounds.max_main());
      } else {
        fade_info.next_view = next->second;
        fade_info.offsets.set_trailing(next->first -
                                       current.start_bounds.max_main());
      }
      if (next == start_leading_edges.begin()) {
        fade_info.prev_view = nullptr;
        fade_info.offsets.set_leading(current.start_bounds.origin_main());
      } else {
        auto prev = next;
        --prev;
        const auto& prev_info = child_to_info[prev->second];
        fade_info.prev_view = prev->second;
        fade_info.offsets.set_leading(current.start_bounds.origin_main() -
                                      prev_info.start_bounds.max_main());
      }
      fade_infos_.push_back(fade_info);
    } else if (!current.start_visible && current.target_visible) {
      LayoutFadeInfo fade_info;
      fade_info.fade_type = LayoutFadeType::kFadingIn;
      fade_info.child_view = child;
      fade_info.reference_bounds = current.target_bounds;
      auto next =
          target_leading_edges.upper_bound(current.target_bounds.origin_main());
      if (next == target_leading_edges.end()) {
        fade_info.next_view = nullptr;
        fade_info.offsets.set_trailing(target_host_size.main() -
                                       current.target_bounds.max_main());
      } else {
        fade_info.next_view = next->second;
        fade_info.offsets.set_trailing(next->first -
                                       current.target_bounds.max_main());
      }
      if (next == target_leading_edges.begin()) {
        fade_info.prev_view = nullptr;
        fade_info.offsets.set_leading(current.target_bounds.origin_main());
      } else {
        auto prev = next;
        --prev;
        const auto& prev_info = child_to_info[prev->second];
        fade_info.prev_view = prev->second;
        fade_info.offsets.set_leading(current.target_bounds.origin_main() -
                                      prev_info.target_bounds.max_main());
      }
      fade_infos_.push_back(fade_info);
    } else if (base::Contains(previously_fading, child)) {
      // Capture the fact that this view was fading as part of an animation that
      // was interrupted. (It is therefore technically still fading.) This
      // status goes away when the animation ends.
      LayoutFadeInfo fade_info;
      fade_info.fade_type = LayoutFadeType::kContinuingFade;
      fade_info.child_view = child;
      // No reference bounds or offsets since we'll use the normal animation
      // pathway for this view.
      fade_infos_.push_back(fade_info);
    }
  }
}

void AnimatingLayoutManager::ResolveFades() {
  // Views that need faded out are views which were were fading out previously
  // because they were set to not be visible, either by calling SetVisible() or
  // FadeOut(). Those views will not be included in the new layout but may not
  // have been allowed to become invisible yet because of the fade-out
  // animation. Even in the case of FadeInOutMode::kHide, if no frames of the
  // animation have run, the relevant view may still be visible.
  for (const LayoutFadeInfo& fade_info : fade_infos_) {
    View* const child = fade_info.child_view;
    if (fade_info.fade_type == LayoutFadeType::kFadingOut &&
        host_view()->GetIndexOf(child).has_value() &&
        !child->GetProperty(kViewIgnoredByLayoutKey) &&
        !IsChildIncludedInLayout(child)) {
      SetViewVisibility(child, false);
    }
    if (default_fade_mode_ == FadeInOutMode::kFadeAndSlideFromTrailingEdge &&
        fade_info.fade_type == LayoutFadeType::kFadingIn &&
        host_view()->GetIndexOf(child).has_value() && child->layer()) {
      child->layer()->SetOpacity(1);
    }
  }
}

ChildLayout AnimatingLayoutManager::CalculateScaleFade(
    const LayoutFadeInfo& fade_info,
    double scale_percent,
    bool scale_from_zero) const {
  ChildLayout child_layout;

  int leading_reference_point = 0;
  if (fade_info.prev_view) {
    // Since prev/next view is always a view in the start and target layouts, it
    // should also be in the current layout. Therefore this should never return
    // null.
    const ChildLayout* const prev_layout =
        FindChildViewInLayout(current_layout_, fade_info.prev_view);
    leading_reference_point =
        Normalize(orientation(), prev_layout->bounds).max_main();
  }
  leading_reference_point += fade_info.offsets.leading();

  int trailing_reference_point;
  if (fade_info.next_view) {
    // Since prev/next view is always a view in the start and target layouts, it
    // should also be in the current layout. Therefore this should never return
    // null.
    const ChildLayout* const next_layout =
        FindChildViewInLayout(current_layout_, fade_info.next_view);
    trailing_reference_point =
        Normalize(orientation(), next_layout->bounds).origin_main();
  } else {
    trailing_reference_point =
        Normalize(orientation(), current_layout_.host_size).main();
  }
  trailing_reference_point -= fade_info.offsets.trailing();

  const int new_size = std::min(
      base::ClampRound(scale_percent * fade_info.reference_bounds.size_main()),
      trailing_reference_point - leading_reference_point);

  child_layout.child_view = fade_info.child_view;
  if (new_size > 0 &&
      (scale_from_zero ||
       new_size >=
           Normalize(orientation(), fade_info.child_view->GetMinimumSize())
               .main())) {
    child_layout.visible = true;
    NormalizedRect new_bounds = fade_info.reference_bounds;
    switch (fade_info.fade_type) {
      case LayoutFadeType::kFadingIn:
        new_bounds.set_origin_main(leading_reference_point);
        break;
      case LayoutFadeType::kFadingOut:
        new_bounds.set_origin_main(trailing_reference_point - new_size);
        break;
      case LayoutFadeType::kContinuingFade:
        NOTREACHED();
    }
    new_bounds.set_size_main(new_size);
    child_layout.bounds = Denormalize(orientation(), new_bounds);
  }

  return child_layout;
}

ChildLayout AnimatingLayoutManager::CalculateSlideFade(
    const LayoutFadeInfo& fade_info,
    double scale_percent,
    bool slide_from_leading) const {
  // Fall back to kScaleFromMinimum if there is no edge to slide out from.
  if (!fade_info.prev_view && !fade_info.next_view)
    return CalculateScaleFade(fade_info, scale_percent, false);

  // Slide from the other direction if against the edge of the host view.
  if (slide_from_leading && !fade_info.prev_view)
    slide_from_leading = false;
  else if (!slide_from_leading && !fade_info.next_view)
    slide_from_leading = true;

  NormalizedRect new_bounds = fade_info.reference_bounds;

  // Determine which layout the sliding view will be completely faded in.
  const ProposedLayout* fully_faded_layout;
  switch (fade_info.fade_type) {
    case LayoutFadeType::kFadingIn:
      fully_faded_layout = &starting_layout_;
      break;
    case LayoutFadeType::kFadingOut:
      fully_faded_layout = &target_layout_;
      break;
    case LayoutFadeType::kContinuingFade:
      NOTREACHED();
  }

  if (slide_from_leading) {
    // Get the layout info for the leading child.
    const ChildLayout* const leading_child =
        FindChildViewInLayout(*fully_faded_layout, fade_info.prev_view);

    // This is the right side of the leading control that will eclipse the
    // sliding view at the start/end of the animation.
    const int initial_trailing =
        Normalize(orientation(), leading_child->bounds).max_main();

    // Interpolate between initial and final trailing edge.
    const int new_trailing = gfx::Tween::IntValueBetween(
        scale_percent, initial_trailing, new_bounds.max_main());

    // Adjust the bounding rectangle of the view.
    new_bounds.Offset(new_trailing - new_bounds.max_main(), 0);

  } else {
    // Get the layout info for the trailing child.
    const ChildLayout* const trailing_child =
        FindChildViewInLayout(*fully_faded_layout, fade_info.next_view);

    // This is the left side of the trailing control that will eclipse the
    // sliding view at the start/end of the animation.
    const int initial_leading =
        Normalize(orientation(), trailing_child->bounds).origin_main();

    // Interpolate between initial and final leading edge.
    const int new_leading = gfx::Tween::IntValueBetween(
        scale_percent, initial_leading, new_bounds.origin_main());

    // Adjust the bounding rectangle of the view.
    new_bounds.Offset(new_leading - new_bounds.origin_main(), 0);
  }

  // Actual bounds are a linear interpolation between starting and reference
  // bounds.
  ChildLayout child_layout;
  child_layout.child_view = fade_info.child_view;
  child_layout.visible = true;
  child_layout.bounds = Denormalize(orientation(), new_bounds);

  return child_layout;
}

ChildLayout AnimatingLayoutManager::CalculateFadeAndSlideFade(
    const LayoutFadeInfo& fade_info,
    double scale_percent,
    double opacity_value,
    bool slide_from_leading) const {
  // If not painting to a layer we cannot perform an opacity animation on the
  // view, fall back to just doing a slide animation.
  if (!fade_info.child_view->layer()) {
    return CalculateSlideFade(fade_info, scale_percent, slide_from_leading);
  }

  NormalizedRect new_bounds = fade_info.reference_bounds;

  // Determine which layout the sliding view will be completely faded in.
  const ProposedLayout* fully_faded_layout;
  switch (fade_info.fade_type) {
    case LayoutFadeType::kFadingIn:
      fully_faded_layout = &starting_layout_;
      break;
    case LayoutFadeType::kFadingOut:
      fully_faded_layout = &target_layout_;
      break;
    case LayoutFadeType::kContinuingFade:
      NOTREACHED();
  }

  if (!slide_from_leading) {
    // Find the leading edge of the next child, if there is no next child we use
    // the edge of the host view.
    const ChildLayout* const trailing_child =
        FindChildViewInLayout(*fully_faded_layout, fade_info.next_view);
    const int host_trailing =
        Normalize(orientation(), fully_faded_layout->host_size).main();
    int leading_bound =
        !trailing_child
            ? host_trailing
            : Normalize(orientation(), trailing_child->bounds).origin_main();
    // Interpolate between initial and final leading edge.
    const int new_leading = gfx::Tween::IntValueBetween(
        scale_percent, leading_bound, new_bounds.origin_main());

    new_bounds.Offset(new_leading - new_bounds.origin_main(), 0);
  }

  ChildLayout child_layout;
  child_layout.child_view = fade_info.child_view;
  child_layout.visible = true;
  child_layout.bounds = Denormalize(orientation(), new_bounds);
  fade_info.child_view->layer()->SetOpacity(opacity_value);

  return child_layout;
}

// Returns the space in which to calculate the target layout.
gfx::Size AnimatingLayoutManager::GetAvailableTargetLayoutSize() {
  if (bounds_animation_mode_ == BoundsAnimationMode::kUseHostBounds)
    return host_view()->size();

  const SizeBounds bounds = GetAvailableHostSize();
  last_available_host_size_ = bounds;
  const gfx::Size preferred_size =
      target_layout_manager()->GetPreferredSize(host_view());

  int width;

  if (orientation() == LayoutOrientation::kVertical &&
      bounds_animation_mode_ == BoundsAnimationMode::kAnimateMainAxis) {
    width = host_view()->width();
  } else {
    width = bounds.width().min_of(preferred_size.width());
  }

  int height;

  if (orientation() == LayoutOrientation::kHorizontal &&
      bounds_animation_mode_ == BoundsAnimationMode::kAnimateMainAxis) {
    height = host_view()->height();
  } else {
    height = width < preferred_size.width()
                 ? target_layout_manager()->GetPreferredHeightForWidth(
                       host_view(), width)
                 : preferred_size.height();
    height = bounds.height().min_of(height);
  }

  return gfx::Size(width, height);
}

// static
gfx::Size AnimatingLayoutManager::DefaultFlexRuleImpl(
    const AnimatingLayoutManager* animating_layout,
    const View* view,
    const SizeBounds& size_bounds) {
  DCHECK_EQ(view->GetLayoutManager(), animating_layout);

  // This is the current preferred size, which takes animation into account.
  const gfx::Size preferred_size = animating_layout->GetPreferredSize(view);

  // Does the preferred size fit in the bounds? If so, return the preferred
  // size. Note that the *target* size might not fit in the bounds, but we'll
  // recalculate that the next time we lay out.
  //
  // The one exception is if the current preferred size is empty. If that's the
  // case, then this check becomes trivial and the layout can get stuck at zero
  // size (which is bad). See crbug.com/1506607 for an example of an empty
  // layout causing issues.
  if (!preferred_size.IsEmpty() &&
      CanFitInBounds(preferred_size, size_bounds)) {
    return preferred_size;
  }

  // Special case - if we're being asked for a zero-size layout we'll return the
  // minimum size of the layout. This is because we're being probed for how
  // small we can get, not being asked for an actual size.
  if (GetMainAxis(animating_layout->orientation(), size_bounds) <= 0) {
    return animating_layout->GetMinimumSize(view);
  }

  // We know our current size does not fit into the bounds being given to us.
  // This is going to force a snap to a new size, which will be the ideal size
  // of the target layout in the provided space.
  const LayoutManagerBase* const target_layout =
      animating_layout->target_layout_manager();

  // Easiest case is that the target layout's preferred size *does* fit, in
  // which case we can use that.
  const gfx::Size target_preferred = target_layout->GetPreferredSize(view);
  if (CanFitInBounds(target_preferred, size_bounds)) {
    return target_preferred;
  }

  // We know that at least one of the width and height are constrained, so we
  // need to ask the target layout how large it wants to be in the space
  // provided.
  gfx::Size size;
  if (size_bounds.width().is_bounded() && size_bounds.height().is_bounded()) {
    // Both width and height are specified.  Constraining the width may change
    // the desired height, so we can't just blindly return the minimum in both
    // dimensions.  Instead, query the target layout in the constrained space
    // and return its size.
    size = gfx::Size(size_bounds.width().value(), size_bounds.height().value());
  } else if (size_bounds.width().is_bounded()) {
    // The width is specified and too small.  Use the height-for-width
    // calculation.
    // TODO(dfried): This should be rare, but it is also inefficient. See if we
    // can't add an alternative to GetPreferredHeightForWidth() that actually
    // calculates the layout in this space so we don't have to do it twice.
    const int width = size_bounds.width().value();
    size = gfx::Size(width,
                     target_layout->GetPreferredHeightForWidth(view, width));
  } else {
    DCHECK(size_bounds.height().is_bounded());
    // The height is specified and too small.  Fortunately the height of a
    // layout can't (shouldn't?) affect its width.
    size = gfx::Size(target_preferred.width(), size_bounds.height().value());
  }

  return target_layout->GetProposedLayout(size).host_size;
}

}  // namespace views
