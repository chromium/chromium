// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/animation_delegate_views.h"

#include <memory>
#include <utility>

#include "ui/gfx/animation/animation_container.h"
#include "ui/views/animation/compositor_animation_runner.h"
#include "ui/views/widget/widget.h"

namespace views {

AnimationDelegateViews::AnimationDelegateViews(View* view,
                                               const base::Location& location)
    : view_(view), location_(location) {
  if (view)
    scoped_observation_.Observe(view);
}

AnimationDelegateViews::~AnimationDelegateViews() {
  // Reset the delegate so that we don't attempt to notify our observer from
  // the destructor.
  if (container_)
    container_->set_observer(nullptr);
}

void AnimationDelegateViews::AnimationContainerWasSet(
    gfx::AnimationContainer* container) {
  if (container_ == container)
    return;

  if (container_)
    container_->set_observer(nullptr);

  container_ = container;
  container_->set_observer(this);
  UpdateAnimationRunner(location_);
}

void AnimationDelegateViews::OnViewAddedToWidget(View* observed_view) {
  UpdateAnimationRunner(location_);
}

void AnimationDelegateViews::OnViewRemovedFromWidget(View* observed_view) {
  ClearAnimationRunner();
}

void AnimationDelegateViews::OnViewIsDeleting(View* observed_view) {
  DCHECK(scoped_observation_.IsObservingSource(view_.get()));
  scoped_observation_.Reset();
  view_ = nullptr;
  UpdateAnimationRunner(location_);
}

void AnimationDelegateViews::AnimationContainerShuttingDown(
    gfx::AnimationContainer* container) {
  container_ = nullptr;
  ClearAnimationRunner();
}

base::TimeDelta AnimationDelegateViews::GetAnimationDurationForReporting()
    const {
  return base::TimeDelta();
}

void AnimationDelegateViews::UpdateAnimationRunner(
    const base::Location& location) {
  if (!view_ || !view_->GetWidget() || !view_->GetWidget()->GetCompositor()) {
    ClearAnimationRunner();
    return;
  }

  if (!container_ || container_->has_custom_animation_runner())
    return;

  auto compositor_animation_runner =
      std::make_unique<CompositorAnimationRunner>(view_->GetWidget(), location);
  compositor_animation_runner_ = compositor_animation_runner.get();
  container_->SetAnimationRunner(std::move(compositor_animation_runner));
}

void AnimationDelegateViews::ClearAnimationRunner() {
  // `compositor_animation_runner_` holds a pointer owned by `container_`, so
  // we need to release it before `container_` actually releases the memory it
  // points to.
  compositor_animation_runner_ = nullptr;
  // TODO(crbug.com/41457352): make sure the container has a correct
  // compositor-assisted runner.
  if (container_)
    container_->SetAnimationRunner(nullptr);
}

}  // namespace views
