// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/footnote_container_view.h"

#include "cc/paint/paint_flags.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"

namespace views {

namespace {

// A solid color background where the bottom two corners are rounded.
class HalfRoundedRectBackground : public Background {
 public:
  explicit HalfRoundedRectBackground(SkColor color, float radius)
      : radius_(radius) {
    SetNativeControlColor(color);
  }
  ~HalfRoundedRectBackground() override = default;

  // Background:
  void Paint(gfx::Canvas* canvas, View* view) const override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(get_color());
    // Draw a rounded rect that spills outside of the clipping area, so that the
    // rounded corners only show in the bottom 2 corners.
    gfx::RectF spilling_rect(view->GetLocalBounds());
    spilling_rect.set_y(spilling_rect.x() - radius_);
    spilling_rect.set_height(spilling_rect.height() + radius_);
    canvas->DrawRoundRect(spilling_rect, radius_, flags);
  }

 private:
  float radius_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(HalfRoundedRectBackground);
};

}  // namespace

FootnoteContainerView::FootnoteContainerView(const gfx::Insets& margins,
                                             View* child_view,
                                             float corner_radius) {
  SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::kVertical, margins, 0));
  SetCornerRadius(corner_radius);
  SetBorder(CreateSolidSidedBorder(1, 0, 0, 0, gfx::kGoogleGrey200));
  AddChildView(child_view);
  SetVisible(child_view->visible());
}

FootnoteContainerView::~FootnoteContainerView() = default;

void FootnoteContainerView::SetCornerRadius(float corner_radius) {
  // TODO(crbug.com/893598): Finalize dark mode color.
  SkColor background_color = GetNativeTheme()->SystemDarkModeEnabled()
                                 ? gfx::kGoogleGrey800
                                 : gfx::kGoogleGrey050;
  SetBackground(std::make_unique<HalfRoundedRectBackground>(background_color,
                                                            corner_radius));
}

void FootnoteContainerView::ChildVisibilityChanged(View* child) {
  DCHECK_EQ(child_count(), 1);
  SetVisible(child->visible());
}

}  // namespace views
