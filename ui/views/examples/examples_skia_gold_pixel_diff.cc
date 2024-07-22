// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/examples_skia_gold_pixel_diff.h"

#include <utility>

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/native_theme.h"
#include "ui/snapshot/snapshot.h"
#include "ui/views/examples/examples_window.h"

#if defined(USE_AURA)
#include "ui/snapshot/snapshot_aura.h"
#endif

namespace views::examples {

ExamplesSkiaGoldPixelDiff::ExamplesSkiaGoldPixelDiff() = default;
ExamplesSkiaGoldPixelDiff::~ExamplesSkiaGoldPixelDiff() = default;

void ExamplesSkiaGoldPixelDiff::Init(const std::string& screenshot_prefix) {
  screenshot_prefix_ = screenshot_prefix;
  CHECK(!pixel_diff_);
  pixel_diff_ = ui::test::SkiaGoldPixelDiff::GetSession();
  CHECK(pixel_diff_);
}

ExamplesExitCode ExamplesSkiaGoldPixelDiff::CompareScreenshot(
    const std::string& screenshot_name,
    const views::Widget* widget) const {
  // If host is in dark mode skip the pixel comparison.
  if (ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()) {
    return ExamplesExitCode::kNone;
  }

  CHECK(pixel_diff_) << "Initialize the class before using this method.";

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  gfx::Rect widget_bounds = widget->GetRootView()->bounds();
#if defined(USE_AURA)
  ui::GrabWindowSnapshotAura(
#else
  ui::GrabWindowSnapshot(
#endif
      widget->GetNativeWindow(), widget_bounds,
      base::BindOnce(
          [](gfx::Image* screenshot, base::OnceClosure quit_loop,
             gfx::Image image) {
            *screenshot = image;
            std::move(quit_loop).Run();
          },
          &screenshot_, run_loop.QuitClosure()));
  run_loop.Run();
  if (screenshot_.IsEmpty())
    return ExamplesExitCode::kImageEmpty;
  return pixel_diff_->CompareScreenshot(
             ui::test::SkiaGoldPixelDiff::GetGoldenImageName(
                 screenshot_prefix_, screenshot_name,
                 ui::test::SkiaGoldPixelDiff::GetPlatform()),
             *screenshot_.ToSkBitmap())
             ? ExamplesExitCode::kSucceeded
             : ExamplesExitCode::kFailed;
}

void ExamplesSkiaGoldPixelDiff::DoScreenshot(views::Widget* widget) {
  const auto* const test_info =
      testing::UnitTest::GetInstance()->current_test_info();
  result_ = CompareScreenshot(test_info ? test_info->name() : "ExampleWindow",
                              widget);
  widget->Close();
}

void ExamplesSkiaGoldPixelDiff::OnExamplesWindowShown(views::Widget* widget) {
  if (widget->GetName() == views::examples::kExamplesWidgetName) {
    DoScreenshot(widget);
  }
}

}  // namespace views::examples
