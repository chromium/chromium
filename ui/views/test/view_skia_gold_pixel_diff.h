// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_VIEW_SKIA_GOLD_PIXEL_DIFF_H_
#define UI_VIEWS_TEST_VIEW_SKIA_GOLD_PIXEL_DIFF_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/base/test/skia_gold_pixel_diff.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Rect;
class Image;
}  // namespace gfx

namespace ui::test {
class SkiaGoldMatchingAlgorithm;
class SkiaGoldPixelDiff;
}  // namespace ui::test

namespace views {
class View;

// This is the utility class to protect views with pixeltest based on Skia Gold.
// For an example on how to write pixeltests, please refer to the demo.
// NOTE: this class has to be initialized before using. A screenshot prefix and
// a corpus string are required for initialization. Check
// `SkiaGoldPixelDiff::Init()` for more details.
class ViewSkiaGoldPixelDiff {
 public:
  explicit ViewSkiaGoldPixelDiff(const std::string& screenshot_prefix,
                                 const std::optional<std::string>& corpus = {});

  ViewSkiaGoldPixelDiff(const ViewSkiaGoldPixelDiff&) = delete;
  ViewSkiaGoldPixelDiff& operator=(const ViewSkiaGoldPixelDiff&) = delete;

  virtual ~ViewSkiaGoldPixelDiff();

  // Takes a screenshot then uploads to Skia Gold and compares it with the
  // remote golden image. Returns true if the screenshot is the same as the
  // golden image (compared with hashcode).
  // `screenshot_name` specifies the name of the screenshot to be taken. For
  // every screenshot you take, it should have a unique name across Chromium,
  // because all screenshots (aka golden images) stores in one bucket on GCS.
  // The standard convention is to use the browser test class name as the
  // prefix. The name will be `screenshot_prefix` + "_" + `screenshot_name`.
  // E.g. 'ToolbarTest_BackButtonHover'. Here `screenshot_prefix` is passed as
  // an argument during initialization.
  // `view` is the view you want to take screenshot.
  // `algorithm` specifies how two images are matched. Use the exact match as
  // default. Read the comment of `SkiaGoldMatchingAlgorithm` to learn more.
  bool CompareViewScreenshot(
      const std::string& screenshot_name,
      const views::View* view,
      const ui::test::SkiaGoldMatchingAlgorithm* algorithm = nullptr) const;

  // Similar to `CompareViewScreenshot()`. But the screenshot is taken within
  // the specified bounds on `window`. `snapshot_bounds` is based on `window`'s
  // local coordinates.
  bool CompareNativeWindowScreenshot(
      const std::string& screenshot_name,
      gfx::NativeWindow window,
      const gfx::Rect& snapshot_bounds,
      const ui::test::SkiaGoldMatchingAlgorithm* algorithm = nullptr) const;

  // Similar to `CompareNativeWindowScreenshot()` but with the difference that
  // only the pixel differences within `regions_of_interest` should affect the
  // comparison result. Each rect in `regions_of_interest` is in pixel
  // coordinates. NOTE:
  // 1. If `algorithm` is `FuzzySkiaGoldMatchingAlgorithm`, the total amount of
  // different pixels across `regions_of_interest` is compared with the
  // threshold carried by `algorithm`.
  // 2. `algorithm` cannot be `SobelSkiaGoldMatchingAlgorithm` because the
  // border of an image with `regions_of_interest` is ambiguous.
  bool CompareNativeWindowScreenshotInRects(
      const std::string& screenshot_name,
      gfx::NativeWindow window,
      const gfx::Rect& snapshot_bounds,
      const ui::test::SkiaGoldMatchingAlgorithm* algorithm,
      const std::vector<gfx::Rect>& regions_of_interest) const;

 protected:
  // Takes a screenshot of `window` within the specified area and stores the
  // screenshot in `image`. Returns true if succeeding.
  virtual bool GrabWindowSnapshotInternal(gfx::NativeWindow window,
                                          const gfx::Rect& snapshot_bounds,
                                          gfx::Image* image) const;

 private:
  // Updates `image` so that only the pixels within `rects` are kept.
  void KeepPixelsInRects(const std::vector<gfx::Rect>& rects,
                         gfx::Image* image) const;

  const std::string screenshot_prefix_;
  raw_ptr<ui::test::SkiaGoldPixelDiff> pixel_diff_ = nullptr;
};

}  // namespace views

#endif  // UI_VIEWS_TEST_VIEW_SKIA_GOLD_PIXEL_DIFF_H_
