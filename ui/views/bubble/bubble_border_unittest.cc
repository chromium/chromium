// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_border.h"

#include <stddef.h>

#include <memory>

#include "base/strings/stringprintf.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/bubble/bubble_border_arrow_utils.h"
#include "ui/views/test/views_test_base.h"

namespace views {

using BubbleBorderTest = views::ViewsTestBase;

TEST_F(BubbleBorderTest, GetMirroredArrow) {
  // Horizontal mirroring.
  EXPECT_EQ(BubbleBorder::TOP_RIGHT,
            BubbleBorder::horizontal_mirror(BubbleBorder::TOP_LEFT));
  EXPECT_EQ(BubbleBorder::TOP_LEFT,
            BubbleBorder::horizontal_mirror(BubbleBorder::TOP_RIGHT));

  EXPECT_EQ(BubbleBorder::BOTTOM_RIGHT,
            BubbleBorder::horizontal_mirror(BubbleBorder::BOTTOM_LEFT));
  EXPECT_EQ(BubbleBorder::BOTTOM_LEFT,
            BubbleBorder::horizontal_mirror(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_EQ(BubbleBorder::RIGHT_TOP,
            BubbleBorder::horizontal_mirror(BubbleBorder::LEFT_TOP));
  EXPECT_EQ(BubbleBorder::LEFT_TOP,
            BubbleBorder::horizontal_mirror(BubbleBorder::RIGHT_TOP));

  EXPECT_EQ(BubbleBorder::RIGHT_BOTTOM,
            BubbleBorder::horizontal_mirror(BubbleBorder::LEFT_BOTTOM));
  EXPECT_EQ(BubbleBorder::LEFT_BOTTOM,
            BubbleBorder::horizontal_mirror(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_EQ(BubbleBorder::TOP_CENTER,
            BubbleBorder::horizontal_mirror(BubbleBorder::TOP_CENTER));
  EXPECT_EQ(BubbleBorder::BOTTOM_CENTER,
            BubbleBorder::horizontal_mirror(BubbleBorder::BOTTOM_CENTER));

  EXPECT_EQ(BubbleBorder::RIGHT_CENTER,
            BubbleBorder::horizontal_mirror(BubbleBorder::LEFT_CENTER));
  EXPECT_EQ(BubbleBorder::LEFT_CENTER,
            BubbleBorder::horizontal_mirror(BubbleBorder::RIGHT_CENTER));

  EXPECT_EQ(BubbleBorder::NONE,
            BubbleBorder::horizontal_mirror(BubbleBorder::NONE));
  EXPECT_EQ(BubbleBorder::FLOAT,
            BubbleBorder::horizontal_mirror(BubbleBorder::FLOAT));

  // Vertical mirroring.
  EXPECT_EQ(BubbleBorder::BOTTOM_LEFT,
            BubbleBorder::vertical_mirror(BubbleBorder::TOP_LEFT));
  EXPECT_EQ(BubbleBorder::BOTTOM_RIGHT,
            BubbleBorder::vertical_mirror(BubbleBorder::TOP_RIGHT));

  EXPECT_EQ(BubbleBorder::TOP_LEFT,
            BubbleBorder::vertical_mirror(BubbleBorder::BOTTOM_LEFT));
  EXPECT_EQ(BubbleBorder::TOP_RIGHT,
            BubbleBorder::vertical_mirror(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_EQ(BubbleBorder::LEFT_BOTTOM,
            BubbleBorder::vertical_mirror(BubbleBorder::LEFT_TOP));
  EXPECT_EQ(BubbleBorder::RIGHT_BOTTOM,
            BubbleBorder::vertical_mirror(BubbleBorder::RIGHT_TOP));

  EXPECT_EQ(BubbleBorder::LEFT_TOP,
            BubbleBorder::vertical_mirror(BubbleBorder::LEFT_BOTTOM));
  EXPECT_EQ(BubbleBorder::RIGHT_TOP,
            BubbleBorder::vertical_mirror(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_EQ(BubbleBorder::BOTTOM_CENTER,
            BubbleBorder::vertical_mirror(BubbleBorder::TOP_CENTER));
  EXPECT_EQ(BubbleBorder::TOP_CENTER,
            BubbleBorder::vertical_mirror(BubbleBorder::BOTTOM_CENTER));

  EXPECT_EQ(BubbleBorder::LEFT_CENTER,
            BubbleBorder::vertical_mirror(BubbleBorder::LEFT_CENTER));
  EXPECT_EQ(BubbleBorder::RIGHT_CENTER,
            BubbleBorder::vertical_mirror(BubbleBorder::RIGHT_CENTER));

  EXPECT_EQ(BubbleBorder::NONE,
            BubbleBorder::vertical_mirror(BubbleBorder::NONE));
  EXPECT_EQ(BubbleBorder::FLOAT,
            BubbleBorder::vertical_mirror(BubbleBorder::FLOAT));
}

TEST_F(BubbleBorderTest, HasArrow) {
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::TOP_LEFT));
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::TOP_RIGHT));

  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::BOTTOM_LEFT));
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::LEFT_TOP));
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::RIGHT_TOP));

  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::LEFT_BOTTOM));
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::TOP_CENTER));
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::BOTTOM_CENTER));

  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::LEFT_CENTER));
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::RIGHT_CENTER));

  EXPECT_FALSE(BubbleBorder::has_arrow(BubbleBorder::NONE));
  EXPECT_FALSE(BubbleBorder::has_arrow(BubbleBorder::FLOAT));
}

TEST_F(BubbleBorderTest, IsArrowOnLeft) {
  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(BubbleBorder::TOP_LEFT));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::TOP_RIGHT));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(BubbleBorder::BOTTOM_LEFT));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(BubbleBorder::LEFT_TOP));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::RIGHT_TOP));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(BubbleBorder::LEFT_BOTTOM));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::TOP_CENTER));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::BOTTOM_CENTER));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(BubbleBorder::LEFT_CENTER));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::RIGHT_CENTER));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::NONE));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::FLOAT));
}

TEST_F(BubbleBorderTest, IsArrowOnTop) {
  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(BubbleBorder::TOP_LEFT));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(BubbleBorder::TOP_RIGHT));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::BOTTOM_LEFT));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(BubbleBorder::LEFT_TOP));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(BubbleBorder::RIGHT_TOP));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::LEFT_BOTTOM));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(BubbleBorder::TOP_CENTER));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::BOTTOM_CENTER));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::LEFT_CENTER));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::RIGHT_CENTER));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::NONE));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::FLOAT));
}

TEST_F(BubbleBorderTest, IsArrowOnHorizontal) {
  EXPECT_TRUE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::TOP_LEFT));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::TOP_RIGHT));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::BOTTOM_LEFT));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::LEFT_TOP));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::RIGHT_TOP));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::LEFT_BOTTOM));
  EXPECT_FALSE(
      BubbleBorder::is_arrow_on_horizontal(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::TOP_CENTER));
  EXPECT_TRUE(
      BubbleBorder::is_arrow_on_horizontal(BubbleBorder::BOTTOM_CENTER));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::LEFT_CENTER));
  EXPECT_FALSE(
      BubbleBorder::is_arrow_on_horizontal(BubbleBorder::RIGHT_CENTER));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::NONE));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::FLOAT));
}

TEST_F(BubbleBorderTest, IsArrowAtCenter) {
  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::TOP_LEFT));
  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::TOP_RIGHT));

  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::BOTTOM_LEFT));
  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::LEFT_TOP));
  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::RIGHT_TOP));

  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::LEFT_BOTTOM));
  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_TRUE(BubbleBorder::is_arrow_at_center(BubbleBorder::TOP_CENTER));
  EXPECT_TRUE(BubbleBorder::is_arrow_at_center(BubbleBorder::BOTTOM_CENTER));

  EXPECT_TRUE(BubbleBorder::is_arrow_at_center(BubbleBorder::LEFT_CENTER));
  EXPECT_TRUE(BubbleBorder::is_arrow_at_center(BubbleBorder::RIGHT_CENTER));

  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::NONE));
  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::FLOAT));
}

TEST_F(BubbleBorderTest, GetSizeForContentsSizeTest) {
  views::BubbleBorder border(BubbleBorder::NONE, BubbleBorder::NO_SHADOW);

  const gfx::Insets kInsets = border.GetInsets();

  // kSmallSize is smaller than the minimum allowable size and does not
  // contribute to the resulting size.
  const gfx::Size kSmallSize = gfx::Size(1, 2);
  // kMediumSize is larger than the minimum allowable size and contributes to
  // the resulting size.
  const gfx::Size kMediumSize = gfx::Size(50, 60);

  const gfx::Size kSmallHorizArrow(kSmallSize.width() + kInsets.width(),
                                   kSmallSize.height() + kInsets.height());

  const gfx::Size kSmallVertArrow(kSmallHorizArrow.width(),
                                  kSmallHorizArrow.height());

  const gfx::Size kSmallNoArrow(kSmallHorizArrow.width(),
                                kSmallHorizArrow.height());

  const gfx::Size kMediumHorizArrow(kMediumSize.width() + kInsets.width(),
                                    kMediumSize.height() + kInsets.height());

  const gfx::Size kMediumVertArrow(kMediumHorizArrow.width(),
                                   kMediumHorizArrow.height());

  const gfx::Size kMediumNoArrow(kMediumHorizArrow.width(),
                                 kMediumHorizArrow.height());

  struct TestCase {
    BubbleBorder::Arrow arrow;
    gfx::Size content;
    gfx::Size expected_without_arrow;
  };

  const auto cases = std::to_array<TestCase>(
      {// Content size: kSmallSize
       {BubbleBorder::TOP_LEFT, kSmallSize, kSmallNoArrow},
       {BubbleBorder::TOP_CENTER, kSmallSize, kSmallNoArrow},
       {BubbleBorder::TOP_RIGHT, kSmallSize, kSmallNoArrow},
       {BubbleBorder::BOTTOM_LEFT, kSmallSize, kSmallNoArrow},
       {BubbleBorder::BOTTOM_CENTER, kSmallSize, kSmallNoArrow},
       {BubbleBorder::BOTTOM_RIGHT, kSmallSize, kSmallNoArrow},
       {BubbleBorder::LEFT_TOP, kSmallSize, kSmallNoArrow},
       {BubbleBorder::LEFT_CENTER, kSmallSize, kSmallNoArrow},
       {BubbleBorder::LEFT_BOTTOM, kSmallSize, kSmallNoArrow},
       {BubbleBorder::RIGHT_TOP, kSmallSize, kSmallNoArrow},
       {BubbleBorder::RIGHT_CENTER, kSmallSize, kSmallNoArrow},
       {BubbleBorder::RIGHT_BOTTOM, kSmallSize, kSmallNoArrow},
       {BubbleBorder::NONE, kSmallSize, kSmallNoArrow},
       {BubbleBorder::FLOAT, kSmallSize, kSmallNoArrow},

       // Content size: kMediumSize
       {BubbleBorder::TOP_LEFT, kMediumSize, kMediumNoArrow},
       {BubbleBorder::TOP_CENTER, kMediumSize, kMediumNoArrow},
       {BubbleBorder::TOP_RIGHT, kMediumSize, kMediumNoArrow},
       {BubbleBorder::BOTTOM_LEFT, kMediumSize, kMediumNoArrow},
       {BubbleBorder::BOTTOM_CENTER, kMediumSize, kMediumNoArrow},
       {BubbleBorder::BOTTOM_RIGHT, kMediumSize, kMediumNoArrow},
       {BubbleBorder::LEFT_TOP, kMediumSize, kMediumNoArrow},
       {BubbleBorder::LEFT_CENTER, kMediumSize, kMediumNoArrow},
       {BubbleBorder::LEFT_BOTTOM, kMediumSize, kMediumNoArrow},
       {BubbleBorder::RIGHT_TOP, kMediumSize, kMediumNoArrow},
       {BubbleBorder::RIGHT_CENTER, kMediumSize, kMediumNoArrow},
       {BubbleBorder::RIGHT_BOTTOM, kMediumSize, kMediumNoArrow},
       {BubbleBorder::NONE, kMediumSize, kMediumNoArrow},
       {BubbleBorder::FLOAT, kMediumSize, kMediumNoArrow}});

  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("i=%d arrow=%d", static_cast<int>(i),
                                    cases[i].arrow));

    border.set_arrow(cases[i].arrow);
    EXPECT_EQ(cases[i].expected_without_arrow,
              border.GetSizeForContentsSize(cases[i].content));
  }
}

TEST_F(BubbleBorderTest, GetBoundsOriginTest) {
  for (int i = 0; i < BubbleBorder::SHADOW_COUNT; ++i) {
    const BubbleBorder::Shadow shadow = static_cast<BubbleBorder::Shadow>(i);
    SCOPED_TRACE(testing::Message() << "BubbleBorder::Shadow: " << shadow);
    views::BubbleBorder border(BubbleBorder::TOP_LEFT, shadow);

    const gfx::Rect kAnchor(100, 100, 20, 30);
    const gfx::Size kContentSize(500, 600);
    const gfx::Insets kInsets = border.GetInsets();

    border.set_arrow(BubbleBorder::TOP_LEFT);
    const gfx::Size kTotalSize = border.GetSizeForContentsSize(kContentSize);

    border.set_arrow(BubbleBorder::RIGHT_BOTTOM);
    EXPECT_EQ(kTotalSize, border.GetSizeForContentsSize(kContentSize));

    border.set_arrow(BubbleBorder::NONE);
    EXPECT_EQ(kTotalSize, border.GetSizeForContentsSize(kContentSize));

    const int kStrokeWidth =
        shadow == BubbleBorder::NO_SHADOW ? 0 : BubbleBorder::kStroke;

    const int kBorderedContentHeight =
        kContentSize.height() + (2 * kStrokeWidth);

    const int kStrokeTopInset = kStrokeWidth - kInsets.top();
    const int kStrokeBottomInset = kStrokeWidth - kInsets.bottom();
    const int kStrokeLeftInset = kStrokeWidth - kInsets.left();
    const int kStrokeRightInset = kStrokeWidth - kInsets.right();

    const int kTopHorizArrowY = kAnchor.bottom() + kStrokeTopInset;
    const int kBottomHorizArrowY =
        kAnchor.y() - kTotalSize.height() - kStrokeBottomInset;
    const int kLeftVertArrowX =
        kAnchor.x() + kAnchor.width() + kStrokeLeftInset;
    const int kRightVertArrowX =
        kAnchor.x() - kTotalSize.width() - kStrokeRightInset;

    struct TestCase {
      BubbleBorder::Arrow arrow;
      int expected_x;
      int expected_y;
    };

    const auto cases = std::to_array<TestCase>({
        // Horizontal arrow tests.
        {BubbleBorder::TOP_LEFT, kAnchor.x() + kStrokeLeftInset,
         kTopHorizArrowY},
        {BubbleBorder::TOP_CENTER,
         kAnchor.CenterPoint().x() - (kTotalSize.width() / 2), kTopHorizArrowY},
        {BubbleBorder::BOTTOM_RIGHT,
         kAnchor.x() + kAnchor.width() - kTotalSize.width() - kStrokeRightInset,
         kBottomHorizArrowY},

        // Vertical arrow tests.
        {BubbleBorder::LEFT_TOP, kLeftVertArrowX,
         kAnchor.y() + kStrokeTopInset},
        {BubbleBorder::LEFT_CENTER, kLeftVertArrowX,
         kAnchor.CenterPoint().y() - (kBorderedContentHeight / 2) +
             kStrokeTopInset},
        {BubbleBorder::RIGHT_BOTTOM, kRightVertArrowX,
         kAnchor.y() + kAnchor.height() - kTotalSize.height() -
             kStrokeBottomInset},

        // No arrow tests.
        {BubbleBorder::NONE,
         kAnchor.x() + (kAnchor.width() - kTotalSize.width()) / 2,
         kAnchor.y() + kAnchor.height()},
        {BubbleBorder::FLOAT,
         kAnchor.x() + (kAnchor.width() - kTotalSize.width()) / 2,
         kAnchor.y() + (kAnchor.height() - kTotalSize.height()) / 2},
    });

    for (size_t j = 0; j < std::size(cases); ++j) {
      SCOPED_TRACE(base::StringPrintf("shadow=%d j=%d arrow=%d",
                                      static_cast<int>(shadow),
                                      static_cast<int>(j), cases[j].arrow));
      const BubbleBorder::Arrow arrow = cases[j].arrow;
      border.set_arrow(arrow);
      gfx::Point origin = border.GetBounds(kAnchor, kContentSize).origin();
      EXPECT_EQ(cases[j].expected_x, origin.x());
      EXPECT_EQ(cases[j].expected_y, origin.y());
    }
  }
}

TEST_F(BubbleBorderTest, BubblePositionedCorrectlyWithVisibleArrow) {
  views::BubbleBorder border(BubbleBorder::TOP_LEFT,
                             BubbleBorder::STANDARD_SHADOW);
  const gfx::Insets kInsets = border.GetInsets();
  border.set_visible_arrow(true);

  constexpr gfx::Size kContentSize(200, 150);

  // Anchor position.
  constexpr gfx::Point kAnchorOrigin(100, 100);

  // Anchor smaller than contents.
  constexpr gfx::Rect kAnchor1(kAnchorOrigin, gfx::Size(40, 50));

  // Anchor larger than contents.
  constexpr gfx::Rect kAnchor2(kAnchorOrigin, gfx::Size(400, 300));

  // Anchor extremely small.
  constexpr gfx::Rect kAnchor3(kAnchorOrigin, gfx::Size(10, 12));

  // TODO(dfried): in all of these tests, the border is factored into the height
  // of the bubble an extra time, because of the fact that the arrow doesn't
  // properly overlap the border in the calculation (though it does visually).
  // Please fix at some point.

  // TOP_LEFT:

  border.set_arrow(BubbleBorder::TOP_LEFT);
  gfx::Rect bounds = border.GetBounds(kAnchor1, kContentSize);
  EXPECT_EQ(kContentSize.height() + kInsets.bottom() +
                BubbleBorder::kVisibleArrowLength,
            bounds.height());
  EXPECT_EQ(kContentSize.width() + kInsets.width(), bounds.width());
  EXPECT_EQ(kAnchor1.bottom() + BubbleBorder::kVisibleArrowGap +
                BubbleBorder::kBorderThicknessDip,
            bounds.y());
  EXPECT_EQ(kAnchor1.x() - kInsets.left(), bounds.x());

  bounds = border.GetBounds(kAnchor2, kContentSize);
  EXPECT_EQ(kContentSize.height() + kInsets.bottom() +
                BubbleBorder::kVisibleArrowLength,
            bounds.height());
  EXPECT_EQ(kContentSize.width() + kInsets.width(), bounds.width());
  EXPECT_EQ(kAnchor2.bottom() + BubbleBorder::kVisibleArrowGap +
                BubbleBorder::kBorderThicknessDip,
            bounds.y());
  EXPECT_EQ(kAnchor2.x() - kInsets.left() + BubbleBorder::kBorderThicknessDip,
            bounds.x());

  // Too small of an anchor view will shift the bubble to make sure the arrow
  // is not too close to the edge of the bubble.
  bounds = border.GetBounds(kAnchor3, kContentSize);
  EXPECT_EQ(kContentSize.height() + kInsets.bottom() +
                BubbleBorder::kVisibleArrowLength,
            bounds.height());
  EXPECT_EQ(kContentSize.width() + kInsets.width(), bounds.width());
  EXPECT_EQ(kAnchor3.bottom() + BubbleBorder::kVisibleArrowGap +
                BubbleBorder::kBorderThicknessDip,
            bounds.y());
  EXPECT_GT(kAnchor3.x() - kInsets.left() + BubbleBorder::kBorderThicknessDip,
            bounds.x());

  // TOP_CENTER:

  border.set_arrow(BubbleBorder::TOP_CENTER);
  bounds = border.GetBounds(kAnchor1, kContentSize);
  EXPECT_EQ(kContentSize.height() + kInsets.bottom() +
                BubbleBorder::kVisibleArrowLength,
            bounds.height());
  EXPECT_EQ(kContentSize.width() + kInsets.width(), bounds.width());
  EXPECT_EQ(kAnchor1.bottom() + BubbleBorder::kVisibleArrowGap +
                BubbleBorder::kBorderThicknessDip,
            bounds.y());
  EXPECT_EQ(kAnchor1.bottom_center().x() - bounds.width() / 2, bounds.x());

  bounds = border.GetBounds(kAnchor2, kContentSize);
  EXPECT_EQ(kContentSize.height() + kInsets.bottom() +
                BubbleBorder::kVisibleArrowLength,
            bounds.height());
  EXPECT_EQ(kContentSize.width() + kInsets.width(), bounds.width());
  EXPECT_EQ(kAnchor2.bottom() + BubbleBorder::kVisibleArrowGap +
                BubbleBorder::kBorderThicknessDip,
            bounds.y());
  EXPECT_EQ(kAnchor2.bottom_center().x() - bounds.width() / 2, bounds.x());

  bounds = border.GetBounds(kAnchor3, kContentSize);
  EXPECT_EQ(kContentSize.height() + kInsets.bottom() +
                BubbleBorder::kVisibleArrowLength,
            bounds.height());
  EXPECT_EQ(kContentSize.width() + kInsets.width(), bounds.width());
  EXPECT_EQ(kAnchor3.bottom() + BubbleBorder::kVisibleArrowGap +
                BubbleBorder::kBorderThicknessDip,
            bounds.y());
  EXPECT_EQ(kAnchor3.bottom_center().x() - bounds.width() / 2, bounds.x());

  // TOP_RIGHT:

  border.set_arrow(BubbleBorder::TOP_RIGHT);
  bounds = border.GetBounds(kAnchor1, kContentSize);
  EXPECT_EQ(kContentSize.height() + kInsets.bottom() +
                BubbleBorder::kVisibleArrowLength,
            bounds.height());
  EXPECT_EQ(kContentSize.width() + kInsets.width(), bounds.width());
  EXPECT_EQ(kAnchor1.bottom() + BubbleBorder::kVisibleArrowGap +
                BubbleBorder::kBorderThicknessDip,
            bounds.y());
  EXPECT_EQ(kAnchor1.right() + kInsets.right(), bounds.right());

  bounds = border.GetBounds(kAnchor2, kContentSize);
  EXPECT_EQ(kContentSize.height() + kInsets.bottom() +
                BubbleBorder::kVisibleArrowLength,
            bounds.height());
  EXPECT_EQ(kContentSize.width() + kInsets.width(), bounds.width());
  EXPECT_EQ(kAnchor2.bottom() + BubbleBorder::kVisibleArrowGap +
                BubbleBorder::kBorderThicknessDip,
            bounds.y());
  EXPECT_EQ(
      kAnchor2.right() + kInsets.right() - BubbleBorder::kBorderThicknessDip,
      bounds.right());

  // Too small of an anchor view will shift the bubble to make sure the arrow
  // is not too close to the edge of the bubble.
  bounds = border.GetBounds(kAnchor3, kContentSize);
  EXPECT_EQ(kContentSize.height() + kInsets.bottom() +
                BubbleBorder::kVisibleArrowLength,
            bounds.height());
  EXPECT_EQ(kContentSize.width() + kInsets.width(), bounds.width());
  EXPECT_EQ(kAnchor3.bottom() + BubbleBorder::kVisibleArrowGap +
                BubbleBorder::kBorderThicknessDip,
            bounds.y());
  EXPECT_LT(
      kAnchor3.right() + kInsets.right() - BubbleBorder::kBorderThicknessDip,
      bounds.right());

  // BOTTOM_LEFT:

  border.set_arrow(BubbleBorder::BOTTOM_LEFT);
  bounds = border.GetBounds(kAnchor1, kContentSize);
  EXPECT_EQ(kContentSize.height() + kInsets.top() +
                BubbleBorder::kVisibleArrowLength +
                BubbleBorder::kBorderThicknessDip,
            bounds.height());
  EXPECT_EQ(kContentSize.width() + kInsets.width(), bounds.width());
  EXPECT_EQ(kAnchor1.y() - BubbleBorder::kVisibleArrowGap, bounds.bottom());
  EXPECT_EQ(kAnchor1.x() - kInsets.left(), bounds.x());

  bounds = border.GetBounds(kAnchor2, kContentSize);
  EXPECT_EQ(kContentSize.height() + kInsets.top() +
                BubbleBorder::kVisibleArrowLength +
                BubbleBorder::kBorderThicknessDip,
            bounds.height());
  EXPECT_EQ(kContentSize.width() + kInsets.width(), bounds.width());
  EXPECT_EQ(kAnchor2.y() - BubbleBorder::kVisibleArrowGap, bounds.bottom());
  EXPECT_EQ(kAnchor2.x() - kInsets.left() + BubbleBorder::kBorderThicknessDip,
            bounds.x());

  // Too small of an anchor view will shift the bubble to make sure the arrow
  // is not too close to the edge of the bubble.
  bounds = border.GetBounds(kAnchor3, kContentSize);
  EXPECT_EQ(kContentSize.height() + kInsets.top() +
                BubbleBorder::kVisibleArrowLength +
                BubbleBorder::kBorderThicknessDip,
            bounds.height());
  EXPECT_EQ(kContentSize.width() + kInsets.width(), bounds.width());
  EXPECT_EQ(kAnchor3.y() - BubbleBorder::kVisibleArrowGap, bounds.bottom());
  EXPECT_GT(kAnchor3.x() - kInsets.left() + BubbleBorder::kBorderThicknessDip,
            bounds.x());

  // BOTTOM_CENTER:

  border.set_arrow(BubbleBorder::BOTTOM_CENTER);
  bounds = border.GetBounds(kAnchor1, kContentSize);
  EXPECT_EQ(kContentSize.height() + kInsets.top() +
                BubbleBorder::kVisibleArrowLength +
                BubbleBorder::kBorderThicknessDip,
            bounds.height());
  EXPECT_EQ(kContentSize.width() + kInsets.width(), bounds.width());
  EXPECT_EQ(kAnchor1.y() - BubbleBorder::kVisibleArrowGap, bounds.bottom());
  EXPECT_EQ(kAnchor1.bottom_center().x() - bounds.width() / 2, bounds.x());

  bounds = border.GetBounds(kAnchor2, kContentSize);
  EXPECT_EQ(kContentSize.height() + kInsets.top() +
                BubbleBorder::kVisibleArrowLength +
                BubbleBorder::kBorderThicknessDip,
            bounds.height());
  EXPECT_EQ(kContentSize.width() + kInsets.width(), bounds.width());
  EXPECT_EQ(kAnchor2.y() - BubbleBorder::kVisibleArrowGap, bounds.bottom());
  EXPECT_EQ(kAnchor2.bottom_center().x() - bounds.width() / 2, bounds.x());

  bounds = border.GetBounds(kAnchor3, kContentSize);
  EXPECT_EQ(kContentSize.height() + kInsets.top() +
                BubbleBorder::kVisibleArrowLength +
                BubbleBorder::kBorderThicknessDip,
            bounds.height());
  EXPECT_EQ(kContentSize.width() + kInsets.width(), bounds.width());
  EXPECT_EQ(kAnchor3.y() - BubbleBorder::kVisibleArrowGap, bounds.bottom());
  EXPECT_EQ(kAnchor3.bottom_center().x() - bounds.width() / 2, bounds.x());

  // BOTTOM_RIGHT:

  border.set_arrow(BubbleBorder::BOTTOM_RIGHT);
  bounds = border.GetBounds(kAnchor1, kContentSize);
  EXPECT_EQ(kContentSize.height() + kInsets.top() +
                BubbleBorder::kVisibleArrowLength +
                BubbleBorder::kBorderThicknessDip,
            bounds.height());
  EXPECT_EQ(kContentSize.width() + kInsets.width(), bounds.width());
  EXPECT_EQ(kAnchor1.y() - BubbleBorder::kVisibleArrowGap, bounds.bottom());
  EXPECT_EQ(kAnchor1.right() + kInsets.right(), bounds.right());

  bounds = border.GetBounds(kAnchor2, kContentSize);
  EXPECT_EQ(kContentSize.height() + kInsets.top() +
                BubbleBorder::kVisibleArrowLength +
                BubbleBorder::kBorderThicknessDip,
            bounds.height());
  EXPECT_EQ(kContentSize.width() + kInsets.width(), bounds.width());
  EXPECT_EQ(kAnchor2.y() - BubbleBorder::kVisibleArrowGap, bounds.bottom());
  EXPECT_EQ(
      kAnchor2.right() + kInsets.right() - BubbleBorder::kBorderThicknessDip,
      bounds.right());

  // Too small of an anchor view will shift the bubble to make sure the arrow
  // is not too close to the edge of the bubble.
  bounds = border.GetBounds(kAnchor3, kContentSize);
  EXPECT_EQ(kContentSize.height() + kInsets.top() +
                BubbleBorder::kVisibleArrowLength +
                BubbleBorder::kBorderThicknessDip,
            bounds.height());
  EXPECT_EQ(kContentSize.width() + kInsets.width(), bounds.width());
  EXPECT_EQ(kAnchor3.y() - BubbleBorder::kVisibleArrowGap, bounds.bottom());
  EXPECT_LT(
      kAnchor3.right() + kInsets.right() - BubbleBorder::kBorderThicknessDip,
      bounds.right());

  // LEFT_TOP:

  border.set_arrow(BubbleBorder::LEFT_TOP);
  bounds = border.GetBounds(kAnchor1, kContentSize);
  EXPECT_EQ(kContentSize.width() + kInsets.right() +
                BubbleBorder::kVisibleArrowLength,
            bounds.width());
  EXPECT_EQ(kContentSize.height() + kInsets.height(), bounds.height());
  EXPECT_EQ(kAnchor1.right() + BubbleBorder::kVisibleArrowGap +
                BubbleBorder::kBorderThicknessDip,
            bounds.x());
  EXPECT_EQ(kAnchor1.y() - kInsets.top() + BubbleBorder::kBorderThicknessDip,
            bounds.y());

  bounds = border.GetBounds(kAnchor2, kContentSize);
  EXPECT_EQ(kContentSize.width() + kInsets.right() +
                BubbleBorder::kVisibleArrowLength,
            bounds.width());
  EXPECT_EQ(kContentSize.height() + kInsets.height(), bounds.height());
  EXPECT_EQ(kAnchor2.right() + BubbleBorder::kVisibleArrowGap +
                BubbleBorder::kBorderThicknessDip,
            bounds.x());
  EXPECT_EQ(kAnchor2.y() - kInsets.top() + BubbleBorder::kBorderThicknessDip,
            bounds.y());

  // Too small of an anchor view will shift the bubble to make sure the arrow
  // is not too close to the edge of the bubble.
  bounds = border.GetBounds(kAnchor3, kContentSize);
  EXPECT_EQ(kContentSize.width() + kInsets.right() +
                BubbleBorder::kVisibleArrowLength,
            bounds.width());
  EXPECT_EQ(kContentSize.height() + kInsets.height(), bounds.height());
  EXPECT_EQ(kAnchor3.right() + BubbleBorder::kVisibleArrowGap +
                BubbleBorder::kBorderThicknessDip,
            bounds.x());
  EXPECT_GT(kAnchor3.y() - kInsets.top() + BubbleBorder::kBorderThicknessDip,
            bounds.y());

  // LEFT_CENTER:

  // Because the shadow counts as part of the bounds, the shadow offset (which
  // is applied vertically) will affect the vertical positioning of a bubble
  // which is placed next to the anchor by a similar amount.
  border.set_arrow(BubbleBorder::LEFT_CENTER);
  const auto insets = border.GetInsets();
  const int shadow_offset = (insets.bottom() - insets.top()) / 2;
  bounds = border.GetBounds(kAnchor1, kContentSize);
  EXPECT_EQ(kContentSize.width() + kInsets.right() +
                BubbleBorder::kVisibleArrowLength,
            bounds.width());
  EXPECT_EQ(kContentSize.height() + kInsets.height(), bounds.height());
  EXPECT_EQ(kAnchor1.right() + BubbleBorder::kVisibleArrowGap +
                BubbleBorder::kBorderThicknessDip,
            bounds.x());
  EXPECT_NEAR(kAnchor1.right_center().y() - bounds.height() / 2, bounds.y(),
              shadow_offset);

  bounds = border.GetBounds(kAnchor2, kContentSize);
  EXPECT_EQ(kContentSize.width() + kInsets.right() +
                BubbleBorder::kVisibleArrowLength,
            bounds.width());
  EXPECT_EQ(kContentSize.height() + kInsets.height(), bounds.height());
  EXPECT_EQ(kAnchor2.right() + BubbleBorder::kVisibleArrowGap +
                BubbleBorder::kBorderThicknessDip,
            bounds.x());
  EXPECT_NEAR(kAnchor2.right_center().y() - bounds.height() / 2, bounds.y(),
              shadow_offset);

  bounds = border.GetBounds(kAnchor3, kContentSize);
  EXPECT_EQ(kContentSize.width() + kInsets.right() +
                BubbleBorder::kVisibleArrowLength,
            bounds.width());
  EXPECT_EQ(kContentSize.height() + kInsets.height(), bounds.height());
  EXPECT_EQ(kAnchor3.right() + BubbleBorder::kVisibleArrowGap +
                BubbleBorder::kBorderThicknessDip,
            bounds.x());
  EXPECT_NEAR(kAnchor3.right_center().y() - bounds.height() / 2, bounds.y(),
              shadow_offset);

  // LEFT_BOTTOM:

  border.set_arrow(BubbleBorder::LEFT_BOTTOM);
  bounds = border.GetBounds(kAnchor1, kContentSize);
  EXPECT_EQ(kContentSize.width() + kInsets.right() +
                BubbleBorder::kVisibleArrowLength,
            bounds.width());
  EXPECT_EQ(kContentSize.height() + kInsets.height(), bounds.height());
  EXPECT_EQ(kAnchor1.right() + BubbleBorder::kVisibleArrowGap +
                BubbleBorder::kBorderThicknessDip,
            bounds.x());
  EXPECT_EQ(
      kAnchor1.bottom() + kInsets.bottom() - BubbleBorder::kBorderThicknessDip,
      bounds.bottom());

  bounds = border.GetBounds(kAnchor2, kContentSize);
  EXPECT_EQ(kContentSize.width() + kInsets.right() +
                BubbleBorder::kVisibleArrowLength,
            bounds.width());
  EXPECT_EQ(kContentSize.height() + kInsets.height(), bounds.height());
  EXPECT_EQ(kAnchor2.right() + BubbleBorder::kVisibleArrowGap +
                BubbleBorder::kBorderThicknessDip,
            bounds.x());
  EXPECT_EQ(
      kAnchor2.bottom() + kInsets.bottom() - BubbleBorder::kBorderThicknessDip,
      bounds.bottom());

  // Too small of an anchor view will shift the bubble to make sure the arrow
  // is not too close to the edge of the bubble.
  bounds = border.GetBounds(kAnchor3, kContentSize);
  EXPECT_EQ(kContentSize.width() + kInsets.right() +
                BubbleBorder::kVisibleArrowLength,
            bounds.width());
  EXPECT_EQ(kContentSize.height() + kInsets.height(), bounds.height());
  EXPECT_EQ(kAnchor3.right() + BubbleBorder::kVisibleArrowGap +
                BubbleBorder::kBorderThicknessDip,
            bounds.x());
  EXPECT_LT(
      kAnchor3.bottom() + kInsets.bottom() - BubbleBorder::kBorderThicknessDip,
      bounds.bottom());

  // RIGHT_TOP:

  border.set_arrow(BubbleBorder::RIGHT_TOP);
  bounds = border.GetBounds(kAnchor1, kContentSize);
  EXPECT_EQ(kContentSize.width() + kInsets.right() +
                BubbleBorder::kVisibleArrowLength,
            bounds.width());
  EXPECT_EQ(kContentSize.height() + kInsets.height(), bounds.height());
  EXPECT_EQ(kAnchor1.x() - BubbleBorder::kVisibleArrowGap -
                BubbleBorder::kBorderThicknessDip,
            bounds.right());
  EXPECT_EQ(kAnchor1.y() - kInsets.top() + BubbleBorder::kBorderThicknessDip,
            bounds.y());

  bounds = border.GetBounds(kAnchor2, kContentSize);
  EXPECT_EQ(kContentSize.width() + kInsets.right() +
                BubbleBorder::kVisibleArrowLength,
            bounds.width());
  EXPECT_EQ(kContentSize.height() + kInsets.height(), bounds.height());
  EXPECT_EQ(kAnchor2.x() - BubbleBorder::kVisibleArrowGap -
                BubbleBorder::kBorderThicknessDip,
            bounds.right());
  EXPECT_EQ(kAnchor2.y() - kInsets.top() + BubbleBorder::kBorderThicknessDip,
            bounds.y());

  // Too small of an anchor view will shift the bubble to make sure the arrow
  // is not too close to the edge of the bubble.
  bounds = border.GetBounds(kAnchor3, kContentSize);
  EXPECT_EQ(kContentSize.width() + kInsets.right() +
                BubbleBorder::kVisibleArrowLength,
            bounds.width());
  EXPECT_EQ(kContentSize.height() + kInsets.height(), bounds.height());
  EXPECT_EQ(kAnchor3.x() - BubbleBorder::kVisibleArrowGap -
                BubbleBorder::kBorderThicknessDip,
            bounds.right());
  EXPECT_GT(kAnchor3.y() - kInsets.top() + BubbleBorder::kBorderThicknessDip,
            bounds.y());

  // // RIGHT_CENTER:

  // Because the shadow counts as part of the bounds, the shadow offset (which
  // is applied vertically) will affect the vertical positioning of a bubble
  // which is placed next to the anchor by a similar amount.
  border.set_arrow(BubbleBorder::RIGHT_CENTER);
  bounds = border.GetBounds(kAnchor1, kContentSize);
  EXPECT_EQ(kContentSize.width() + kInsets.right() +
                BubbleBorder::kVisibleArrowLength,
            bounds.width());
  EXPECT_EQ(kContentSize.height() + kInsets.height(), bounds.height());
  EXPECT_EQ(kAnchor1.x() - BubbleBorder::kVisibleArrowGap -
                BubbleBorder::kBorderThicknessDip,
            bounds.right());
  EXPECT_NEAR(kAnchor1.right_center().y() - bounds.height() / 2, bounds.y(),
              shadow_offset);

  bounds = border.GetBounds(kAnchor2, kContentSize);
  EXPECT_EQ(kContentSize.width() + kInsets.right() +
                BubbleBorder::kVisibleArrowLength,
            bounds.width());
  EXPECT_EQ(kContentSize.height() + kInsets.height(), bounds.height());
  EXPECT_EQ(kAnchor2.x() - BubbleBorder::kVisibleArrowGap -
                BubbleBorder::kBorderThicknessDip,
            bounds.right());
  EXPECT_NEAR(kAnchor2.right_center().y() - bounds.height() / 2, bounds.y(),
              shadow_offset);

  bounds = border.GetBounds(kAnchor3, kContentSize);
  EXPECT_EQ(kContentSize.width() + kInsets.right() +
                BubbleBorder::kVisibleArrowLength,
            bounds.width());
  EXPECT_EQ(kContentSize.height() + kInsets.height(), bounds.height());
  EXPECT_EQ(kAnchor3.x() - BubbleBorder::kVisibleArrowGap -
                BubbleBorder::kBorderThicknessDip,
            bounds.right());
  EXPECT_NEAR(kAnchor3.right_center().y() - bounds.height() / 2, bounds.y(),
              shadow_offset);

  // RIGHT_BOTTOM:

  border.set_arrow(BubbleBorder::RIGHT_BOTTOM);
  bounds = border.GetBounds(kAnchor1, kContentSize);
  EXPECT_EQ(kContentSize.width() + kInsets.right() +
                BubbleBorder::kVisibleArrowLength,
            bounds.width());
  EXPECT_EQ(kContentSize.height() + kInsets.height(), bounds.height());
  EXPECT_EQ(kAnchor1.x() - BubbleBorder::kVisibleArrowGap -
                BubbleBorder::kBorderThicknessDip,
            bounds.right());
  EXPECT_EQ(
      kAnchor1.bottom() + kInsets.bottom() - BubbleBorder::kBorderThicknessDip,
      bounds.bottom());

  bounds = border.GetBounds(kAnchor2, kContentSize);
  EXPECT_EQ(kContentSize.width() + kInsets.right() +
                BubbleBorder::kVisibleArrowLength,
            bounds.width());
  EXPECT_EQ(kContentSize.height() + kInsets.height(), bounds.height());
  EXPECT_EQ(kAnchor2.x() - BubbleBorder::kVisibleArrowGap -
                BubbleBorder::kBorderThicknessDip,
            bounds.right());
  EXPECT_EQ(
      kAnchor2.bottom() + kInsets.bottom() - BubbleBorder::kBorderThicknessDip,
      bounds.bottom());

  // Too small of an anchor view will shift the bubble to make sure the arrow
  // is not too close to the edge of the bubble.
  bounds = border.GetBounds(kAnchor3, kContentSize);
  EXPECT_EQ(kContentSize.width() + kInsets.right() +
                BubbleBorder::kVisibleArrowLength,
            bounds.width());
  EXPECT_EQ(kContentSize.height() + kInsets.height(), bounds.height());
  EXPECT_EQ(kAnchor3.x() - BubbleBorder::kVisibleArrowGap -
                BubbleBorder::kBorderThicknessDip,
            bounds.right());
  EXPECT_LT(
      kAnchor3.bottom() + kInsets.bottom() - BubbleBorder::kBorderThicknessDip,
      bounds.bottom());
}

TEST_F(BubbleBorderTest, AddArrowToBubbleCornerAndPointTowardsAnchor) {
  // Create bubble bounds located at pixel x=400,y=600 with a dimension of
  // 300x200 pixels.
  const gfx::Rect bubble_bounds(400, 600, 300, 200);
  // The element will have a fixed size as well.
  const gfx::Size element_size(350, 100);

  int most_left_x_position =
      bubble_bounds.x() + BubbleBorder::kVisibleArrowBuffer;
  int most_right_x_position = bubble_bounds.right() -
                              BubbleBorder::kVisibleArrowBuffer -
                              BubbleBorder::kVisibleArrowRadius * 2;
  // The y position of an arrow at the upper edge of the bubble.
  int upper_arrow_y_position = bubble_bounds.y();
  // The y position of an arrow at the lower edge of the bubble.
  int lower_arrow_y_position =
      bubble_bounds.bottom() - BubbleBorder::kVisibleArrowLength;

  struct TestCase {
    gfx::Point element_origin;
    BubbleBorder::Arrow supplied_arrow;
    gfx::Point expected_arrow_position;
    bool expected_arrow_visibility_and_return_value;
    gfx::Rect expected_bubble_bounds;
    int popup_min_y = 0;
  } test_cases[]{
      // First are using the following scenario:
      //
      //  y=200        -----------------
      //               |       x       |   element
      //               -----------------
      //
      //  y=600         -----------
      //                |         |
      //                |         |   bubble
      //                |         |
      //  y=800         -----------
      //
      //               | x=380
      //                | x=400
      {{380, 200},
       BubbleBorder::Arrow::TOP_LEFT,
       // The arrow sits close to the right edge of the bubble.
       // The bubble is located above the upper edge. Note that
       // insets need to be taken into account.
       {most_left_x_position, upper_arrow_y_position},
       true,
       bubble_bounds + gfx::Vector2d(0, BubbleBorder::kVisibleArrowLength)},
      {{380, 200},
       BubbleBorder::Arrow::TOP_CENTER,
       // The arrow points to the horizontal center of the element.
       // Note that the spatial extension of the arrow has to be
       // taken into account. The bubble is located above the upper
       // edge. Note that insets need to be taken into account.
       {380 + element_size.width() / 2 - BubbleBorder::kVisibleArrowRadius,
        upper_arrow_y_position},
       true,
       bubble_bounds + gfx::Vector2d(0, BubbleBorder::kVisibleArrowLength)},
      {{380, 200},
       BubbleBorder::Arrow::TOP_RIGHT,
       // The arrow points to the horizontal center of the element.
       // Note that the spatial extension of the arrow has to be
       // taken into account. The bubble is located above the upper
       // edge. Note that insets need to be taken into account.
       {most_right_x_position, upper_arrow_y_position},
       true,
       bubble_bounds + gfx::Vector2d(0, BubbleBorder::kVisibleArrowLength)},
      // The following tests are using a bubble that is highly displaced from
      // the
      // element:
      //
      //  y=200                     -----------------
      //                            |       x       |   element
      //                            -----------------
      //
      //  y=600         -----------
      //                |         |
      //                |         |   bubble
      //                |         |
      //  y=800         -----------
      //
      //                            | x=750
      //                | x=400
      // The arrow should always be located on the most right position.
      {{750, 200},
       BubbleBorder::Arrow::TOP_LEFT,
       {most_right_x_position, upper_arrow_y_position},
       true,
       bubble_bounds + gfx::Vector2d(0, BubbleBorder::kVisibleArrowLength)},
      {{750, 200},
       BubbleBorder::Arrow::TOP_CENTER,
       {most_right_x_position, upper_arrow_y_position},
       true,
       bubble_bounds + gfx::Vector2d(0, BubbleBorder::kVisibleArrowLength)},
      {{750, 200},
       BubbleBorder::Arrow::TOP_RIGHT,
       {most_right_x_position, upper_arrow_y_position},
       true,
       bubble_bounds + gfx::Vector2d(0, BubbleBorder::kVisibleArrowLength)},
      // And the reverse scenario:
      //
      //  y=200    -----------------
      //           |       x       |   element
      //           -----------------
      //
      //  y=600                             -----------
      //                                    |         |
      //                                    |         |    bubble
      //                                    |         |
      //  y=800                             -----------
      //
      //           | x=0
      //                                    | x=400
      // The arrow should always be located on the most right position.
      {{0, 200},
       BubbleBorder::Arrow::TOP_LEFT,
       {most_left_x_position, upper_arrow_y_position},
       true,
       bubble_bounds + gfx::Vector2d(0, BubbleBorder::kVisibleArrowLength)},
      {{0, 200},
       BubbleBorder::Arrow::TOP_CENTER,
       {most_left_x_position, upper_arrow_y_position},
       true,
       bubble_bounds + gfx::Vector2d(0, BubbleBorder::kVisibleArrowLength)},
      {{0, 200},
       BubbleBorder::Arrow::TOP_RIGHT,
       {most_left_x_position, upper_arrow_y_position},
       true,
       bubble_bounds + gfx::Vector2d(0, BubbleBorder::kVisibleArrowLength)},
      // The following tests use a BOTTOM arrow. This should only replace the
      // upper_arrow_y_position with the lower_arrow_y_position in all tests.
      {{380, 200},
       BubbleBorder::Arrow::BOTTOM_LEFT,
       {most_left_x_position, lower_arrow_y_position},
       true,
       bubble_bounds - gfx::Vector2d(0, BubbleBorder::kVisibleArrowLength)},
      {{380, 200},
       BubbleBorder::Arrow::BOTTOM_CENTER,
       {380 + element_size.width() / 2 - BubbleBorder::kVisibleArrowRadius,
        lower_arrow_y_position},
       true,
       bubble_bounds - gfx::Vector2d(0, BubbleBorder::kVisibleArrowLength)},
      {{380, 200},
       BubbleBorder::Arrow::BOTTOM_RIGHT,
       {most_right_x_position, lower_arrow_y_position},
       true,
       bubble_bounds - gfx::Vector2d(0, BubbleBorder::kVisibleArrowLength)},
      {{750, 200},
       BubbleBorder::Arrow::BOTTOM_LEFT,
       {most_right_x_position, lower_arrow_y_position},
       true,
       bubble_bounds - gfx::Vector2d(0, BubbleBorder::kVisibleArrowLength)},
      {{750, 200},
       BubbleBorder::Arrow::BOTTOM_CENTER,
       {most_right_x_position, lower_arrow_y_position},
       true,
       bubble_bounds - gfx::Vector2d(0, BubbleBorder::kVisibleArrowLength)},
      {{750, 200},
       BubbleBorder::Arrow::BOTTOM_RIGHT,
       {most_right_x_position, lower_arrow_y_position},
       true,
       bubble_bounds - gfx::Vector2d(0, BubbleBorder::kVisibleArrowLength)},
      {{0, 200},
       BubbleBorder::Arrow::BOTTOM_LEFT,
       {most_left_x_position, lower_arrow_y_position},
       true,
       bubble_bounds - gfx::Vector2d(0, BubbleBorder::kVisibleArrowLength)},
      {{0, 200},
       BubbleBorder::Arrow::BOTTOM_CENTER,
       {most_left_x_position, lower_arrow_y_position},
       true,
       bubble_bounds - gfx::Vector2d(0, BubbleBorder::kVisibleArrowLength)},
      {{0, 200},
       BubbleBorder::Arrow::BOTTOM_RIGHT,
       {most_left_x_position, lower_arrow_y_position},
       true,
       bubble_bounds - gfx::Vector2d(0, BubbleBorder::kVisibleArrowLength)},
      // Now, the horizontal arrow scenario is tested
      //  y=600                            -----------
      //  y=650    -----------------       |         |
      //           |       x       |       |         |
      //  y=750    -----------------       |         |
      //  y=800                            -----------
      //           | x=0
      //                                   | x=400
      // The arrow is always located on the right side to point towards the
      // vertical center of the element.
      {{0, 650},
       BubbleBorder::Arrow::LEFT_TOP,
       {bubble_bounds.x(),
        650 + element_size.height() / 2 - BubbleBorder::kVisibleArrowRadius},
       true,
       bubble_bounds + gfx::Vector2d(BubbleBorder::kVisibleArrowLength, 0)},
      {{0, 650},
       BubbleBorder::Arrow::LEFT_CENTER,
       {bubble_bounds.x(),
        650 + element_size.height() / 2 - BubbleBorder::kVisibleArrowRadius},
       true,
       bubble_bounds + gfx::Vector2d(BubbleBorder::kVisibleArrowLength, 0)},
      {{0, 650},
       BubbleBorder::Arrow::LEFT_BOTTOM,
       {bubble_bounds.x(),
        650 + element_size.height() / 2 - BubbleBorder::kVisibleArrowRadius},
       true,
       bubble_bounds + gfx::Vector2d(BubbleBorder::kVisibleArrowLength, 0)},
      // With the element moved to the top of the screen, the arrow should
      // always be placed at the most top position on the bubble, the bubble
      // position is adjusted as well
      {{0, 0},
       BubbleBorder::Arrow::LEFT_TOP,
       {bubble_bounds.x(),
        element_size.height() / 2 - BubbleBorder::kVisibleArrowRadius},
       true,
       {bubble_bounds.x() + BubbleBorder::kVisibleArrowLength,
        element_size.height() / 2 - BubbleBorder::kVisibleArrowRadius -
            BubbleBorder::kVisibleArrowBuffer,
        bubble_bounds.width(), bubble_bounds.height()}},
      {{0, 0},
       BubbleBorder::Arrow::LEFT_CENTER,
       {bubble_bounds.x(),
        element_size.height() / 2 - BubbleBorder::kVisibleArrowRadius},
       true,
       {bubble_bounds.x() + BubbleBorder::kVisibleArrowLength,
        element_size.height() / 2 - BubbleBorder::kVisibleArrowRadius -
            BubbleBorder::kVisibleArrowBuffer,
        bubble_bounds.width(), bubble_bounds.height()}},
      {{0, 0},
       BubbleBorder::Arrow::LEFT_BOTTOM,
       {bubble_bounds.x(),
        element_size.height() / 2 - BubbleBorder::kVisibleArrowRadius},
       true,
       {bubble_bounds.x() + BubbleBorder::kVisibleArrowLength,
        element_size.height() / 2 - BubbleBorder::kVisibleArrowRadius -
            BubbleBorder::kVisibleArrowBuffer,
        bubble_bounds.width(), bubble_bounds.height()}},
      {{0, 0},
       BubbleBorder::Arrow::LEFT_TOP,
       {bubble_bounds.x(),
        element_size.height() / 2 - 10 + BubbleBorder::kVisibleArrowBuffer},
       true,
       {bubble_bounds.x() + BubbleBorder::kVisibleArrowLength,
        element_size.height() / 2 - 10, bubble_bounds.width(),
        bubble_bounds.height()},
       element_size.height() / 2 - 10},
  };

  for (auto test_case : test_cases) {
    gfx::Rect bubble_bounds_copy = bubble_bounds;
    views::BubbleBorder border(BubbleBorder::Arrow::NONE,
                               BubbleBorder::STANDARD_SHADOW);
    border.set_arrow(test_case.supplied_arrow);
    EXPECT_EQ(border.AddArrowToBubbleCornerAndPointTowardsAnchor(
                  {test_case.element_origin, element_size}, bubble_bounds_copy,
                  test_case.popup_min_y),
              test_case.expected_arrow_visibility_and_return_value);
    EXPECT_EQ(border.visible_arrow(),
              test_case.expected_arrow_visibility_and_return_value);
    EXPECT_EQ(border.GetVisibibleArrowRectForTesting().origin(),
              test_case.expected_arrow_position);
    EXPECT_EQ(GetVisibleArrowSize(test_case.supplied_arrow),
              border.GetVisibibleArrowRectForTesting().size());
    EXPECT_EQ(test_case.expected_bubble_bounds, bubble_bounds_copy);
  }
}

TEST_F(BubbleBorderTest,
       AddArrowToBubbleCornerAndPointTowardsAnchorWithInsufficientSpace) {
  // This bubble bound has uinsufficient width to place an arrow on the top or
  // the bottom.
  const gfx::Rect insufficient_width_bubble_bounds(0, 0, 10, 200);

  // This bound has insufficient height to place an arrow on the left or right.
  const gfx::Rect insufficient_height_bubble_bounds(0, 0, 100, 10);

  // Create bounds for the element, the specifics do no matter.
  const gfx::Rect element_bounds(0, 0, 350, 100);

  struct TestCase {
    gfx::Rect bubble_bounds;
    BubbleBorder::Arrow supplied_arrow;
    bool expected_arrow_visibility_and_return_value;
  } test_cases[]{
      // Bubble is placeable on top because there is sufficient width.
      {insufficient_height_bubble_bounds, BubbleBorder::Arrow::TOP_CENTER,
       true},
      // Bubble is not placeable on top because the width is insufficient.
      {insufficient_width_bubble_bounds, BubbleBorder::Arrow::TOP_CENTER,
       false},
      // Bubble is not placeable on the side because the height is insufficient.
      {insufficient_height_bubble_bounds, BubbleBorder::Arrow::LEFT_CENTER,
       false},
      // Bubble is placeable on the side because the height is sufficient.
      {insufficient_width_bubble_bounds, BubbleBorder::Arrow::LEFT_CENTER,
       true},
  };

  for (auto test_case : test_cases) {
    views::BubbleBorder border(BubbleBorder::Arrow::NONE,
                               BubbleBorder::STANDARD_SHADOW);
    border.set_arrow(test_case.supplied_arrow);
    EXPECT_EQ(border.AddArrowToBubbleCornerAndPointTowardsAnchor(
                  element_bounds, test_case.bubble_bounds, 0),
              test_case.expected_arrow_visibility_and_return_value);
  }
}

TEST_F(BubbleBorderTest, IsVerticalArrow) {
  struct TestCase {
    BubbleBorder::Arrow arrow;
    bool is_vertical_expected;
  };

  TestCase test_cases[] = {
      // BOTTOM and TOP arrows are vertical.
      {BubbleBorder::Arrow::BOTTOM_CENTER, true},
      {BubbleBorder::Arrow::BOTTOM_LEFT, true},
      {BubbleBorder::Arrow::BOTTOM_RIGHT, true},
      {BubbleBorder::Arrow::TOP_CENTER, true},
      {BubbleBorder::Arrow::TOP_LEFT, true},
      {BubbleBorder::Arrow::TOP_RIGHT, true},
      // The rest is horizontal.
      {BubbleBorder::Arrow::LEFT_BOTTOM, false},
      {BubbleBorder::Arrow::LEFT_CENTER, false},
      {BubbleBorder::Arrow::LEFT_TOP, false},
      {BubbleBorder::Arrow::RIGHT_BOTTOM, false},
      {BubbleBorder::Arrow::RIGHT_CENTER, false},
      {BubbleBorder::Arrow::RIGHT_TOP, false},
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(IsVerticalArrow(test_case.arrow), test_case.is_vertical_expected);
  }
}

// Test that the correct arrow size is returned for a given arrow position.
TEST_F(BubbleBorderTest, GetVisibleArrowSize) {
  const gfx::Size vertical_size(2 * BubbleBorder::kVisibleArrowRadius,
                                BubbleBorder::kVisibleArrowLength);
  const gfx::Size horizontal_size(BubbleBorder::kVisibleArrowLength,
                                  2 * BubbleBorder::kVisibleArrowRadius);

  struct TestCase {
    BubbleBorder::Arrow arrow;
    gfx::Size expected_size;
  };

  TestCase test_cases[] = {
      // BOTTOM and TOP arrows have a vertical size.
      {BubbleBorder::Arrow::BOTTOM_CENTER, vertical_size},
      {BubbleBorder::Arrow::BOTTOM_LEFT, vertical_size},
      {BubbleBorder::Arrow::BOTTOM_RIGHT, vertical_size},
      {BubbleBorder::Arrow::TOP_CENTER, vertical_size},
      {BubbleBorder::Arrow::TOP_LEFT, vertical_size},
      {BubbleBorder::Arrow::TOP_RIGHT, vertical_size},
      // The rest has a horizontal size.
      {BubbleBorder::Arrow::LEFT_BOTTOM, horizontal_size},
      {BubbleBorder::Arrow::LEFT_CENTER, horizontal_size},
      {BubbleBorder::Arrow::LEFT_TOP, horizontal_size},
      {BubbleBorder::Arrow::RIGHT_BOTTOM, horizontal_size},
      {BubbleBorder::Arrow::RIGHT_CENTER, horizontal_size},
      {BubbleBorder::Arrow::RIGHT_TOP, horizontal_size},
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(GetVisibleArrowSize(test_case.arrow), test_case.expected_size);
  }
}

// Test that the contents bounds are moved correctly to place the visible arrow
// at the appropriate position.
TEST_F(BubbleBorderTest, MoveContentsBoundsToPlaceVisibleArrow) {
  const int arrow_length =
      BubbleBorder::kVisibleArrowLength + BubbleBorder::kVisibleArrowGap;

  struct TestCase {
    BubbleBorder::Arrow arrow;
    gfx::Vector2d expected_contents_bounds_move;
    gfx::Point initial_bubble_origin = gfx::Point(0, 0);
  };

  TestCase test_cases[] = {
      // BOTTOM cases: The contents is moved to the top of the screen.
      {BubbleBorder::Arrow::BOTTOM_LEFT, gfx::Vector2d(0, -arrow_length)},
      {BubbleBorder::Arrow::BOTTOM_CENTER, gfx::Vector2d(0, -arrow_length)},
      {BubbleBorder::Arrow::BOTTOM_RIGHT, gfx::Vector2d(0, -arrow_length)},
      // TOP cases: The contents is moved to the bottom of the screen.
      {BubbleBorder::Arrow::TOP_LEFT, gfx::Vector2d(0, arrow_length)},
      {BubbleBorder::Arrow::TOP_CENTER, gfx::Vector2d(0, arrow_length)},
      {BubbleBorder::Arrow::TOP_RIGHT, gfx::Vector2d(0, arrow_length)},
      // LEFT cases: The contents is moved to the right.
      {BubbleBorder::Arrow::LEFT_BOTTOM, gfx::Vector2d(arrow_length, 0)},
      {BubbleBorder::Arrow::LEFT_CENTER, gfx::Vector2d(arrow_length, 0)},
      {BubbleBorder::Arrow::LEFT_TOP, gfx::Vector2d(arrow_length, 0)},
      // RIGHT cases: The contents is moved to the left.
      {BubbleBorder::Arrow::RIGHT_BOTTOM, gfx::Vector2d(-arrow_length, 0)},
      {BubbleBorder::Arrow::RIGHT_CENTER, gfx::Vector2d(-arrow_length, 0)},
      {BubbleBorder::Arrow::RIGHT_TOP, gfx::Vector2d(-arrow_length, 0)},
  };

  for (const auto& test_case : test_cases) {
    // Create a bubble border with a visible arrow.
    views::BubbleBorder border(test_case.arrow, BubbleBorder::STANDARD_SHADOW);
    border.set_visible_arrow(true);

    // Create, move and verify the contents bounds.
    EXPECT_EQ(border.GetContentsBoundsOffsetToPlaceVisibleArrow(
                  test_case.arrow, /*include_gap=*/true),
              test_case.expected_contents_bounds_move);
  }
}

}  // namespace views
