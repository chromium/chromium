// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_EXAMPLES_SKIA_GOLD_PIXEL_DIFF_H_
#define UI_VIEWS_EXAMPLES_EXAMPLES_SKIA_GOLD_PIXEL_DIFF_H_

#include <string>

#include "base/run_loop.h"
#include "ui/base/test/skia_gold_pixel_diff.h"
#include "ui/gfx/image/image.h"
#include "ui/views/examples/examples_exit_code.h"
#include "ui/views/widget/widget.h"

namespace views::examples {

class ExamplesSkiaGoldPixelDiff {
 public:
  ExamplesSkiaGoldPixelDiff();
  ~ExamplesSkiaGoldPixelDiff();

  void Init(const std::string& screenshot_prefix);

  void OnExamplesWindowShown(views::Widget* widget);

  ExamplesExitCode get_result() const { return result_; }

 private:
  ExamplesExitCode CompareScreenshot(const std::string& screenshot_name,
                                     const views::Widget* widget) const;
  void DoScreenshot(views::Widget* widget);

  std::string screenshot_prefix_;
  raw_ptr<ui::test::SkiaGoldPixelDiff> pixel_diff_ = nullptr;
  mutable gfx::Image screenshot_;
  ExamplesExitCode result_ = ExamplesExitCode::kNone;
};

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_EXAMPLES_SKIA_GOLD_PIXEL_DIFF_H_
