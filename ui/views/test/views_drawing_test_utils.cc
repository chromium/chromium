// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_drawing_test_utils.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/paint_info.h"
#include "ui/views/view.h"

namespace views {
namespace test {

SkBitmap PaintViewToBitmap(View* view) {
  SkBitmap bitmap;
  {
    gfx::Size size = view->size();
    ui::CanvasPainter canvas_painter(&bitmap, size, 1.f, gfx::kPlaceholderColor,
                                     false);
    view->Paint(PaintInfo::CreateRootPaintInfo(canvas_painter.context(), size));
  }
  return bitmap;
}

}  // namespace test
}  // namespace views
