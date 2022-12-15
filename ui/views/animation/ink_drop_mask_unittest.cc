// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_mask.h"

#include <algorithm>
#include <memory>

#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_record.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "ui/compositor/paint_context.h"

namespace views {

TEST(InkDropMaskTest, PathInkDropMaskPaintsTriangle) {
  gfx::Size layer_size(10, 10);
  SkPath path;
  SkPoint p1 = SkPoint::Make(3, 3);
  SkPoint p2 = SkPoint::Make(5, 6);
  SkPoint p3 = SkPoint::Make(8, 1);
  path.moveTo(p1.x(), p1.y());
  path.lineTo(p2.x(), p2.y());
  path.lineTo(p3.x(), p3.y());
  path.close();
  PathInkDropMask mask(layer_size, path);

  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  mask.OnPaintLayer(
      ui::PaintContext(list.get(), 1.f, gfx::Rect(layer_size), false));
  EXPECT_EQ(1u, list->num_paint_ops()) << list->ToString();

  sk_sp<cc::PaintRecord> record = list->FinalizeAndReleaseAsRecord();
  const auto* draw_record_op = record->GetOpAtForTesting<cc::DrawRecordOp>(0);
  ASSERT_NE(nullptr, draw_record_op);
  const auto* draw_op =
      draw_record_op->record->GetOpAtForTesting<cc::DrawPathOp>(0);
  ASSERT_NE(nullptr, draw_op);
  ASSERT_EQ(3, draw_op->path.countPoints());

  SkPoint points[3];
  ASSERT_EQ(3, draw_op->path.getPoints(points, 3));
  std::sort(points, points + 3,
            [](const SkPoint& a, const SkPoint& b) { return a.x() < b.x(); });
  EXPECT_EQ(p1, points[0]);
  EXPECT_EQ(p2, points[1]);
  EXPECT_EQ(p3, points[2]);
}

}  // namespace views
