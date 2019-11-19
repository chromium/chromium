// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/animating_layout_manager.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/layout/normalized_geometry.h"
#include "ui/views/view.h"

namespace views {

namespace {

int GetMainAxis(LayoutOrientation orientation, const gfx::Size& size) {
  switch (orientation) {
    case LayoutOrientation::kHorizontal:
      return size.width();
    case LayoutOrientation::kVertical:
      return size.height();
  }
}

}  // anonymous namespace

// Holds data about a view that is fading in or out as part of an animation.
struct AnimatingLayoutManager::LayoutFadeInfo {
  // Whether the view is fading in or out.
  bool fading_in = false;
  // The child view which is fading.
  View* child_view = nullptr;
  // The view previous (leading side) to the fading view which is in both the
  // starting and target layout, or null if none.
  View* prev_view = nullptr;
  // The view next (trailing side) to the fading view which is in both the
  // starting and target layout, or null if none.
  View* next_view = nullptr;
  // The full-size bounds, normalized to the orientation of the layout manaer,
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
      if (animation_delegate_->scoped_observer_.IsObserving(observed_view))
        animation_delegate_->scoped_observer_.Remove(observed_view);
    }

   private:
    AnimationDelegate* const animation_delegate_;
  };
  friend class Observer;

  // AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  bool ready_to_animate_ = false;
  bool resetting_animation_ = false;
  AnimatingLayoutManager* const target_layout_manager_;
  std::unique_ptr<gfx::SlideAnimation> animation_;
  ViewWidgetObserver view_widget_observer_{this};
  ScopedObserver<View, ViewObserver> scoped_observer_{&view_widget_observer_};
};

AnimatingLayoutManager::AnimationDelegate::AnimationDelegate(
    AnimatingLayoutManager* layout_manager)
    : AnimationDelegateViews(layout_manager->host_view()),
      target_layout_manager_(layout_manager),
      animation_(std::make_unique<gfx::SlideAnimation>(this)) {
  animation_->SetContainer(new gfx::AnimationContainer());
  View* const host_view = layout_manager->host_view();
  DCHECK(host_view);
  if (host_view->GetWidget())
    MakeReadyForAnimation();
  else
    scoped_observer_.Add(host_view);
  UpdateAnimationParameters();
}

void AnimatingLayoutManager::AnimationDelegate::UpdateAnimationParameters() {
  animation_->SetTweenType(target_layout_manager_->tween_type());
  animation_->SetSlideDuration(target_layout_manager_->animation_duration());
}

void AnimatingLayoutManager::AnimationDelegate::Animate() {
  DCHECK(ready_to_animate_);
  Reset();
  animation_->Show();
}

void AnimatingLayoutManager::AnimationDelegate::Reset() {
  if (!ready_to_animate_)
    return;
  base::AutoReset<bool> setter(&resetting_animation_, true);
  animation_->Reset();
}

void AnimatingLayoutManager::AnimationDelegate::MakeReadyForAnimation() {
  if (!ready_to_animate_) {
    target_layout_manager_->ResetLayout();
    target_layout_manager_->InvalidateHost(false);
    ready_to_animate_ = true;
    if (scoped_observer_.IsObserving(target_layout_manager_->host_view()))
      scoped_observer_.Remove(target_layout_manager_->host_view());
  }
}

void AnimatingLayoutManager::AnimationDelegate::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK(animation_.get() == animation);
  target_layout_manager_->AnimateTo(animation->GetCurrentValue());
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
  target_layout_manager_->AnimateTo(1.0);
}

// AnimatingLayoutManager:

AnimatingLayoutManager::AnimatingLayoutManager() {}
AnimatingLayoutManager::~AnimatingLayoutManager() = default;

AnimatingLayoutManager& AnimatingLayoutManager::SetShouldAnimateBounds(
    bool should_animate_bounds) {
  if (should_animate_bounds_ != should_animate_bounds) {
    should_animate_bounds_ = should_animate_bounds;
    ResetLayout();
    InvalidateHost(false);
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

AnimatingLayoutManager& AnimatingLayoutManager::SetOrientation(
    LayoutOrientation orientation) {
  if (orientation_ != orientation) {
    orientation_ = orientation;
    ResetLayout();
    InvalidateHost(false);
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

  const gfx::Size target_size =
      should_animate_bounds_
          ? target_layout_manager()->GetPreferredSize(host_view())
          : host_view()->size();

  ResetLayoutToSize(target_size);
}

void AnimatingLayoutManager::ResetLayoutToSize(const gfx::Size& target_size) {
  if (animation_delegate_)
    animation_delegate_->Reset();

  target_layout_ = target_layout_manager()->GetProposedLayout(target_size);
  current_layout_ = target_layout_;
  starting_layout_ = current_layout_;
  fade_infos_.clear();
  current_offset_ = 1.0;
  set_cached_layout_size(target_size);

  if (is_animating_)
    OnAnimationEnded();
}

void AnimatingLayoutManager::OnAnimationEnded() {
  DCHECK(is_animating_);
  is_animating_ = false;
  RunDelayedActions();
  DCHECK(!is_animating_) << "Queued actions should not change animation state.";
  NotifyIsAnimatingChanged();
}

void AnimatingLayoutManager::FadeOut(View* child_view) {
  DCHECK(child_view);
  DCHECK(child_view->parent());
  DCHECK_EQ(host_view(), child_view->parent());

  // Indicate that the view should become hidden in the layout without
  // immediately changing its visibility. Instead, this triggers an animation
  // which results in the view being hidden.
  //
  // This method is typically only called from View and has a private final
  // implementation in LayoutManagerBase so we have to cast to call it.
  static_cast<LayoutManager*>(this)->ViewVisibilitySet(host_view(), child_view,
                                                       false);
}

void AnimatingLayoutManager::FadeIn(View* child_view) {
  DCHECK(child_view);
  DCHECK(child_view->parent());
  DCHECK_EQ(host_view(), child_view->parent());
  // Indicate that the view should become visible in the layout without
  // immediately changing its visibility. Instead, this triggers an animation
  // which results in the view being shown.
  //
  // This method is typically only called from View and has a private final
  // implementation in LayoutManagerBase so we have to cast to call it.
  static_cast<LayoutManager*>(this)->ViewVisibilitySet(host_view(), child_view,
                                                       true);
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
  return should_animate_bounds_
             ? current_layout_.host_size
             : target_layout_manager()->GetPreferredSize(host);
}

gfx::Size AnimatingLayoutManager::GetMinimumSize(const View* host) const {
  if (!target_layout_manager())
    return gfx::Size();
  // TODO(dfried): consider cases where the minimum size might not be just the
  // minimum size of the embedded layout.
  gfx::Size minimum_size = target_layout_manager()->GetMinimumSize(host);
  if (should_animate_bounds_)
    minimum_size.SetToMin(current_layout_.host_size);
  return minimum_size;
}

int AnimatingLayoutManager::GetPreferredHeightForWidth(const View* host,
                                                       int width) const {
  if (!target_layout_manager())
    return 0;
  // TODO(dfried): revisit this computation.
  return should_animate_bounds_
             ? current_layout_.host_size.height()
             : target_layout_manager()->GetPreferredHeightForWidth(host, width);
}

std::vector<View*> AnimatingLayoutManager::GetChildViewsInPaintOrder(
    const View* host) const {
  DCHECK_EQ(host_view(), host);

  if (!is_animating())
    return LayoutManagerBase::GetChildViewsInPaintOrder(host);

  std::vector<View*> result;
  std::set<View*> fading;

  // Put all fading views to the front of the list (back of the Z-order).
  for (const LayoutFadeInfo& fade_info : fade_infos_) {
    result.push_back(fade_info.child_view);
    fading.insert(fade_info.child_view);
  }

  // Add the result of the views.
  for (View* child : host->children()) {
    if (!base::Contains(fading, child))
      result.push_back(child);
  }

  return result;
}

void AnimatingLayoutManager::QueueDelayedAction(DelayedAction delayed_action) {
  DCHECK(is_animating());
  delayed_actions_.emplace_back(std::move(delayed_action));
}

void AnimatingLayoutManager::RunOrQueueAction(DelayedAction action) {
  if (!is_animating())
    std::move(action).Run();
  else
    QueueDelayedAction(std::move(action));
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
  return ProposedLayout();
}

void AnimatingLayoutManager::OnInstalled(View* host) {
  DCHECK(!animation_delegate_);
  animation_delegate_ = std::make_unique<AnimationDelegate>(this);
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
  if (should_animate_bounds_) {
    const int host_main = GetMainAxis(orientation(), host_size);
    const int desired_main =
        GetMainAxis(orientation(), current_layout_.host_size);
    if (desired_main > host_main)
      ResetLayoutToSize(host_size);
  } else if (!cached_layout_size() || host_size != *cached_layout_size()) {
    ResetLayout();
  }

  ApplyLayout(current_layout_);

  // Send animating stopped events on layout so the current layout during the
  // event represents the final state instead of an intermediate state.
  if (is_animating_ && current_offset_ == 1.0)
    OnAnimationEnded();
}

bool AnimatingLayoutManager::RecalculateTarget() {
  constexpr double kResetAnimationThreshold = 0.8;

  if (!target_layout_manager())
    return false;

  if (!cached_layout_size() || !animation_delegate_ ||
      !animation_delegate_->ready_to_animate()) {
    ResetLayout();
    return true;
  }

  const gfx::Size target_size =
      should_animate_bounds_
          ? target_layout_manager()->GetPreferredSize(host_view())
          : host_view()->size();

  // For layouts that are confined to available space, changing the available
  // space causes a fresh layout, not an animation.
  // TODO(dfried): define a way for views to animate into and out of empty space
  // as adjacent child views appear/disappear. This will be useful in animating
  // tab titles, which currently slide over when the favicon disappears.
  if (!should_animate_bounds_ && *cached_layout_size() != target_size) {
    ResetLayout();
    return true;
  }

  set_cached_layout_size(target_size);

  // If there has been no appreciable change in layout, there's no reason to
  // start or update an animation.
  const ProposedLayout proposed_layout =
      target_layout_manager()->GetProposedLayout(target_size);
  if (target_layout_ == proposed_layout)
    return false;

  starting_layout_ = current_layout_;
  target_layout_ = proposed_layout;
  if (current_offset_ > kResetAnimationThreshold) {
    starting_offset_ = 0.0;
    current_offset_ = 0.0;
    animation_delegate_->Animate();
    if (!is_animating_) {
      is_animating_ = true;
      NotifyIsAnimatingChanged();
    }
  } else {
    starting_offset_ = current_offset_;
  }
  CalculateFadeInfos();
  UpdateCurrentLayout(0.0);
  return true;
}

void AnimatingLayoutManager::AnimateTo(double value) {
  DCHECK_GE(value, 0.0);
  DCHECK_LE(value, 1.0);
  DCHECK_GE(value, starting_offset_);
  DCHECK_GE(value, current_offset_);
  if (current_offset_ == value)
    return;
  current_offset_ = value;
  const double percent =
      (current_offset_ - starting_offset_) / (1.0 - starting_offset_);
  UpdateCurrentLayout(percent);
  InvalidateHost(false);
}

void AnimatingLayoutManager::NotifyIsAnimatingChanged() {
  for (auto& observer : observers_)
    observer.OnLayoutIsAnimatingChanged(this, is_animating());
}

void AnimatingLayoutManager::RunDelayedActions() {
  for (auto& action : delayed_actions_)
    std::move(action).Run();
  delayed_actions_.clear();
}

void AnimatingLayoutManager::UpdateCurrentLayout(double percent) {
  // This drops out any child view elements that don't exist in the target
  // layout. We'll add them back in later.
  current_layout_ =
      ProposedLayoutBetween(percent, starting_layout_, target_layout_);

  if (default_fade_mode_ == FadeInOutMode::kHide || fade_infos_.empty())
    return;

  std::map<const View*, size_t> view_indices;
  for (size_t i = 0; i < current_layout_.child_layouts.size(); ++i)
    view_indices.emplace(current_layout_.child_layouts[i].child_view, i);

  for (const LayoutFadeInfo& fade_info : fade_infos_) {
    // This shouldn't happen but we should ensure that with a check.
    DCHECK_NE(-1, host_view()->GetIndexOf(fade_info.child_view));

    ChildLayout child_layout;

    if (percent == 1.0) {
      // At the end of the animation snap to the final state of the child view.
      child_layout.child_view = fade_info.child_view;
      if (fade_info.fading_in) {
        child_layout.visible = true;
        child_layout.bounds =
            Denormalize(orientation(), fade_info.reference_bounds);
      } else {
        child_layout.visible = false;
      }

    } else {
      const double scale_percent =
          fade_info.fading_in ? percent : 1.0 - percent;

      const base::Optional<size_t> prev_index =
          fade_info.prev_view
              ? base::make_optional(view_indices[fade_info.prev_view])
              : base::nullopt;
      const base::Optional<size_t> next_index =
          fade_info.next_view
              ? base::make_optional(view_indices[fade_info.next_view])
              : base::nullopt;

      switch (default_fade_mode_) {
        case FadeInOutMode::kHide:
          NOTREACHED();
          break;
        case FadeInOutMode::kScaleFromMinimum:
          child_layout = CalculateScaleFade(fade_info, prev_index, next_index,
                                            scale_percent,
                                            /* scale_from_zero */ false);
          break;
        case FadeInOutMode::kScaleFromZero:
          child_layout = CalculateScaleFade(fade_info, prev_index, next_index,
                                            scale_percent,
                                            /* scale_from_zero */ true);
          break;
        case FadeInOutMode::kSlideFromLeadingEdge:
          child_layout = CalculateSlideFade(fade_info, prev_index, next_index,
                                            scale_percent,
                                            /* scale_from_zero */ true);
          break;
        case FadeInOutMode::kSlideFromTrailingEdge:
          child_layout = CalculateSlideFade(fade_info, prev_index, next_index,
                                            scale_percent,
                                            /* scale_from_zero */ false);
          break;
      }
    }

    auto it = view_indices.find(fade_info.child_view);
    if (it == view_indices.end())
      current_layout_.child_layouts.push_back(child_layout);
    else
      current_layout_.child_layouts[it->second] = child_layout;
  }
}

void AnimatingLayoutManager::CalculateFadeInfos() {
  fade_infos_.clear();

  struct ChildInfo {
    base::Optional<size_t> start;
    NormalizedRect start_bounds;
    bool start_visible = false;
    base::Optional<size_t> target;
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
    if (index.start_visible && index.target_visible) {
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
      fade_info.fading_in = false;
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
      fade_info.fading_in = true;
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
    }
  }
}

ChildLayout AnimatingLayoutManager::CalculateScaleFade(
    const LayoutFadeInfo& fade_info,
    base::Optional<size_t> prev_index,
    base::Optional<size_t> next_index,
    double scale_percent,
    bool scale_from_zero) const {
  ChildLayout child_layout;

  int leading_reference_point = 0;
  if (prev_index) {
    const auto& prev_bounds = current_layout_.child_layouts[*prev_index].bounds;
    leading_reference_point = Normalize(orientation(), prev_bounds).max_main();
  }
  leading_reference_point += fade_info.offsets.leading();

  int trailing_reference_point;
  if (next_index) {
    const auto& next_bounds = current_layout_.child_layouts[*next_index].bounds;
    trailing_reference_point =
        Normalize(orientation(), next_bounds).origin_main();
  } else {
    trailing_reference_point =
        Normalize(orientation(), current_layout_.host_size).main();
  }
  trailing_reference_point -= fade_info.offsets.trailing();

  const int new_size =
      std::min(int{scale_percent * fade_info.reference_bounds.size_main()},
               trailing_reference_point - leading_reference_point);

  child_layout.child_view = fade_info.child_view;
  if (new_size > 0 &&
      (scale_from_zero ||
       new_size >=
           Normalize(orientation(), fade_info.child_view->GetMinimumSize())
               .main())) {
    child_layout.visible = true;
    NormalizedRect new_bounds = fade_info.reference_bounds;
    if (fade_info.fading_in) {
      new_bounds.set_origin_main(leading_reference_point);
    } else {
      new_bounds.set_origin_main(trailing_reference_point - new_size);
    }
    new_bounds.set_size_main(new_size);
    child_layout.bounds = Denormalize(orientation(), new_bounds);
  }

  return child_layout;
}

ChildLayout AnimatingLayoutManager::CalculateSlideFade(
    const LayoutFadeInfo& fade_info,
    base::Optional<size_t> prev_index,
    base::Optional<size_t> next_index,
    double scale_percent,
    bool slide_from_leading) const {
  // Fall back to kScaleFromMinimum if there is no edge to slide out from.
  if (!prev_index.has_value() && !next_index.has_value()) {
    return CalculateScaleFade(fade_info, prev_index, next_index, scale_percent,
                              false);
  }

  // Slide from the other direction if against the edge of the host view.
  if (slide_from_leading && !prev_index.has_value())
    slide_from_leading = false;
  else if (!slide_from_leading && !next_index.has_value())
    slide_from_leading = true;

  NormalizedRect new_bounds = fade_info.reference_bounds;

  if (slide_from_leading) {
    // This is the right side of the leading control that will eclipse the
    // sliding view at the start/end of the animation.
    const int initial_trailing =
        Normalize(orientation(),
                  (fade_info.fading_in ? starting_layout_ : target_layout_)
                      .child_layouts[*prev_index]
                      .bounds)
            .max_main();

    // Interpolate between initial and final trailing edge.
    const int new_trailing = gfx::Tween::IntValueBetween(
        scale_percent, initial_trailing, new_bounds.max_main());

    // Adjust the bounding rectangle of the view.
    new_bounds.Offset(new_trailing - new_bounds.max_main(), 0);

  } else {
    // This is the left side of the trailing control that will eclipse the
    // sliding view at the start/end of the animation.
    const int initial_leading =
        Normalize(orientation(),
                  (fade_info.fading_in ? starting_layout_ : target_layout_)
                      .child_layouts[*next_index]
                      .bounds)
            .origin_main();

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

}  // namespace views
