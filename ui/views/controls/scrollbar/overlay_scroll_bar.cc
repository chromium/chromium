// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"

#include "base/bind.h"
#include "base/macros.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/overlay_scrollbar_constants_aura.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/fill_layout.h"

namespace views {
namespace {

// Total thickness of the thumb (matches visuals when hovered).
constexpr int kThumbThickness =
    ui::kOverlayScrollbarThumbWidthPressed + ui::kOverlayScrollbarStrokeWidth;
// When hovered, the thumb takes up the full width. Otherwise, it's a bit
// slimmer.
constexpr int kThumbHoverOffset = 4;
// The layout size of the thumb stroke, in DIP.
constexpr int kThumbStroke = ui::kOverlayScrollbarStrokeWidth;
// The visual size of the thumb stroke, in px.
constexpr int kThumbStrokeVisualSize = ui::kOverlayScrollbarStrokeWidth;

}  // namespace

OverlayScrollBar::Thumb::Thumb(OverlayScrollBar* scroll_bar)
    : BaseScrollBarThumb(scroll_bar), scroll_bar_(scroll_bar) {
  // |scroll_bar| isn't done being constructed; it's not safe to do anything
  // that might reference it yet.
}

OverlayScrollBar::Thumb::~Thumb() = default;

void OverlayScrollBar::Thumb::Init() {
  EnableCanvasFlippingForRTLUI(true);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  // Animate all changes to the layer except the first one.
  OnStateChanged();
  layer()->SetAnimator(ui::LayerAnimator::CreateImplicitAnimator());
}

gfx::Size OverlayScrollBar::Thumb::CalculatePreferredSize() const {
  // The visual size of the thumb is kThumbThickness, but it slides back and
  // forth by kThumbHoverOffset. To make event targetting work well, expand the
  // width of the thumb such that it's always taking up the full width of the
  // track regardless of the offset.
  return gfx::Size(kThumbThickness + kThumbHoverOffset,
                   kThumbThickness + kThumbHoverOffset);
}

void OverlayScrollBar::Thumb::OnPaint(gfx::Canvas* canvas) {
  cc::PaintFlags fill_flags;
  fill_flags.setStyle(cc::PaintFlags::kFill_Style);
  fill_flags.setColor(SK_ColorBLACK);
  gfx::RectF fill_bounds(GetLocalBounds());
  fill_bounds.Inset(gfx::InsetsF(IsHorizontal() ? kThumbHoverOffset : 0,
                                 IsHorizontal() ? 0 : kThumbHoverOffset, 0, 0));
  fill_bounds.Inset(gfx::InsetsF(kThumbStroke, kThumbStroke,
                                 IsHorizontal() ? 0 : kThumbStroke,
                                 IsHorizontal() ? kThumbStroke : 0));
  canvas->DrawRect(fill_bounds, fill_flags);

  cc::PaintFlags stroke_flags;
  stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
  stroke_flags.setColor(
      SkColorSetA(SK_ColorWHITE, (ui::kOverlayScrollbarStrokeNormalAlpha /
                                  ui::kOverlayScrollbarThumbNormalAlpha) *
                                     SK_AlphaOPAQUE));
  stroke_flags.setStrokeWidth(kThumbStrokeVisualSize);
  stroke_flags.setStrokeCap(cc::PaintFlags::kSquare_Cap);

  // The stroke is a single pixel, so we must deal with the unscaled canvas.
  const float dsf = canvas->UndoDeviceScaleFactor();
  gfx::RectF stroke_bounds(fill_bounds);
  stroke_bounds.Scale(dsf);
  // The stroke should be aligned to the pixel center that is nearest the fill,
  // so outset by a half pixel.
  stroke_bounds.Inset(gfx::InsetsF(-kThumbStrokeVisualSize / 2.0f));
  // The stroke doesn't apply to the far edge of the thumb.
  SkPath path;
  path.moveTo(gfx::PointFToSkPoint(stroke_bounds.top_right()));
  path.lineTo(gfx::PointFToSkPoint(stroke_bounds.origin()));
  path.lineTo(gfx::PointFToSkPoint(stroke_bounds.bottom_left()));
  if (IsHorizontal()) {
    path.moveTo(gfx::PointFToSkPoint(stroke_bounds.bottom_right()));
    path.close();
  } else {
    path.lineTo(gfx::PointFToSkPoint(stroke_bounds.bottom_right()));
  }
  canvas->DrawPath(path, stroke_flags);
}

void OverlayScrollBar::Thumb::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  scroll_bar_->Show();
  // Don't start the hide countdown if the thumb is still hovered or pressed.
  if (GetState() == Button::STATE_NORMAL)
    scroll_bar_->StartHideCountdown();
}

void OverlayScrollBar::Thumb::OnStateChanged() {
  if (GetState() == Button::STATE_NORMAL) {
    gfx::Transform translation;
    const int direction = base::i18n::IsRTL() ? -1 : 1;
    translation.Translate(
        gfx::Vector2d(IsHorizontal() ? 0 : direction * kThumbHoverOffset,
                      IsHorizontal() ? kThumbHoverOffset : 0));
    layer()->SetTransform(translation);
    layer()->SetOpacity(ui::kOverlayScrollbarThumbNormalAlpha);

    if (GetWidget())
      scroll_bar_->StartHideCountdown();
  } else {
    layer()->SetTransform(gfx::Transform());
    layer()->SetOpacity(ui::kOverlayScrollbarThumbHoverAlpha);
  }
}

OverlayScrollBar::OverlayScrollBar(bool horizontal) : ScrollBar(horizontal) {
  set_notify_enter_exit_on_child(true);
  SetPaintToLayer();
  layer()->SetMasksToBounds(true);
  layer()->SetFillsBoundsOpaquely(false);

  // Allow the thumb to take up the whole size of the scrollbar.  Layout need
  // only set the thumb cross-axis coordinate; ScrollBar::Update() will set the
  // thumb size/offset.
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto* thumb = new Thumb(this);
  SetThumb(thumb);
  thumb->Init();
}

OverlayScrollBar::~OverlayScrollBar() = default;

gfx::Insets OverlayScrollBar::GetInsets() const {
  return IsHorizontal() ? gfx::Insets(-kThumbHoverOffset, 0, 0, 0)
                        : gfx::Insets(0, -kThumbHoverOffset, 0, 0);
}

void OverlayScrollBar::OnMouseEntered(const ui::MouseEvent& event) {
  Show();
}

void OverlayScrollBar::OnMouseExited(const ui::MouseEvent& event) {
  StartHideCountdown();
}

bool OverlayScrollBar::OverlapsContent() const {
  return true;
}

gfx::Rect OverlayScrollBar::GetTrackBounds() const {
  return GetContentsBounds();
}

int OverlayScrollBar::GetThickness() const {
  return kThumbThickness;
}

void OverlayScrollBar::Show() {
  layer()->SetOpacity(1.0f);
  hide_timer_.Stop();
}

void OverlayScrollBar::Hide() {
  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.SetTransitionDuration(ui::kOverlayScrollbarFadeDuration);
  layer()->SetOpacity(0.0f);
}

void OverlayScrollBar::StartHideCountdown() {
  if (IsMouseHovered())
    return;
  hide_timer_.Start(
      FROM_HERE, ui::kOverlayScrollbarFadeDelay,
      base::BindOnce(&OverlayScrollBar::Hide, base::Unretained(this)));
}

BEGIN_METADATA(OverlayScrollBar)
METADATA_PARENT_CLASS(ScrollBar)
END_METADATA()

}  // namespace views
