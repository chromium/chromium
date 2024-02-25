// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_ANIMATION_ABORT_HANDLE_H_
#define UI_VIEWS_ANIMATION_ANIMATION_ABORT_HANDLE_H_

#include <set>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/compositor/layer_observer.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/views_export.h"

namespace ui {
class Layer;
}  // namespace ui

namespace views {

// A handle that aborts associated animations on destruction.
// Caveat: ALL properties will be aborted on handle destruction,
// including those not initiated by the builder.
class VIEWS_EXPORT AnimationAbortHandle : public ui::LayerObserver {
 public:
  ~AnimationAbortHandle() override;

  void OnObserverDeleted();

 private:
  friend class AnimationBuilder;
  FRIEND_TEST_ALL_PREFIXES(AnimationBuilderTest, AbortHandle);

  enum class AnimationState { kNotStarted, kRunning, kEnded };

  explicit AnimationAbortHandle(AnimationBuilder::Observer* observer);

  // Called when an animation is created for `layer`.
  void AddLayer(ui::Layer* layer);

  void OnAnimationStarted();
  void OnAnimationEnded();

  // ui::LayerObserver:
  void LayerDestroyed(ui::Layer* layer) override;

  AnimationState animation_state() const { return animation_state_; }

  raw_ptr<AnimationBuilder::Observer, DanglingUntriaged> observer_;
  AnimationState animation_state_ = AnimationState::kNotStarted;

  // Stores the layers tracked by the animation abort handle.
  std::set<raw_ptr<ui::Layer, SetExperimental>> tracked_layers_;

  // Stores the layers that are deleted during tracking.
  std::set<raw_ptr<ui::Layer, SetExperimental>> deleted_layers_;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_ANIMATION_ABORT_HANDLE_H_
