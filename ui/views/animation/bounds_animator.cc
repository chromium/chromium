// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/bounds_animator.h"

#include <memory>

#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

BoundsAnimator::BoundsAnimator(View* parent)
    : AnimationDelegateViews(parent),
      parent_(parent),
      container_(new gfx::AnimationContainer()) {
}

BoundsAnimator::~BoundsAnimator() {
  // Delete all the animations, but don't remove any child views. We assume the
  // view owns us and is going to be deleted anyway.
  for (auto& entry : data_)
    CleanupData(false, &entry.second);
}

void BoundsAnimator::AnimateViewTo(
    View* view,
    const gfx::Rect& target,
    std::unique_ptr<gfx::AnimationDelegate> delegate) {
  DCHECK(view);
  DCHECK_EQ(view->parent(), parent_);

  Data existing_data;

  if (IsAnimating(view)) {
    // Don't immediately delete the animation, that might trigger a callback
    // from the animation container.
    existing_data = RemoveFromMaps(view);
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

void BoundsAnimator::SetAnimationForView(
    View* view,
    std::unique_ptr<gfx::SlideAnimation> animation) {
  DCHECK(animation);

  const auto i = data_.find(view);
  if (i == data_.end())
    return;

  // We delay deleting the animation until the end so that we don't prematurely
  // send out notification that we're done.
  std::unique_ptr<gfx::Animation> old_animation = ResetAnimationForView(view);

  gfx::SlideAnimation* animation_ptr = animation.get();
  i->second.animation = std::move(animation);
  animation_to_view_[animation_ptr] = view;

  animation_ptr->set_delegate(this);
  animation_ptr->SetContainer(container_.get());
  animation_ptr->Show();
}

const gfx::SlideAnimation* BoundsAnimator::GetAnimationForView(View* view) {
  const auto i = data_.find(view);
  return (i == data_.end()) ? nullptr : i->second.animation.get();
}

void BoundsAnimator::SetAnimationDelegate(
    View* view,
    std::unique_ptr<AnimationDelegate> delegate) {
  const auto i = data_.find(view);
  DCHECK(i != data_.end());

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
  DCHECK(i != data_.end());
  DCHECK(animation_to_view_.count(i->second.animation.get()) > 0);

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

  // Save the data for later clean up.
  Data data = RemoveFromMaps(view);

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
  gfx::Rect new_bounds =
      animation->CurrentValueBetween(data.start_bounds, data.target_bounds);
  if (new_bounds != view->bounds()) {
    gfx::Rect total_bounds = gfx::UnionRects(new_bounds, view->bounds());

    // Build up the region to repaint in repaint_bounds_. We'll do the repaint
    // when all animations complete (in AnimationContainerProgressed).
    repaint_bounds_.Union(total_bounds);

    view->SetBoundsRect(new_bounds);
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

  for (BoundsAnimatorObserver& observer : observers_)
    observer.OnBoundsAnimatorProgressed(this);

  if (!IsAnimating()) {
    // Notify here rather than from AnimationXXX to avoid deleting the animation
    // while the animation is calling us.
    for (BoundsAnimatorObserver& observer : observers_)
      observer.OnBoundsAnimatorDone(this);
  }
}

void BoundsAnimator::AnimationContainerEmpty(
    gfx::AnimationContainer* container) {}

}  // namespace views
