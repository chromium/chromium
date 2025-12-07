// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_border_arrow_utils.h"

namespace views {

gfx::Point GetArrowAnchorPointFromAnchorRect(BubbleBorder::Arrow arrow,
                                             const gfx::Rect& anchor_rect) {
  switch (arrow) {
    case BubbleBorder::TOP_LEFT:
      return anchor_rect.bottom_left();

    case BubbleBorder::TOP_RIGHT:
      return anchor_rect.bottom_right();

    case BubbleBorder::BOTTOM_LEFT:
      return anchor_rect.origin();

    case BubbleBorder::BOTTOM_RIGHT:
      return anchor_rect.top_right();

    case BubbleBorder::LEFT_TOP:
      return anchor_rect.top_right();

    case BubbleBorder::RIGHT_TOP:
      return anchor_rect.origin();

    case BubbleBorder::LEFT_BOTTOM:
      return anchor_rect.bottom_right();

    case BubbleBorder::RIGHT_BOTTOM:
      return anchor_rect.bottom_left();

    case BubbleBorder::TOP_CENTER:
      return anchor_rect.bottom_center();

    case BubbleBorder::BOTTOM_CENTER:
      return anchor_rect.top_center();

    case BubbleBorder::LEFT_CENTER:
      return anchor_rect.right_center();

    case BubbleBorder::RIGHT_CENTER:
      return anchor_rect.left_center();

    default:
      NOTREACHED();
  }
}

gfx::Vector2d GetContentBoundsOffsetToArrowAnchorPoint(
    const gfx::Rect& contents_bounds,
    BubbleBorder::Arrow arrow,
    const gfx::Point& anchor_point) {
  switch (arrow) {
    case BubbleBorder::TOP_LEFT:
      return anchor_point - contents_bounds.origin();

    case BubbleBorder::TOP_RIGHT:
      return anchor_point - contents_bounds.top_right();

    case BubbleBorder::BOTTOM_LEFT:
      return anchor_point - contents_bounds.bottom_left();

    case BubbleBorder::BOTTOM_RIGHT:
      return anchor_point - contents_bounds.bottom_right();

    case BubbleBorder::LEFT_TOP:
      return anchor_point - contents_bounds.origin();

    case BubbleBorder::RIGHT_TOP:
      return anchor_point - contents_bounds.top_right();

    case BubbleBorder::LEFT_BOTTOM:
      return anchor_point - contents_bounds.bottom_left();

    case BubbleBorder::RIGHT_BOTTOM:
      return anchor_point - contents_bounds.bottom_right();

    case BubbleBorder::TOP_CENTER:
      return anchor_point - contents_bounds.top_center();

    case BubbleBorder::BOTTOM_CENTER:
      return anchor_point - contents_bounds.bottom_center();

    case BubbleBorder::LEFT_CENTER:
      return anchor_point - contents_bounds.left_center();

    case BubbleBorder::RIGHT_CENTER:
      return anchor_point - contents_bounds.right_center();

    default:
      NOTREACHED();
  }
}

BubbleArrowSide GetBubbleArrowSide(BubbleBorder::Arrow arrow) {
  // Note: VERTICAL arrows are on the sides of the bubble, while !VERTICAL are
  // on the top or bottom.
  if (int{arrow} & BubbleBorder::VERTICAL) {
    return (int{arrow} & BubbleBorder::RIGHT) ? BubbleArrowSide::kRight
                                              : BubbleArrowSide::kLeft;
  }
  return (int{arrow} & BubbleBorder::BOTTOM) ? BubbleArrowSide::kBottom
                                             : BubbleArrowSide::kTop;
}

gfx::Vector2d GetContentsBoundsOffsetToPlaceVisibleArrow(
    BubbleBorder::Arrow arrow,
    bool include_gap) {
  if (arrow == BubbleBorder::NONE || arrow == BubbleBorder::FLOAT) {
    return gfx::Vector2d();
  }
  const gfx::Insets visible_arrow_insets =
      GetVisibleArrowInsets(arrow, include_gap);
  return gfx::Vector2d(
      visible_arrow_insets.left() - visible_arrow_insets.right(),
      visible_arrow_insets.top() - visible_arrow_insets.bottom());
}

gfx::Insets GetVisibleArrowInsets(BubbleBorder::Arrow arrow, bool include_gap) {
  DCHECK(BubbleBorder::has_arrow(arrow));
  const int arrow_size = include_gap ? BubbleBorder::kVisibleArrowGap +
                                           BubbleBorder::kVisibleArrowLength
                                     : BubbleBorder::kVisibleArrowLength;
  gfx::Insets result;
  switch (GetBubbleArrowSide(arrow)) {
    case BubbleArrowSide::kRight:
      result.set_right(arrow_size);
      break;
    case BubbleArrowSide::kLeft:
      result.set_left(arrow_size);
      break;
    case BubbleArrowSide::kTop:
      result.set_top(arrow_size);
      break;
    case BubbleArrowSide::kBottom:
      result.set_bottom(arrow_size);
      break;
  }
  return result;
}

bool IsVerticalArrow(BubbleBorder::Arrow arrow) {
  const BubbleArrowSide side = GetBubbleArrowSide(arrow);
  return side == BubbleArrowSide::kTop || side == BubbleArrowSide::kBottom;
}

gfx::Size GetVisibleArrowSize(BubbleBorder::Arrow arrow) {
  int kVisibleArrowDiameter = 2 * BubbleBorder::kVisibleArrowRadius;

  return IsVerticalArrow(arrow) ? gfx::Size(kVisibleArrowDiameter,
                                            BubbleBorder::kVisibleArrowLength)
                                : gfx::Size(BubbleBorder::kVisibleArrowLength,
                                            kVisibleArrowDiameter);
}

}  // namespace views
