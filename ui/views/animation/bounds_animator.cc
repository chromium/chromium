// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/bounds_animator.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

// This should be after all other #includes.
#if defined(_WINDOWS_)  // Detect whether windows.h was included.
#include "base/win/windows_h_disallowed.h"
#endif  // defined(_WINDOWS_)

namespace views {

BoundsAnimator::BoundsAnimator(View* parent, bool use_transforms)
    : AnimationDelegateViews(parent),
      parent_(parent),
      use_transforms_(use_transforms),
      container_(new gfx::AnimationContainer()) {}

BoundsAnimator::~BoundsAnimator() {
  // Delete all the animations, but don't remove any child views. We assume the
  // view owns us and is going to be deleted anyway. However, deleting a
  // delegate may results in removing a child view, so empty the data_ first so
  // that it won't call AnimationCanceled().
  ViewToDataMap data;
  data.swap(data_);
  for (auto& entry : data)
    CleanupData(false, &entry.second);
}

void BoundsAnimator::AnimateViewTo(
    View* view,
    const gfx::Rect& target,
    std::unique_ptr<gfx::AnimationDelegate> delegate) {
  DCHECK(view);
  DCHECK_EQ(view->parent(), parent_);

  const bool is_animating = IsAnimating(view);

  // Return early if the existing animation on |view| has the same target
  // bounds.
  if (is_animating && target == data_[view].target_bounds) {
    // If this animation specifies a different delegate, swap them out.
    if (delegate && delegate != data_[view].delegate)
      SetAnimationDelegate(view, std::move(delegate));

    return;
  }

  Data existing_data;
  if (is_animating) {
    DCHECK(base::Contains(data_, view));
    const bool used_transforms = data_[view].target_transform.has_value();
    if (used_transforms) {
      // Using transforms means a view does not have the proper bounds until an
      // animation is complete or canceled. So here we cancel the animation so
      // that the bounds can be updated. Note that this means that callers who
      // want to set bounds (i.e. View::SetBoundsRect()) directly before calling
      // this function will have to explicitly call StopAnimatingView() before
      // doing so.
      StopAnimatingView(view);
    } else {
      // Don't immediately delete the animation, that might trigger a callback
      // from the animation container.
      existing_data = RemoveFromMaps(view);
    }
  }

  // NOTE: we don't check if the view is already at the target location. Doing
  // so leads to odd cases where no animations may be present after invoking
  // AnimateViewTo. AnimationProgressed does nothing when the bounds of the
  // view don't change.

  Data& data = data_[view];
  data.start_bounds = view->bounds();
  data.target_bounds = target;
  data.animation = CreateAnimation();
  data.delegate = std::move(delegate);

  // If the start bounds are empty we cannot derive a transform from start to
  // target. Views with existing transforms are not supported. Default back to
  // using the bounds update animation in these cases.
  // Note that transform is not used if bounds animation requires scaling.
  // Because for some views, their children cannot be scaled with the same scale
  // factor. For example, ShelfAppButton's size in normal state and dense state
  // is 56 and 48 respectively while the size of icon image, the child of
  // ShelfAppButton, is 44 and 36 respectively.
  if (use_transforms_ && !data.start_bounds.IsEmpty() &&
      view->GetTransform().IsIdentity() &&
      data.start_bounds.size() == data.target_bounds.size()) {
    // Calculate the target transform. Note that we don't reset the transform if
    // there already was one, otherwise users will end up with visual bounds
    // different than what they set.
    // Note that View::SetTransform() does not handle RTL, which is different
    // from View::SetBounds(). So mirror the start bounds and target bounds
    // manually if necessary.
    const gfx::Transform target_transform = gfx::TransformBetweenRects(
        gfx::RectF(parent_->GetMirroredRect(data.start_bounds)),
        gfx::RectF(parent_->GetMirroredRect(data.target_bounds)));
    data.target_transform = target_transform;
  }

  animation_to_view_[data.animation.get()] = view;

  data.animation->Show();

  CleanupData(true, &existing_data);
}

void BoundsAnimator::SetTargetBounds(View* view, const gfx::Rect& target) {
  const auto i = data_.find(view);
  if (i == data_.end())
    AnimateViewTo(view, target);
  else
    i->second.target_bounds = target;
}

gfx::Rect BoundsAnimator::GetTargetBounds(const View* view) const {
  const auto i = data_.find(view);
  return (i == data_.end()) ? view->bounds() : i->second.target_bounds;
}

const gfx::SlideAnimation* BoundsAnimator::GetAnimationForView(View* view) {
  const auto i = data_.find(view);
  return (i == data_.end()) ? nullptr : i->second.animation.get();
}

void BoundsAnimator::SetAnimationDelegate(
    View* view,
    std::unique_ptr<AnimationDelegate> delegate) {
  const auto i = data_.find(view);
  CHECK(i != data_.end(), base::NotFatalUntil::M130);

  i->second.delegate = std::move(delegate);
}

void BoundsAnimator::StopAnimatingView(View* view) {
  const auto i = data_.find(view);
  if (i != data_.end())
    i->second.animation->Stop();
}

bool BoundsAnimator::IsAnimating(View* view) const {
  return data_.find(view) != data_.end();
}

bool BoundsAnimator::IsAnimating() const {
  return !data_.empty();
}

void BoundsAnimator::Complete() {
  if (data_.empty())
    return;

  while (!data_.empty())
    data_.begin()->second.animation->End();

  // Invoke AnimationContainerProgressed to force a repaint and notify delegate.
  AnimationContainerProgressed(container_.get());
}

void BoundsAnimator::Cancel() {
  if (data_.empty())
    return;

  while (!data_.empty())
    data_.begin()->second.animation->Stop();

  // Invoke AnimationContainerProgressed to force a repaint and notify delegate.
  AnimationContainerProgressed(container_.get());
}

void BoundsAnimator::SetAnimationDuration(base::TimeDelta duration) {
  animation_duration_ = duration;
}

void BoundsAnimator::AddObserver(BoundsAnimatorObserver* observer) {
  observers_.AddObserver(observer);
}

void BoundsAnimator::RemoveObserver(BoundsAnimatorObserver* observer) {
  observers_.RemoveObserver(observer);
}

std::unique_ptr<gfx::SlideAnimation> BoundsAnimator::CreateAnimation() {
  auto animation = std::make_unique<gfx::SlideAnimation>(this);
  animation->SetContainer(container_.get());
  animation->SetSlideDuration(animation_duration_);
  animation->SetTweenType(tween_type_);
  return animation;
}

BoundsAnimator::Data::Data() = default;
BoundsAnimator::Data::Data(Data&&) = default;
BoundsAnimator::Data& BoundsAnimator::Data::operator=(Data&&) = default;
BoundsAnimator::Data::~Data() = default;

BoundsAnimator::Data BoundsAnimator::RemoveFromMaps(View* view) {
  const auto i = data_.find(view);
  CHECK(i != data_.end(), base::NotFatalUntil::M130);
  DCHECK_GT(animation_to_view_.count(i->second.animation.get()), 0u);

  Data old_data = std::move(i->second);
  data_.erase(view);
  animation_to_view_.erase(old_data.animation.get());
  return old_data;
}

void BoundsAnimator::CleanupData(bool send_cancel, Data* data) {
  if (send_cancel && data->delegate)
    data->delegate->AnimationCanceled(data->animation.get());

  data->delegate.reset();

  if (data->animation) {
    data->animation->set_delegate(nullptr);
    data->animation.reset();
  }
}

std::unique_ptr<gfx::Animation> BoundsAnimator::ResetAnimationForView(
    View* view) {
  const auto i = data_.find(view);
  if (i == data_.end())
    return nullptr;

  std::unique_ptr<gfx::Animation> old_animation =
      std::move(i->second.animation);
  animation_to_view_.erase(old_animation.get());
  // Reset the delegate so that we don't attempt any processing when the
  // animation calls us back.
  old_animation->set_delegate(nullptr);
  return old_animation;
}

void BoundsAnimator::AnimationEndedOrCanceled(const gfx::Animation* animation,
                                              AnimationEndType type) {
  DCHECK(animation_to_view_.find(animation) != animation_to_view_.end());

  View* view = animation_to_view_[animation];
  DCHECK(view);

  // Notify the delegate so it has a chance to paint the final state of a
  // completed animation.
  if (type == AnimationEndType::kEnded) {
    DCHECK_EQ(animation->GetCurrentValue(), 1.0);
    AnimationProgressed(animation);
  }

  // Save the data for later clean up.
  Data data = RemoveFromMaps(view);

  if (data.target_transform) {
    if (type == AnimationEndType::kEnded) {
      // Set the bounds at the end of the animation and reset the transform.
      view->SetBoundsRect(data.target_bounds);
    } else {
      DCHECK_EQ(AnimationEndType::kCanceled, type);
      // Get the existing transform and apply it to the start bounds which is
      // the current bounds of the view. This will place the bounds at the place
      // where the animation stopped. See comment in AnimateViewTo() for details
      // as to why GetMirroredRect() is used.
      const gfx::Transform transform = view->GetTransform();
      gfx::Rect bounds = parent_->GetMirroredRect(view->bounds());
      bounds = gfx::ToRoundedRect(transform.MapRect(gfx::RectF(bounds)));
      view->SetBoundsRect(parent_->GetMirroredRect(bounds));
    }
    view->SetTransform(gfx::Transform());
  }

  if (data.delegate) {
    if (type == AnimationEndType::kEnded) {
      data.delegate->AnimationEnded(animation);
    } else {
      DCHECK_EQ(AnimationEndType::kCanceled, type);
      data.delegate->AnimationCanceled(animation);
    }
  }

  CleanupData(false, &data);
}

void BoundsAnimator::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK(animation_to_view_.find(animation) != animation_to_view_.end());

  View* view = animation_to_view_[animation];
  DCHECK(view);
  const Data& data = data_[view];

  if (data.target_transform) {
    const gfx::Transform current_transform = gfx::Tween::TransformValueBetween(
        animation->GetCurrentValue(), gfx::Transform(), *data.target_transform);
    view->SetTransform(current_transform);
  } else {
    gfx::Rect new_bounds =
        animation->CurrentValueBetween(data.start_bounds, data.target_bounds);
    if (new_bounds != view->bounds()) {
      gfx::Rect total_bounds = gfx::UnionRects(new_bounds, view->bounds());

      // Build up the region to repaint in repaint_bounds_. We'll do the repaint
      // when all animations complete (in AnimationContainerProgressed).
      repaint_bounds_.Union(total_bounds);

      view->SetBoundsRect(new_bounds);
    }
  }

  if (data.delegate)
    data.delegate->AnimationProgressed(animation);
}

void BoundsAnimator::AnimationEnded(const gfx::Animation* animation) {
  AnimationEndedOrCanceled(animation, AnimationEndType::kEnded);
}

void BoundsAnimator::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEndedOrCanceled(animation, AnimationEndType::kCanceled);
}

void BoundsAnimator::AnimationContainerProgressed(
    gfx::AnimationContainer* container) {
  if (!repaint_bounds_.IsEmpty()) {
    // Adjust for rtl.
    repaint_bounds_.set_x(parent_->GetMirroredXWithWidthInView(
        repaint_bounds_.x(), repaint_bounds_.width()));
    parent_->SchedulePaintInRect(repaint_bounds_);
    repaint_bounds_.SetRect(0, 0, 0, 0);
  }

  observers_.Notify(&BoundsAnimatorObserver::OnBoundsAnimatorProgressed, this);

  if (!IsAnimating()) {
    // Notify here rather than from AnimationXXX to avoid deleting the animation
    // while the animation is calling us.
    observers_.Notify(&BoundsAnimatorObserver::OnBoundsAnimatorDone, this);
  }
}

void BoundsAnimator::AnimationContainerEmpty(
    gfx::AnimationContainer* container) {}

void BoundsAnimator::OnChildViewRemoved(views::View* observed_view,
                                        views::View* removed) {
  DCHECK_EQ(parent_, observed_view);
  const auto iter = data_.find(removed);
  if (iter == data_.end())
    return;
  AnimationCanceled(iter->second.animation.get());
}

base::TimeDelta BoundsAnimator::GetAnimationDurationForReporting() const {
  return GetAnimationDuration();
}

}  // namespace views
