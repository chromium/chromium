// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_tree_owner.h"

#include <memory>
#include <utility>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_textured.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"

namespace ui {
namespace {

class TestImplicitAnimationObserver : public ImplicitAnimationObserver {
 public:
  TestImplicitAnimationObserver() = default;
  TestImplicitAnimationObserver(const TestImplicitAnimationObserver&) = delete;
  TestImplicitAnimationObserver& operator=(
      const TestImplicitAnimationObserver&) = delete;
  ~TestImplicitAnimationObserver() override = default;

  bool completed() const { return completed_; }

  // ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override { completed_ = true; }

 private:
  bool completed_ = false;
};

}  // namespace

// Tests that implicit animation observers of a layer in the tree are notified
// when the tree is destroyed while the layer is still animating.
TEST(LayerTreeOwnerTest, CompletesAnimationsOnDestruction) {
  gfx::ScopedAnimationDurationScaleMode non_zero_duration(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<Layer> root = std::make_unique<LayerTextured>();
  // `child` is owned by the LayerTreeOwner once `root` is passed to it.
  Layer* child = new LayerTextured();
  root->Add(child);

  TestImplicitAnimationObserver observer;
  {
    ScopedLayerAnimationSettings settings(child->GetAnimator());
    settings.SetTransitionDuration(base::Seconds(1));
    settings.AddObserver(&observer);
    child->SetBounds({100, 100});
  }
  ASSERT_TRUE(child->GetAnimator()->is_animating());

  auto owner = std::make_unique<LayerTreeOwner>(std::move(root));
  EXPECT_FALSE(observer.completed());

  child = nullptr;
  owner.reset();
  EXPECT_TRUE(observer.completed());
}

}  // namespace ui
