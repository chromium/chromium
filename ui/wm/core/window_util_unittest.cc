// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/window_util.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/time/time.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/wm/core/window_properties.h"

namespace wm {

namespace {

int dump_count = 0;
void FakeDumpWithoutCrashing() {
  ++dump_count;
}

}  // namespace

using WindowUtilTest = aura::test::AuraTestBase;

// Test if the recreate layers does not recreate layers that have
// already been acquired.
TEST_F(WindowUtilTest, RecreateLayers) {
  std::unique_ptr<aura::Window> window1(
      aura::test::CreateTestWindowWithId(0, NULL));
  std::unique_ptr<aura::Window> window11(
      aura::test::CreateTestWindowWithId(1, window1.get()));
  std::unique_ptr<aura::Window> window12(
      aura::test::CreateTestWindowWithId(2, window1.get()));

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

// Test if map_func is correctly executed in RecreateLayerWithClosure.
TEST_F(WindowUtilTest, RecreateLayersWithClosure) {
  std::unique_ptr<aura::Window> window1(
      aura::test::CreateTestWindowWithId(0, NULL));
  std::unique_ptr<aura::Window> window11(
      aura::test::CreateTestWindowWithId(1, window1.get()));
  std::unique_ptr<aura::Window> window12(
      aura::test::CreateTestWindowWithId(2, window1.get()));

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

TEST_F(WindowUtilTest, NoMutationAfterCopy) {
  base::debug::SetDumpWithoutCrashingFunction(&FakeDumpWithoutCrashing);
  base::ScopedMockClockOverride clock;

  std::unique_ptr<aura::Window> window1(
      aura::test::CreateTestWindowWithId(0, nullptr));
  std::unique_ptr<aura::Window> window11(
      aura::test::CreateTestWindowWithId(1, window1.get()));

  // Add and SetMaskLayer on `window1` and `window11` works before
  // RecreateLayers.
  {
    std::unique_ptr<ui::Layer> layer = std::make_unique<ui::Layer>();
    window1->layer()->Add(layer.get());

    std::unique_ptr<ui::Layer> mask_layer = std::make_unique<ui::Layer>();
    window1->layer()->SetMaskLayer(layer.get());

    std::unique_ptr<ui::Layer> child_layer = std::make_unique<ui::Layer>();
    window11->layer()->Add(child_layer.get());

    std::unique_ptr<ui::Layer> child_mask_layer = std::make_unique<ui::Layer>();
    window11->layer()->SetMaskLayer(child_mask_layer.get());
  }

  std::unique_ptr<ui::LayerTreeOwner> tree = wm::RecreateLayers(window1.get());

  // Add and SetMaskLayer on `window1` and `window11` crashes after
  // RecreateLayers.
  {
    ASSERT_EQ(dump_count, 0);

    ui::Layer* window1_old_layer = tree->root();
    std::unique_ptr<ui::Layer> layer = std::make_unique<ui::Layer>();
    window1_old_layer->Add(layer.get());

    std::unique_ptr<ui::Layer> mask_layer = std::make_unique<ui::Layer>();
    window1_old_layer->SetMaskLayer(mask_layer.get());

    // 2 dumps should be created from Add/SetMaskLayer calls above.
    EXPECT_EQ(dump_count, 2);

    {
      ui::Layer* window11_old_layer = tree->root()->children().front();
      std::unique_ptr<ui::Layer> child_layer = std::make_unique<ui::Layer>();
      window11_old_layer->Add(child_layer.get());

      // No new dumps within the 1 day interval.
      EXPECT_EQ(dump_count, 2);
    }

    // Skip the dump blocking time.
    clock.Advance(base::Days(1));

    ui::Layer* window11_old_layer = tree->root()->children().front();
    std::unique_ptr<ui::Layer> child_layer = std::make_unique<ui::Layer>();
    window11_old_layer->Add(child_layer.get());

    std::unique_ptr<ui::Layer> child_mask_layer = std::make_unique<ui::Layer>();
    window11_old_layer->SetMaskLayer(child_mask_layer.get());

    EXPECT_EQ(dump_count, 4);
  }

  base::debug::SetDumpWithoutCrashingFunction(nullptr);
}

}  // namespace wm
