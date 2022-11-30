// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_BORDER_ARROW_UTILS_H_
#define UI_VIEWS_BUBBLE_BUBBLE_BORDER_ARROW_UTILS_H_

#include "ui/views/bubble/bubble_border.h"
#include "ui/views/views_export.h"

namespace views {

// The side of the bubble the arrow is located on.
enum class BubbleArrowSide { kLeft, kRight, kTop, kBottom };

// Converts the |arrow| into a BubbleArrowSide.
VIEWS_EXPORT BubbleArrowSide GetBubbleArrowSide(BubbleBorder::Arrow arrow);

// Returns the appropriate anchor point on the edge of the |anchor_rect| for a
// given |arrow| position.
VIEWS_EXPORT gfx::Point GetArrowAnchorPointFromAnchorRect(
    BubbleBorder::Arrow arrow,
    const gfx::Rect& anchor_rect);

// Returns the origin offset to move the |contents_bounds| to be placed
// appropriately for a given |arrow| at the |anchor_point|.
VIEWS_EXPORT gfx::Vector2d GetContentBoundsOffsetToArrowAnchorPoint(
    const gfx::Rect& contents_bounds,
    BubbleBorder::Arrow arrow,
    const gfx::Point& anchor_point);

// Converts the |arrow| into a BubbleArrowSide.
VIEWS_EXPORT BubbleArrowSide GetBubbleArrowSide(BubbleBorder::Arrow arrow);

// Returns the translation vector for a bubble to make space for
// inserting the visible arrow at the right position for |arrow_|.
// |include_gap| controls if the displacement accounts for the
// kVisibleArrowGap.
VIEWS_EXPORT gfx::Vector2d GetContentsBoundsOffsetToPlaceVisibleArrow(
    BubbleBorder::Arrow arrow,
    bool include_gap = true);

VIEWS_EXPORT gfx::Insets GetVisibleArrowInsets(BubbleBorder::Arrow arrow,
                                               bool include_gap);

// Returns true if the arrow is vertical meaning that it is either placed on
// the top of the bottom of the border.
VIEWS_EXPORT bool IsVerticalArrow(BubbleBorder::Arrow arrow);

// Returns the size of the bounding rectangle of the visible |arrow|.
VIEWS_EXPORT gfx::Size GetVisibleArrowSize(BubbleBorder::Arrow arrow);

}  // namespace views
#endif  // UI_VIEWS_BUBBLE_BUBBLE_BORDER_ARROW_UTILS_H_
