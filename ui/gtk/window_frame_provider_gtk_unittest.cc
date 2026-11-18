// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/window_frame_provider_gtk.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace gtk {

namespace {

void PaintFrame(WindowFrameProviderGtk& provider, float scale) {
  gfx::Canvas canvas(gfx::Size(200, 200), scale, /*is_opaque=*/false);
  provider.PaintWindowFrame(&canvas, gfx::Rect(0, 0, 200, 200),
                            /*top_area_height=*/40, /*focused=*/true,
                            gfx::Insets());
}

}  // namespace

// Painting at a non-1x scale before any metrics are cached must not use stale
// cache entries.
TEST(WindowFrameProviderGtkTest, PaintAtHighScaleBeforeMetricsCached) {
  for (bool tiled : {false, true}) {
    WindowFrameProviderGtk provider(ui::FrameType::kBrowser,
                                    /*solid_frame=*/false, tiled,
                                    /*maximized=*/false);
    PaintFrame(provider, 2.0f);
    PaintFrame(provider, 1.5f);
    PaintFrame(provider, 1.0f);
    EXPECT_GE(provider.GetFrameThicknessDip().left(), 0);
  }
}

// PaintWindowFrame() holds a reference from GetOrCreateAsset() while other
// scales may be added to the cache, so existing references must stay valid.
TEST(WindowFrameProviderGtkTest, AssetReferencesStayValid) {
  WindowFrameProviderGtk provider(ui::FrameType::kBrowser,
                                  /*solid_frame=*/false, /*tiled=*/false,
                                  /*maximized=*/false);
  const auto* asset = &provider.GetOrCreateAsset(2.0f);
  for (float scale : {1.0f, 1.25f, 1.5f, 1.75f, 3.0f}) {
    provider.GetOrCreateAsset(scale);
  }
  EXPECT_EQ(asset, &provider.GetOrCreateAsset(2.0f));
}

}  // namespace gtk
