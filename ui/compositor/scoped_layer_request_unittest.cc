// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/scoped_layer_request.h"

#include <memory>

#include "cc/layers/layer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"

namespace ui {

using ScopedLayerRequestTest = testing::Test;

TEST_F(ScopedLayerRequestTest, ScopedPaintLock) {
  Layer layer;
  EXPECT_FALSE(layer.IsPaintDeferredForTesting());
  {
    ScopedPaintLock lock(&layer);
    EXPECT_TRUE(layer.IsPaintDeferredForTesting());
    EXPECT_EQ(&layer, lock.GetLayer());
  }
  EXPECT_FALSE(layer.IsPaintDeferredForTesting());
}

TEST_F(ScopedLayerRequestTest, ScopedCacheRenderSurfaceLock) {
  Layer layer;
  EXPECT_FALSE(layer.cc_layer_for_testing()->cache_render_surface());
  {
    ScopedCacheRenderSurfaceLock lock(&layer);
    EXPECT_TRUE(layer.cc_layer_for_testing()->cache_render_surface());
    EXPECT_EQ(&layer, lock.GetLayer());
  }
  EXPECT_FALSE(layer.cc_layer_for_testing()->cache_render_surface());
}

TEST_F(ScopedLayerRequestTest, ScopedTrilinearFilteringLock) {
  Layer layer;
  EXPECT_FALSE(layer.cc_layer_for_testing()->trilinear_filtering());
  {
    ScopedTrilinearFilteringLock lock(&layer);
    EXPECT_TRUE(layer.cc_layer_for_testing()->trilinear_filtering());
    EXPECT_EQ(&layer, lock.GetLayer());
  }
  EXPECT_FALSE(layer.cc_layer_for_testing()->trilinear_filtering());
}

TEST_F(ScopedLayerRequestTest, LayerDestroyedWhileLocked) {
  auto layer = std::make_unique<Layer>();
  auto lock = std::make_unique<ScopedPaintLock>(layer.get());
  EXPECT_TRUE(layer->IsPaintDeferredForTesting());
  EXPECT_EQ(layer.get(), lock->GetLayer());
  // Destroying the layer should not crash and should reset the observed layer.
  layer.reset();
  EXPECT_EQ(nullptr, lock->GetLayer());
  // Destroying the lock shouldn't crash when layer is already destroyed.
  lock.reset();
}

}  // namespace ui
