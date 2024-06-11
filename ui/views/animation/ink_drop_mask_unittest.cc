// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_mask.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "cc/paint/paint_record.h"
#include "cc/test/paint_op_matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "ui/compositor/paint_context.h"

namespace views {

using ::cc::PaintOpIs;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::ExplainMatchResult;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAreArray;

MATCHER_P(PointsAre, expected, "") {
  int expected_size = expected.size();
  if (arg.countPoints() != expected_size) {
    *result_listener << "Expected path to have " << expected_size
                     << " points, but had " << arg.countPoints() << ".";
    return false;
  }

  std::vector<SkPoint> actual(expected_size);
  if (arg.getPoints(&actual.front(), expected_size) != expected_size) {
    *result_listener << "Failed extracting " << expected.size()
                     << " points from path.";
    return false;
  }

  return ExplainMatchResult(UnorderedElementsAreArray(expected), actual,
                            result_listener);
}

TEST(InkDropMaskTest, PathInkDropMaskPaintsTriangle) {
  gfx::Size layer_size(10, 10);
  SkPath path;
  std::vector<SkPoint> points = {SkPoint::Make(3, 3), SkPoint::Make(5, 6),
                                 SkPoint::Make(8, 1)};
  path.moveTo(points[0].x(), points[0].y());
  path.lineTo(points[1].x(), points[1].y());
  path.lineTo(points[2].x(), points[2].y());
  path.close();
  PathInkDropMask mask(layer_size, path);

  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  mask.OnPaintLayer(
      ui::PaintContext(list.get(), 1.f, gfx::Rect(layer_size), false));
  EXPECT_EQ(1u, list->num_paint_ops()) << list->ToString();

  cc::PaintRecord record = list->FinalizeAndReleaseAsRecordForTesting();
  EXPECT_THAT(
      record,
      ElementsAre(AllOf(
          PaintOpIs<cc::DrawRecordOp>(),
          ResultOf(
              [](const cc::PaintOp& op) {
                return static_cast<const cc::DrawRecordOp&>(op).record;
              },
              ElementsAre(AllOf(
                  PaintOpIs<cc::DrawPathOp>(),
                  ResultOf(
                      [](const cc::PaintOp& op) {
                        return static_cast<const cc::DrawPathOp&>(op).path;
                      },
                      PointsAre(points))))))));
}

}  // namespace views
