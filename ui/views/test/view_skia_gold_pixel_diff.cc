// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/view_skia_gold_pixel_diff.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop->QuitClosure());
}
}  // namespace

ViewSkiaGoldPixelDiff::ViewSkiaGoldPixelDiff() = default;

ViewSkiaGoldPixelDiff::~ViewSkiaGoldPixelDiff() = default;

bool ViewSkiaGoldPixelDiff::CompareViewScreenshot(
    const std::string& screenshot_name,
    views::View* view,
    const ui::test::SkiaGoldMatchingAlgorithm* algorithm) const {
  DCHECK(Initialized()) << "Initialize the class before using this method.";

  // Calculate the snapshot bounds in the widget's coordinates.
  gfx::Rect rc = view->GetBoundsInScreen();
  views::Widget* widget = view->GetWidget();
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
  DCHECK(Initialized()) << "Initialize the class before using this method.";

  gfx::Image image;
  bool ret = GrabWindowSnapshotInternal(window, snapshot_bounds, &image);
  if (!ret) {
    LOG(ERROR) << "Grab screenshot failed.";
    return false;
  }
  return SkiaGoldPixelDiff::CompareScreenshot(screenshot_name,
                                              *image.ToSkBitmap(), algorithm);
}

bool ViewSkiaGoldPixelDiff::GrabWindowSnapshotInternal(
    gfx::NativeWindow window,
    const gfx::Rect& snapshot_bounds,
    gfx::Image* image) const {
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
#if defined(USE_AURA)
  ui::GrabWindowSnapshotAsyncAura(
#else
  ui::GrabWindowSnapshotAsync(
#endif
      window, snapshot_bounds,
      base::BindOnce(&SnapshotCallback, &run_loop, image));
  run_loop.Run();
  return !image->IsEmpty();
}

}  // namespace views
