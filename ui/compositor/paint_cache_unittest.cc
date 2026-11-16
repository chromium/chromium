// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/paint_cache.h"

#include "base/memory/scoped_refptr.h"
#include "cc/paint/display_item_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/compositor/paint_context.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/transform_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia.h"

namespace ui {
namespace {

TEST(PaintCacheTest, SubpixelOffsetInvalidatesCacheWithoutPixelCanvas) {
  constexpr float kScale = 1.25f;
  constexpr gfx::Size kSize(16, 16);

  PaintCache cache;
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  PaintContext root(list.get(), kScale, gfx::Rect(200, 200),
                    /*is_pixel_canvas=*/false);

  // Record at offset (10, 10) DIPs = (12.5, 12.5) device pixels.
  PaintContext initial(root, gfx::Vector2d(10, 10));
  EXPECT_FALSE(cache.UseCache(initial, kSize));
  {
    PaintRecorder recorder(initial, kSize, kScale, kScale, &cache);
    recorder.canvas()->FillRect(gfx::Rect(kSize), SK_ColorRED);
  }

  // Identical offset reuses the cache.
  EXPECT_TRUE(cache.UseCache(initial, kSize));

  // Moving by (4, 8) DIPs = (5, 10) whole device pixels reuses the cache.
  PaintContext whole_pixel_move(root, gfx::Vector2d(14, 18));
  EXPECT_TRUE(cache.UseCache(whole_pixel_move, kSize));

  // Sub-pixel device moves reject the cache so images re-snap cleanly.
  PaintContext subpixel_x_move(root, gfx::Vector2d(11, 10));
  EXPECT_FALSE(cache.UseCache(subpixel_x_move, kSize));

  PaintContext subpixel_y_move(root, gfx::Vector2d(10, 12));
  EXPECT_FALSE(cache.UseCache(subpixel_y_move, kSize));

  // Changing the device scale factor rejects the cache.
  PaintContext different_scale(list.get(), 1.5f, gfx::Rect(200, 200),
                               /*is_pixel_canvas=*/false);
  PaintContext different_scale_child(different_scale, gfx::Vector2d(10, 10));
  EXPECT_FALSE(cache.UseCache(different_scale_child, kSize));
}

TEST(PaintCacheTest, SubpixelOffsetReusesCacheWithPixelCanvas) {
  constexpr float kScale = 1.25f;
  constexpr gfx::Size kSize(20, 20);

  PaintCache cache;
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  PaintContext root(list.get(), kScale, gfx::Rect(200, 200),
                    /*is_pixel_canvas=*/true);

  PaintContext initial(root, gfx::Vector2d(10, 10));
  {
    PaintRecorder recorder(initial, kSize, kScale, kScale, &cache);
    recorder.canvas()->FillRect(gfx::Rect(16, 16), SK_ColorRED);
  }

  PaintContext moved(root, gfx::Vector2d(11, 12));
  EXPECT_TRUE(cache.UseCache(moved, kSize));
}

TEST(PaintCacheTest, SnapsRecordedImagesAcrossSubpixelAndWholePixelMoves) {
  constexpr float kScale = 1.25f;
  constexpr int kPixelSize = 20;
  constexpr int kDipSize = 16;

  SkBitmap bitmap;
  bitmap.allocN32Pixels(kPixelSize, kPixelSize);
  for (int y = 0; y < kPixelSize; ++y) {
    for (int x = 0; x < kPixelSize; ++x) {
      SkColor color = ((x + y) % 2 == 0) ? SK_ColorRED : SK_ColorBLUE;
      *bitmap.getAddr32(x, y) = SkPreMultiplyColor(color);
    }
  }
  gfx::ImageSkia image = gfx::ImageSkia::CreateFromBitmap(bitmap, kScale);

  PaintCache cache;
  auto paint_at = [&](const gfx::Vector2d& offset, bool* used_cache) {
    SkBitmap out;
    {
      CanvasPainter painter(&out, gfx::Size(64, 64), kScale,
                            SK_ColorTRANSPARENT, /*is_pixel_canvas=*/false);
      PaintContext child_context(painter.context(), offset);
      TransformRecorder transform_recorder(painter.context());
      gfx::Transform translate;
      translate.Translate(offset);
      transform_recorder.Transform(translate);

      const gfx::Size recording_size(kDipSize, kDipSize);
      *used_cache = cache.UseCache(child_context, recording_size);
      if (!*used_cache) {
        PaintRecorder recorder(child_context, recording_size, kScale, kScale,
                               &cache);
        recorder.canvas()->DrawImageInt(image, 0, 0, kDipSize, kDipSize, 0, 0,
                                        kDipSize, kDipSize, /*filter=*/true);
      }
    }
    return out;
  };

  // Offset (10, 10) DIPs = (12.5, 12.5) px snaps to (13, 13) px.
  bool used_cache = true;
  SkBitmap first = paint_at(gfx::Vector2d(10, 10), &used_cache);
  EXPECT_FALSE(used_cache);
  for (int y = 0; y < kPixelSize; ++y) {
    for (int x = 0; x < kPixelSize; ++x) {
      EXPECT_EQ(bitmap.getColor(x, y), first.getColor(13 + x, 13 + y));
    }
  }

  // Offset (11, 10) DIPs = (13.75, 12.5) px misses the cache and re-snaps to
  // (14, 13) px.
  SkBitmap second = paint_at(gfx::Vector2d(11, 10), &used_cache);
  EXPECT_FALSE(used_cache);
  for (int y = 0; y < kPixelSize; ++y) {
    for (int x = 0; x < kPixelSize; ++x) {
      EXPECT_EQ(bitmap.getColor(x, y), second.getColor(14 + x, 13 + y));
    }
  }

  // Offset (15, 10) DIPs = (18.75, 12.5) px is +4 DIPs (+5 whole device px)
  // from (11, 10), so it reuses the cached record and lands sharply at
  // (19, 13) px.
  SkBitmap third = paint_at(gfx::Vector2d(15, 10), &used_cache);
  EXPECT_TRUE(used_cache);
  for (int y = 0; y < kPixelSize; ++y) {
    for (int x = 0; x < kPixelSize; ++x) {
      EXPECT_EQ(bitmap.getColor(x, y), third.getColor(19 + x, 13 + y));
    }
  }
}

}  // namespace
}  // namespace ui
