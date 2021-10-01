// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/rect_f.h"

#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/test/gfx_util.h"

namespace gfx {

TEST(RectFTest, Inset) {
  RectF r(10, 20, 30, 40);
  r.Inset(0);
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);
  r.Inset(1.5);
  EXPECT_RECTF_EQ(RectF(11.5, 21.5, 27, 37), r);
  r.Inset(-1.5);
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);

  r.Inset(1.5, 2.25);
  EXPECT_RECTF_EQ(RectF(11.5, 22.25, 27, 35.5), r);
  r.Inset(-1.5, -2.25);
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);

  // The parameters are left, top, right, bottom.
  r.Inset(1.5, 2.25, 3.75, 4);
  EXPECT_RECTF_EQ(RectF(11.5, 22.25, 24.75, 33.75), r);
  r.Inset(-1.5, -2.25, -3.75, -4);
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);

  // InsetsF parameters are top, right, bottom, left.
  r.Inset(InsetsF(1.5, 2.25, 3.75, 4));
  EXPECT_RECTF_EQ(RectF(12.25, 21.5, 23.75, 34.75), r);
  r.Inset(InsetsF(-1.5, -2.25, -3.75, -4));
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);
}

TEST(RectFTest, Outset) {
  RectF r(10, 20, 30, 40);
  r.Outset(0);
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);
  r.Outset(1.5);
  EXPECT_RECTF_EQ(RectF(8.5, 18.5, 33, 43), r);
  r.Outset(-1.5);
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);

  r.Outset(1.5, 2.25);
  EXPECT_RECTF_EQ(RectF(8.5, 17.75, 33, 44.5), r);
  r.Outset(-1.5, -2.25);
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);

  r.Outset(1.5, 2.25, 3.75, 4);
  EXPECT_RECTF_EQ(RectF(8.5, 17.75, 35.25, 46.25), r);
  r.Outset(-1.5, -2.25, -3.75, -4);
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);
}

TEST(RectFTest, InsetClamped) {
  RectF r(10, 20, 30, 40);
  r.Inset(18);
  EXPECT_RECTF_EQ(RectF(28, 38, 0, 4), r);
  r.Inset(-18);
  EXPECT_RECTF_EQ(RectF(10, 20, 36, 40), r);

  r.Inset(15, 30);
  EXPECT_RECTF_EQ(RectF(25, 50, 6, 0), r);
  r.Inset(-15, -30);
  EXPECT_RECTF_EQ(RectF(10, 20, 36, 60), r);

  r.Inset(20, 30, 40, 50);
  EXPECT_RECTF_EQ(RectF(30, 50, 0, 0), r);
  r.Inset(-20, -30, -40, -50);
  EXPECT_RECTF_EQ(RectF(10, 20, 60, 80), r);
}

}  // namespace gfx
