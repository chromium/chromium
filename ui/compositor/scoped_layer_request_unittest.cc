// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/scoped_layer_request.h"

#include <memory>

#include "cc/layers/layer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer_test_api.h"
#include "ui/compositor/layer_textured.h"

namespace ui {

using ScopedLayerRequestTest = testing::Test;

TEST_F(ScopedLayerRequestTest, ScopedPaintLock) {
  LayerTextured layer;
  ui::LayerTestApi layer_test_api(&layer);
  EXPECT_FALSE(layer_test_api.IsPaintDeferred());
  {
    ScopedPaintLock lock(&layer);
    EXPECT_TRUE(layer_test_api.IsPaintDeferred());
    EXPECT_EQ(&layer, lock.GetLayer());
  }
  EXPECT_FALSE(layer_test_api.IsPaintDeferred());
}

TEST_F(ScopedLayerRequestTest, ScopedCacheRenderSurfaceLock) {
  LayerTextured layer;
  ui::LayerTestApi layer_test_api(&layer);
  EXPECT_FALSE(layer_test_api.cc_layer()->cache_render_surface());
  {
    ScopedCacheRenderSurfaceLock lock(&layer);
    EXPECT_TRUE(layer_test_api.cc_layer()->cache_render_surface());
    EXPECT_EQ(&layer, lock.GetLayer());
  }
  EXPECT_FALSE(layer_test_api.cc_layer()->cache_render_surface());
}

TEST_F(ScopedLayerRequestTest, ScopedTrilinearFilteringLock) {
  LayerTextured layer;
  ui::LayerTestApi layer_test_api(&layer);
  EXPECT_FALSE(layer_test_api.cc_layer()->trilinear_filtering());
  {
    ScopedTrilinearFilteringLock lock(&layer);
    EXPECT_TRUE(layer_test_api.cc_layer()->trilinear_filtering());
    EXPECT_EQ(&layer, lock.GetLayer());
  }
  EXPECT_FALSE(layer_test_api.cc_layer()->trilinear_filtering());
}

TEST_F(ScopedLayerRequestTest, LayerDestroyedWhileLocked) {
  auto layer = std::make_unique<LayerTextured>();
  auto lock = std::make_unique<ScopedPaintLock>(layer.get());
  auto layer_test_api = std::make_unique<ui::LayerTestApi>(layer.get());
  EXPECT_TRUE(layer_test_api->IsPaintDeferred());
  EXPECT_EQ(layer.get(), lock->GetLayer());
  // Destroying the layer should not crash and should reset the observed layer.
  layer_test_api.reset();
  layer.reset();
  EXPECT_EQ(nullptr, lock->GetLayer());
  // Destroying the lock shouldn't crash when layer is already destroyed.
  lock.reset();
}

}  // namespace ui
