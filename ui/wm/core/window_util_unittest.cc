// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/window_util.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/wm/core/window_properties.h"

namespace wm {
namespace {

class TestImplicitAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  TestImplicitAnimationObserver() = default;
  TestImplicitAnimationObserver(const TestImplicitAnimationObserver&) = delete;
  TestImplicitAnimationObserver& operator=(
      const TestImplicitAnimationObserver&) = delete;
  ~TestImplicitAnimationObserver() override = default;

  bool completed() const { return completed_; }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override { completed_ = true; }

 private:
  bool completed_ = false;
};

}  // namespace

using WindowUtilTest = aura::test::AuraTestBase;

// Test if the recreate layers does not recreate layers that have
// already been acquired.
TEST_F(WindowUtilTest, RecreateLayers) {
  std::unique_ptr<aura::Window> window1 =
      aura::test::CreateTestWindow({.bounds = {100, 100}, .window_id = 0});
  std::unique_ptr<aura::Window> window11 = aura::test::CreateTestWindow(
      {.parent = window1.get(), .bounds = {100, 100}, .window_id = 1});
  std::unique_ptr<aura::Window> window12 = aura::test::CreateTestWindow(
      {.parent = window1.get(), .bounds = {100, 100}, .window_id = 2});

  ASSERT_EQ(2u, window1->layer()->children().size());

  std::unique_ptr<ui::Layer> acquired(window11->AcquireLayer());
  EXPECT_TRUE(acquired.get());
  EXPECT_EQ(acquired.get(), window11->layer());

  std::unique_ptr<ui::LayerTreeOwner> tree = wm::RecreateLayers(window1.get());

  // The detached layer should not have the layer that has
  // already been detached.
  ASSERT_EQ(1u, tree->root()->children().size());
  // Child layer is new instance.
  EXPECT_NE(window11->layer(), tree->root()->children()[0]);
  EXPECT_NE(window12->layer(), tree->root()->children()[0]);

  // The original window should have both.
  ASSERT_EQ(2u, window1->layer()->children().size());
  EXPECT_EQ(window11->layer(), window1->layer()->children()[0]);
  EXPECT_EQ(window12->layer(), window1->layer()->children()[1]);

  // Delete the window before the acquired layer is deleted.
  window11.reset();
}

// Tests that an implicit animation observer is notified if the layers are
// recreated while a window is animating and the old layers are destroyed
// before the animation completes.
TEST_F(WindowUtilTest, RecreateLayersDuringAnimation) {
  std::unique_ptr<aura::Window> window1 =
      aura::test::CreateTestWindow({.bounds = {100, 100}, .window_id = 0});
  std::unique_ptr<aura::Window> window11 = aura::test::CreateTestWindow(
      {.parent = window1.get(), .bounds = {100, 100}, .window_id = 1});

  gfx::ScopedAnimationDurationScaleMode non_zero_duration(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  TestImplicitAnimationObserver observer;
  {
    ui::ScopedLayerAnimationSettings settings(window11->layer()->GetAnimator());
    settings.SetTransitionDuration(base::Seconds(1));
    settings.AddObserver(&observer);
    window11->layer()->SetOpacity(0.f);
  }
  ASSERT_TRUE(window11->layer()->GetAnimator()->is_animating());

  // The animation stays on the old layer.
  std::unique_ptr<ui::LayerTreeOwner> tree = wm::RecreateLayers(window1.get());
  EXPECT_FALSE(window11->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(observer.completed());

  tree.reset();
  EXPECT_TRUE(observer.completed());
}

// Test if map_func is correctly executed in RecreateLayerWithClosure.
TEST_F(WindowUtilTest, RecreateLayersWithClosure) {
  std::unique_ptr<aura::Window> window1 =
      aura::test::CreateTestWindow({.bounds = {100, 100}, .window_id = 0});
  std::unique_ptr<aura::Window> window11 = aura::test::CreateTestWindow(
      {.parent = window1.get(), .bounds = {100, 100}, .window_id = 1});
  std::unique_ptr<aura::Window> window12 = aura::test::CreateTestWindow(
      {.parent = window1.get(), .bounds = {100, 100}, .window_id = 2});

  ASSERT_EQ(2u, window1->layer()->children().size());

  auto tree_empty = wm::RecreateLayersWithClosure(
      window1.get(),
      base::BindRepeating(
          [](const aura::Window* to_filter,
             ui::LayerOwner* owner) -> std::unique_ptr<ui::Layer> {
            if (owner->layer() == to_filter->layer())
              return nullptr;
            return owner->RecreateLayer();
          },
          window1.get()));

  // The root is filtered. RecreateLayersWithClosure should return nullptr.
  ASSERT_FALSE(tree_empty);

  auto tree = wm::RecreateLayersWithClosure(
      window1.get(),
      base::BindRepeating(
          [](const aura::Window* to_filter,
             ui::LayerOwner* owner) -> std::unique_ptr<ui::Layer> {
            if (owner->layer() == to_filter->layer())
              return nullptr;
            return owner->RecreateLayer();
          },
          window12.get()));

  ASSERT_TRUE(tree);
  // window12 is filtered out in the above recreation logic.
  ASSERT_EQ(1u, tree->root()->children().size());
  // Child layer is new instance.
  EXPECT_NE(window11->layer(), tree->root()->children()[0]);

  // The original window should have both.
  ASSERT_EQ(2u, window1->layer()->children().size());
  EXPECT_EQ(window11->layer(), window1->layer()->children()[0]);
  EXPECT_EQ(window12->layer(), window1->layer()->children()[1]);
}

}  // namespace wm
