// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_ANIMATION_DELEGATE_VIEWS_H_
#define UI_VIEWS_ANIMATION_ANIMATION_DELEGATE_VIEWS_H_

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/gfx/animation/animation_container_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace views {
class CompositorAnimationRunner;

// Provides default implementation to adapt CompositorAnimationRunner for
// Animation. Falls back to the default animation runner when |view| is nullptr.
class VIEWS_EXPORT AnimationDelegateViews
    : public gfx::AnimationDelegate,
      public ViewObserver,
      public gfx::AnimationContainerObserver {
 public:
  explicit AnimationDelegateViews(View* view,
                                  const base::Location& location = FROM_HERE);
  ~AnimationDelegateViews() override;

  // gfx::AnimationDelegate:
  void AnimationContainerWasSet(gfx::AnimationContainer* container) override;

  // ViewObserver:
  void OnViewAddedToWidget(View* observed_view) final;
  void OnViewRemovedFromWidget(View* observed_view) final;
  void OnViewIsDeleting(View* observed_view) final;

  // gfx::AnimationContainerObserver:
  void AnimationContainerProgressed(
      gfx::AnimationContainer* container) override {}
  void AnimationContainerEmpty(gfx::AnimationContainer* container) override {}
  void AnimationContainerShuttingDown(
      gfx::AnimationContainer* container) override;

  // Returns the expected animation duration for metrics reporting purposes.
  // Should be overriden to provide a non-zero value and used with
  // |set_animation_metrics_reporter()|.
  virtual base::TimeDelta GetAnimationDurationForReporting() const;

  gfx::AnimationContainer* container() { return container_; }

  const base::Location& location_for_test() const { return location_; }

 private:
  // Sets CompositorAnimationRunner to |container_| if possible. Otherwise,
  // clears AnimationRunner of |container_|.
  void UpdateAnimationRunner(const base::Location& location);
  void ClearAnimationRunner();

  raw_ptr<View> view_;
  raw_ptr<gfx::AnimationContainer> container_ = nullptr;

  // Code location of where this is created.
  const base::Location location_;

  // The animation runner that |container_| uses.
  raw_ptr<CompositorAnimationRunner> compositor_animation_runner_ = nullptr;

  base::ScopedObservation<View, ViewObserver> scoped_observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_ANIMATION_DELEGATE_VIEWS_H_
