// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/footnote_container_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"

namespace views {

FootnoteContainerView::FootnoteContainerView(const gfx::Insets& margins,
                                             std::unique_ptr<View> child_view,
                                             float lower_left_radius,
                                             float lower_right_radius) {
  SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, margins, 0));
  auto* child_view_ptr = AddChildView(std::move(child_view));
  SetVisible(child_view_ptr->GetVisible());
  SetBorder(CreateSolidSidedBorder(gfx::Insets::TLBR(1, 0, 0, 0),
                                   ui::kColorBubbleFooterBorder));
  SetRoundedBackground(lower_left_radius, lower_right_radius);
}

FootnoteContainerView::~FootnoteContainerView() = default;

void FootnoteContainerView::SetRoundedCorners(float lower_left_radius,
                                              float lower_right_radius) {
  SetRoundedBackground(lower_left_radius, lower_right_radius);
}

void FootnoteContainerView::ChildVisibilityChanged(View* child) {
  DCHECK_EQ(1u, children().size());
  SetVisible(child->GetVisible());
}

void FootnoteContainerView::SetRoundedBackground(float lower_left_radius,
                                                 float lower_right_radius) {
  SetBackground(CreateRoundedRectBackground(
      ui::kColorBubbleFooterBackground,
      gfx::RoundedCornersF(0, 0, lower_left_radius, lower_right_radius)));
}

BEGIN_METADATA(FootnoteContainerView)
END_METADATA

}  // namespace views
