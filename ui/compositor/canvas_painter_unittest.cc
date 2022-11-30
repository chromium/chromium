// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/canvas_painter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
namespace {
void CheckPaintedShape(const SkBitmap& bitmap,
                       const gfx::Rect& shape_bounds,
                       const SkColor shape_color,
                       float device_scale_factor) {
  // Whether pixel canvas is enabled or not, the pixel location of the shape
  // should remain the same.
  const gfx::Point expected_top_left_location =
      gfx::ScaleToRoundedPoint(shape_bounds.origin(), device_scale_factor);
  const gfx::Point expected_bottom_right_location = gfx::ScaleToRoundedPoint(
      shape_bounds.bottom_right(), device_scale_factor);

  EXPECT_EQ(bitmap.getColor(expected_top_left_location.x(),
                            expected_top_left_location.y()),
            shape_color);
  EXPECT_EQ(bitmap.getColor(expected_bottom_right_location.x(),
                            expected_bottom_right_location.y()),
            shape_color);
}
}  // namespace

class CanvasPainterTest : public ::testing::TestWithParam<float> {
 public:
  CanvasPainterTest() : device_scale_factor_(GetParam()) {}

  CanvasPainterTest(const CanvasPainterTest&) = delete;
  CanvasPainterTest& operator=(const CanvasPainterTest&) = delete;

  float device_scale_factor() const { return device_scale_factor_; }

  const gfx::Size& pixel_output_size(const CanvasPainter& painter) const {
    return painter.pixel_output_size_;
  }

  float raster_scale(const CanvasPainter& painter) const {
    return painter.raster_scale_;
  }

  // Paints a rect with bounds |shape_bounds| and color |shape_color| on
  // |bitmap| with the help of CanvasPainter. The output size of the bitmap in
  // DIP is |size|.
  void Paint(SkBitmap* bitmap,
             const gfx::Size& size,
             float device_scale_factor,
             bool is_pixel_canvas,
             const gfx::Rect& shape_bounds,
             SkColor shape_color) {
    CanvasPainter painter(bitmap, size, device_scale_factor,
                          SK_ColorTRANSPARENT, is_pixel_canvas);

    // The paint recording size is scaled to match the raster size if pixel
    // canvas is enabled.
    const gfx::Size paint_recording_size = gfx::ScaleToCeiledSize(
        size, is_pixel_canvas ? device_scale_factor : 1.f);

    PaintRecorder recorder(painter.context(), paint_recording_size,
                           device_scale_factor, device_scale_factor, nullptr);
    recorder.canvas()->DrawRect(gfx::RectF(shape_bounds), shape_color);
  }

 private:
  float device_scale_factor_;
};

TEST_P(CanvasPainterTest, Initialization) {
  SkBitmap output;
  const gfx::Size output_size(100, 100);
  CanvasPainter painter(&output, output_size, device_scale_factor(),
                        SK_ColorTRANSPARENT, false /* is_pixel_canvas */);
  EXPECT_EQ(pixel_output_size(painter),
            gfx::ScaleToCeiledSize(output_size, device_scale_factor()));
  EXPECT_EQ(raster_scale(painter), device_scale_factor());
}

TEST_P(CanvasPainterTest, InitializationPixelCanvasEnabled) {
  SkBitmap output;
  const gfx::Size output_size(100, 100);
  CanvasPainter painter(&output, output_size, device_scale_factor(),
                        SK_ColorTRANSPARENT, true /* is_pixel_canvas */);
  EXPECT_EQ(pixel_output_size(painter),
            gfx::ScaleToCeiledSize(output_size, device_scale_factor()));
  EXPECT_EQ(raster_scale(painter), 1.f);
}

TEST_P(CanvasPainterTest, Paint) {
  SkBitmap bitmap;
  const SkColor shape_color = SK_ColorRED;
  const gfx::Rect shape_bounds(100, 100, 100, 100);

  Paint(&bitmap, gfx::Size(1000, 1000), device_scale_factor(),
        false /* is_pixel_canvas */, shape_bounds, shape_color);
  CheckPaintedShape(bitmap, shape_bounds, shape_color, device_scale_factor());
}

TEST_P(CanvasPainterTest, PaintPixelCanvasEnabled) {
  SkBitmap bitmap;
  const SkColor shape_color = SK_ColorRED;
  const gfx::Rect shape_bounds(100, 100, 100, 100);

  Paint(&bitmap, gfx::Size(1000, 1000), device_scale_factor(),
        true /* is_pixel_canvas */, shape_bounds, shape_color);
  CheckPaintedShape(bitmap, shape_bounds, shape_color, device_scale_factor());
}

INSTANTIATE_TEST_SUITE_P(All,
                         CanvasPainterTest,
                         ::testing::Values(1.f, 1.25f, 1.5f, 1.6f, 2.f, 2.25f));
}  // namespace ui
