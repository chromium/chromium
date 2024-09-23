// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/paint_vector_icon.h"

#include <gtest/gtest.h>

#include <vector>

#include "base/i18n/rtl.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/paint_recorder.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/vector_icon_types.h"

namespace gfx {

namespace {

SkColor GetColorAtTopLeft(const Canvas& canvas) {
  return canvas.GetBitmap().getColor(0, 0);
}

class MockCanvas : public SkCanvas {
 public:
  MockCanvas(int width, int height) : SkCanvas(width, height) {}

  MockCanvas(const MockCanvas&) = delete;
  MockCanvas& operator=(const MockCanvas&) = delete;

  // SkCanvas overrides:
  void onDrawPath(const SkPath& path, const SkPaint& paint) override {
    paths_.push_back(path);
  }

  const std::vector<SkPath>& paths() const { return paths_; }

 private:
  std::vector<SkPath> paths_;
};

// Tests that a relative move to command (R_MOVE_TO) after a close command
// (CLOSE) uses the correct starting point. See crbug.com/697497
TEST(VectorIconTest, RelativeMoveToAfterClose) {
  cc::PaintRecorder recorder;
  Canvas canvas(recorder.beginRecording(), 1.0f);

  const PathElement elements[] = {
      MOVE_TO, 4, 5, LINE_TO, 10, 11, CLOSE,
      // This move should use (4, 5) as the start point rather than (10, 11).
      R_MOVE_TO, 20, 21, R_LINE_TO, 50, 51};
  const VectorIconRep rep_list[] = {{elements, std::size(elements)}};
  const VectorIcon icon(rep_list, 1u, nullptr);

  PaintVectorIcon(&canvas, icon, 100, SK_ColorMAGENTA);
  cc::PaintRecord record = recorder.finishRecordingAsPicture();

  MockCanvas mock(100, 100);
  record.Playback(&mock);

  ASSERT_EQ(1U, mock.paths().size());
  SkPoint last_point;
  EXPECT_TRUE(mock.paths()[0].getLastPt(&last_point));
  EXPECT_EQ(SkIntToScalar(74), last_point.x());
  EXPECT_EQ(SkIntToScalar(77), last_point.y());
}

TEST(VectorIconTest, FillRuleNonZero) {
  Canvas canvas(gfx::Size(200, 100), 1.0, true);
  /* Draw two paths, and each path consists of two stacked squares.
     The first has fill type kEvenOdd and the second has kWinding.
     Both paths have winding order of 2 at their centers.
    +---->------+     +---->------+
    | +-->----+ |     | +-->----+ |
    | |       v V     | |       v V
    | |       | |     | |       | |
    | +--<----+ |     | +---<---+ |
    +----<------+     +-----<-----+
       EvenOdd            Winding
  */
  const PathElement elements[] = {
      MOVE_TO, 0, 0, R_H_LINE_TO, 90, R_V_LINE_TO, 90, R_H_LINE_TO, -90, CLOSE,
      MOVE_TO, 20, 20, R_H_LINE_TO, 50, R_V_LINE_TO, 50, R_H_LINE_TO, -50,
      CLOSE,
      // The next path is filled by kWinding.
      NEW_PATH, FILL_RULE_NONZERO, MOVE_TO, 110, 0, R_H_LINE_TO, 90,
      R_V_LINE_TO, 90, R_H_LINE_TO, -90, CLOSE, MOVE_TO, 130, 20, R_H_LINE_TO,
      50, R_V_LINE_TO, 50, R_H_LINE_TO, -50, CLOSE};
  const VectorIconRep rep_list[] = {{elements, std::size(elements)}};
  const VectorIcon icon(rep_list, 1u, nullptr);

  PaintVectorIcon(&canvas, icon, 100, SK_ColorBLACK);
  // For EvenOdd fill type, the center is NOT filled, because its winding order
  // is an even number.
  EXPECT_EQ(SK_ColorTRANSPARENT, canvas.GetBitmap().getColor(50, 50));
  // For Winding fill type, the center is filled, because its winding order
  // is not zero.
  EXPECT_EQ(SK_ColorBLACK, canvas.GetBitmap().getColor(150, 50));
}

TEST(VectorIconTest, FlipsInRtl) {
  // We need to set RTL, otherwise FLIPS_IN_RTL does nothing.
  base::i18n::SetRTLForTesting(true);
  ASSERT_TRUE(base::i18n::IsRTL());

  const int canvas_size = 20;
  const SkColor color = SK_ColorWHITE;

  Canvas canvas(gfx::Size(canvas_size, canvas_size), 1.0f, true);

  // Create a 20x20 square icon which has FLIPS_IN_RTL, and CANVAS_DIMENSIONS
  // are twice as large as |canvas|.
  const PathElement elements[] = {CANVAS_DIMENSIONS,
                                  2 * canvas_size,
                                  FLIPS_IN_RTL,
                                  MOVE_TO,
                                  10,
                                  10,
                                  R_H_LINE_TO,
                                  20,
                                  R_V_LINE_TO,
                                  20,
                                  R_H_LINE_TO,
                                  -20,
                                  CLOSE};
  const VectorIconRep rep_list[] = {{elements, std::size(elements)}};
  const VectorIcon icon(rep_list, 1u, nullptr);
  PaintVectorIcon(&canvas, icon, canvas_size, color);

  // Count the number of pixels in the canvas.
  auto bitmap = canvas.GetBitmap();
  int colored_pixel_count = 0;
  for (int i = 0; i < bitmap.width(); ++i) {
    for (int j = 0; j < bitmap.height(); ++j) {
      if (bitmap.getColor(i, j) == color)
        colored_pixel_count++;
    }
  }

  // Verify that the amount of colored pixels on the canvas bitmap should be a
  // quarter of the original icon, since each side should be scaled down by a
  // factor of two.
  EXPECT_EQ(100, colored_pixel_count);

  base::i18n::SetRTLForTesting(false);
}

TEST(VectorIconTest, CorrectSizePainted) {
  // Create a set of 5 icons reps, sized {48, 32, 24, 20, 16} for the test icon.
  // Color each of them differently so they can be differentiated (the parts of
  // an icon painted with PATH_COLOR_ARGB will not be overwritten by the color
  // provided to it at creation time).
  const SkColor kPath48Color = SK_ColorRED;
  const PathElement elements48[] = {CANVAS_DIMENSIONS,
                                    48,
                                    PATH_COLOR_ARGB,
                                    0xFF,
                                    SkColorGetR(kPath48Color),
                                    SkColorGetG(kPath48Color),
                                    SkColorGetB(kPath48Color),
                                    MOVE_TO,
                                    0,
                                    0,
                                    H_LINE_TO,
                                    48,
                                    V_LINE_TO,
                                    48,
                                    H_LINE_TO,
                                    0,
                                    V_LINE_TO,
                                    0,
                                    CLOSE};
  const SkColor kPath32Color = SK_ColorGREEN;
  const PathElement elements32[] = {CANVAS_DIMENSIONS,
                                    32,
                                    PATH_COLOR_ARGB,
                                    0xFF,
                                    SkColorGetR(kPath32Color),
                                    SkColorGetG(kPath32Color),
                                    SkColorGetB(kPath32Color),
                                    MOVE_TO,
                                    0,
                                    0,
                                    H_LINE_TO,
                                    32,
                                    V_LINE_TO,
                                    32,
                                    H_LINE_TO,
                                    0,
                                    V_LINE_TO,
                                    0,
                                    CLOSE};
  const SkColor kPath24Color = SK_ColorBLUE;
  const PathElement elements24[] = {CANVAS_DIMENSIONS,
                                    24,
                                    PATH_COLOR_ARGB,
                                    0xFF,
                                    SkColorGetR(kPath24Color),
                                    SkColorGetG(kPath24Color),
                                    SkColorGetB(kPath24Color),
                                    MOVE_TO,
                                    0,
                                    0,
                                    H_LINE_TO,
                                    24,
                                    V_LINE_TO,
                                    24,
                                    H_LINE_TO,
                                    0,
                                    V_LINE_TO,
                                    0,
                                    CLOSE};
  const SkColor kPath20Color = SK_ColorYELLOW;
  const PathElement elements20[] = {CANVAS_DIMENSIONS,
                                    20,
                                    PATH_COLOR_ARGB,
                                    0xFF,
                                    SkColorGetR(kPath20Color),
                                    SkColorGetG(kPath20Color),
                                    SkColorGetB(kPath20Color),
                                    MOVE_TO,
                                    0,
                                    0,
                                    H_LINE_TO,
                                    20,
                                    V_LINE_TO,
                                    20,
                                    H_LINE_TO,
                                    0,
                                    V_LINE_TO,
                                    0,
                                    CLOSE};
  const SkColor kPath16Color = SK_ColorCYAN;
  const PathElement elements16[] = {CANVAS_DIMENSIONS,
                                    16,
                                    PATH_COLOR_ARGB,
                                    0xFF,
                                    SkColorGetR(kPath16Color),
                                    SkColorGetG(kPath16Color),
                                    SkColorGetB(kPath16Color),
                                    MOVE_TO,
                                    0,
                                    0,
                                    H_LINE_TO,
                                    16,
                                    V_LINE_TO,
                                    16,
                                    H_LINE_TO,
                                    0,
                                    V_LINE_TO,
                                    0,
                                    CLOSE};
  // VectorIconReps are always sorted in descending order of size.
  const VectorIconRep rep_list[] = {{elements48, std::size(elements48)},
                                    {elements32, std::size(elements32)},
                                    {elements24, std::size(elements24)},
                                    {elements20, std::size(elements20)},
                                    {elements16, std::size(elements16)}};
  const VectorIcon icon(rep_list, 5u, nullptr);

  // Test exact sizes paint the correctly sized icon, including the largest and
  // smallest icon.
  Canvas canvas_100(gfx::Size(100, 100), 1.0, true);
  PaintVectorIcon(&canvas_100, icon, 48, SK_ColorBLACK);
  EXPECT_EQ(kPath48Color, GetColorAtTopLeft(canvas_100));
  PaintVectorIcon(&canvas_100, icon, 32, SK_ColorBLACK);
  EXPECT_EQ(kPath32Color, GetColorAtTopLeft(canvas_100));
  PaintVectorIcon(&canvas_100, icon, 16, SK_ColorBLACK);
  EXPECT_EQ(kPath16Color, GetColorAtTopLeft(canvas_100));

  // The largest icon may be upscaled to a size larger than what it was
  // designed for.
  PaintVectorIcon(&canvas_100, icon, 50, SK_ColorBLACK);
  EXPECT_EQ(kPath48Color, GetColorAtTopLeft(canvas_100));

  // Other requests will be satisfied by downscaling.
  PaintVectorIcon(&canvas_100, icon, 27, SK_ColorBLACK);
  EXPECT_EQ(kPath32Color, GetColorAtTopLeft(canvas_100));
  PaintVectorIcon(&canvas_100, icon, 8, SK_ColorBLACK);
  EXPECT_EQ(kPath16Color, GetColorAtTopLeft(canvas_100));

  // Except in cases where an exact divisor is found.
  PaintVectorIcon(&canvas_100, icon, 40, SK_ColorBLACK);
  EXPECT_EQ(kPath20Color, GetColorAtTopLeft(canvas_100));
  PaintVectorIcon(&canvas_100, icon, 64, SK_ColorBLACK);
  EXPECT_EQ(kPath32Color, GetColorAtTopLeft(canvas_100));

  // Test icons at a scale factor < 100%, still with an exact size, paint the
  // correctly sized icon.
  Canvas canvas_75(gfx::Size(100, 100), 0.75, true);
  PaintVectorIcon(&canvas_75, icon, 32, SK_ColorBLACK);  // 32 * 0.75 = 24.
  EXPECT_EQ(kPath24Color, GetColorAtTopLeft(canvas_75));

  // Test icons at a scale factor > 100%, still with an exact size, paint the
  // correctly sized icon.
  Canvas canvas_125(gfx::Size(100, 100), 1.25, true);
  PaintVectorIcon(&canvas_125, icon, 16, SK_ColorBLACK);  // 16 * 1.25 = 20.
  EXPECT_EQ(kPath20Color, GetColorAtTopLeft(canvas_125));

  // Inexact sizes at scale factors < 100%.
  PaintVectorIcon(&canvas_75, icon, 12, SK_ColorBLACK);  // 12 * 0.75 = 9.
  EXPECT_EQ(kPath16Color, GetColorAtTopLeft(canvas_75));
  PaintVectorIcon(&canvas_75, icon, 28, SK_ColorBLACK);  // 28 * 0.75 = 21.
  EXPECT_EQ(kPath24Color, GetColorAtTopLeft(canvas_75));

  // Inexact sizes at scale factors > 100%.
  PaintVectorIcon(&canvas_125, icon, 12, SK_ColorBLACK);  // 12 * 1.25 = 15.
  EXPECT_EQ(kPath16Color, GetColorAtTopLeft(canvas_125));
  PaintVectorIcon(&canvas_125, icon, 28, SK_ColorBLACK);  // 28 * 1.25 = 35.
  EXPECT_EQ(kPath48Color, GetColorAtTopLeft(canvas_125));

  // Painting without a requested size will default to the smallest icon rep.
  PaintVectorIcon(&canvas_100, icon, SK_ColorBLACK);
  EXPECT_EQ(kPath16Color, GetColorAtTopLeft(canvas_100));
  // But doing this in another scale factor should assume the smallest icon rep
  // size, then scale it up by the DSF.
  PaintVectorIcon(&canvas_125, icon, SK_ColorBLACK);  // 16 * 1.25 = 20.
  EXPECT_EQ(kPath20Color, GetColorAtTopLeft(canvas_125));
}

}  // namespace

}  // namespace gfx
