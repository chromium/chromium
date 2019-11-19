// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/window_util.h"

#include <memory>

#include "base/bind.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/wm/core/window_properties.h"

namespace wm {

typedef aura::test::AuraTestBase WindowUtilTest;

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

}  // namespace wm
