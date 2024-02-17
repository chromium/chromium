// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/view_skia_gold_pixel_diff.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/base/test/skia_gold_matching_algorithm.h"
#include "ui/base/test/skia_gold_pixel_diff.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/snapshot/snapshot_aura.h"
#endif

namespace views {

namespace {
void SnapshotCallback(base::RunLoop* run_loop,
                      gfx::Image* ret_image,
                      gfx::Image image) {
  *ret_image = image;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop->QuitClosure());
}
}  // namespace

ViewSkiaGoldPixelDiff::ViewSkiaGoldPixelDiff(
    const std::string& screenshot_prefix,
    const std::optional<std::string>& corpus)
    : screenshot_prefix_(screenshot_prefix),
      pixel_diff_(ui::test::SkiaGoldPixelDiff::GetSession(corpus)) {
  CHECK(pixel_diff_);
}

ViewSkiaGoldPixelDiff::~ViewSkiaGoldPixelDiff() = default;

bool ViewSkiaGoldPixelDiff::CompareViewScreenshot(
    const std::string& screenshot_name,
    const views::View* view,
    const ui::test::SkiaGoldMatchingAlgorithm* algorithm) const {
  // Calculate the snapshot bounds in the widget's coordinates.
  gfx::Rect rc = view->GetBoundsInScreen();
  const views::Widget* widget = view->GetWidget();
  gfx::Rect bounds_in_screen = widget->GetRootView()->GetBoundsInScreen();
  gfx::Rect bounds = widget->GetRootView()->bounds();
  rc.Offset(bounds.x() - bounds_in_screen.x(),
            bounds.y() - bounds_in_screen.y());

  return CompareNativeWindowScreenshot(
      screenshot_name, widget->GetNativeWindow(), rc, algorithm);
}

bool ViewSkiaGoldPixelDiff::CompareNativeWindowScreenshot(
    const std::string& screenshot_name,
    gfx::NativeWindow window,
    const gfx::Rect& snapshot_bounds,
    const ui::test::SkiaGoldMatchingAlgorithm* algorithm) const {
  gfx::Image image;
  bool ret = GrabWindowSnapshotInternal(window, snapshot_bounds, &image);
  if (!ret) {
    return false;
  }

  return pixel_diff_->CompareScreenshot(
      ui::test::SkiaGoldPixelDiff::GetGoldenImageName(
          screenshot_prefix_, screenshot_name,
          ui::test::SkiaGoldPixelDiff::GetPlatform()),
      *image.ToSkBitmap(), algorithm);
}

bool ViewSkiaGoldPixelDiff::CompareNativeWindowScreenshotInRects(
    const std::string& screenshot_name,
    gfx::NativeWindow window,
    const gfx::Rect& snapshot_bounds,
    const ui::test::SkiaGoldMatchingAlgorithm* algorithm,
    const std::vector<gfx::Rect>& regions_of_interest) const {
  CHECK(!algorithm || algorithm->GetCommandLineSwitchName() != "sobel");

  gfx::Image image;
  bool ret = GrabWindowSnapshotInternal(window, snapshot_bounds, &image);
  if (!ret) {
    return false;
  }

  // Only keep the pixels within `regions_of_interest` so that the differences
  // outside of `regions_of_interest` are ignored.
  KeepPixelsInRects(regions_of_interest, &image);

  return pixel_diff_->CompareScreenshot(
      ui::test::SkiaGoldPixelDiff::GetGoldenImageName(
          screenshot_prefix_, screenshot_name,
          ui::test::SkiaGoldPixelDiff::GetPlatform()),
      *image.ToSkBitmap(), algorithm);
}

bool ViewSkiaGoldPixelDiff::GrabWindowSnapshotInternal(
    gfx::NativeWindow window,
    const gfx::Rect& snapshot_bounds,
    gfx::Image* image) const {
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
#if defined(USE_AURA)
  ui::GrabWindowSnapshotAura(
#else
  ui::GrabWindowSnapshot(
#endif
      window, snapshot_bounds,
      base::BindOnce(&SnapshotCallback, &run_loop, image));
  run_loop.Run();

  const bool success = !image->IsEmpty();
  if (!success) {
    LOG(ERROR) << "Grab screenshot failed.";
  }
  return success;
}

void ViewSkiaGoldPixelDiff::KeepPixelsInRects(
    const std::vector<gfx::Rect>& rects,
    gfx::Image* image) const {
  // `rects` should not be empty.
  CHECK(!rects.empty());

  // Create a bitmap with the same size as `image`. NOTE: `image` is immutable.
  // Therefore, we have to create a new bitmap.
  SkBitmap bitmap;
  bitmap.allocPixels(image->ToSkBitmap()->info());
  bitmap.eraseColor(SK_ColorTRANSPARENT);

  // Allow `canvas` to draw on `bitmap`.
  SkCanvas canvas(bitmap, SkSurfaceProps{});

  // Only copy the pixels within `rects`.
  SkPaint paint;
  for (const auto& rect : rects) {
    canvas.drawImageRect(image->ToSkBitmap()->asImage(),
                         gfx::RectToSkRect(rect), gfx::RectToSkRect(rect),
                         SkSamplingOptions(), &paint,
                         SkCanvas::kStrict_SrcRectConstraint);
  }

  *image = gfx::Image::CreateFrom1xBitmap(bitmap);
}

}  // namespace views
