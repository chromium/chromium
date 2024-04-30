// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
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
  SetFlipCanvasOnPaintForRTLUI(true);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  // Animate all changes to the layer except the first one.
  OnStateChanged();
  layer()->SetAnimator(ui::LayerAnimator::CreateImplicitAnimator());
}

gfx::Size OverlayScrollBar::Thumb::CalculatePreferredSize(
    const SizeBounds& /*available_size*/) const {
  // The visual size of the thumb is kThumbThickness, but it slides back and
  // forth by kThumbHoverOffset. To make event targetting work well, expand the
  // width of the thumb such that it's always taking up the full width of the
  // track regardless of the offset.
  return gfx::Size(kThumbThickness + kThumbHoverOffset,
                   kThumbThickness + kThumbHoverOffset);
}

void OverlayScrollBar::Thumb::OnPaint(gfx::Canvas* canvas) {
  const bool hovered = GetState() != Button::STATE_NORMAL;
  cc::PaintFlags fill_flags;
  fill_flags.setStyle(cc::PaintFlags::kFill_Style);
  fill_flags.setColor(GetColorProvider()->GetColor(
      hovered ? ui::kColorOverlayScrollbarFillHovered
              : ui::kColorOverlayScrollbarFill));
  gfx::RectF fill_bounds(GetLocalBounds());
  fill_bounds.Inset(gfx::InsetsF::TLBR(IsHorizontal() ? kThumbHoverOffset : 0,
                                       IsHorizontal() ? 0 : kThumbHoverOffset,
                                       0, 0));
  fill_bounds.Inset(gfx::InsetsF::TLBR(kThumbStroke, kThumbStroke,
                                       IsHorizontal() ? 0 : kThumbStroke,
                                       IsHorizontal() ? kThumbStroke : 0));
  canvas->DrawRect(fill_bounds, fill_flags);

  cc::PaintFlags stroke_flags;
  stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
  stroke_flags.setColor(GetColorProvider()->GetColor(
      hovered ? ui::kColorOverlayScrollbarStrokeHovered
              : ui::kColorOverlayScrollbarStroke));
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

    if (GetWidget())
      scroll_bar_->StartHideCountdown();
  } else {
    layer()->SetTransform(gfx::Transform());
  }
  SchedulePaint();
}

BEGIN_METADATA(OverlayScrollBar, Thumb)
END_METADATA

OverlayScrollBar::OverlayScrollBar(Orientation orientation)
    : ScrollBar(orientation) {
  SetNotifyEnterExitOnChild(true);
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
  return GetOrientation() == Orientation::kHorizontal
             ? gfx::Insets::TLBR(-kThumbHoverOffset, 0, 0, 0)
             : gfx::Insets::TLBR(0, -kThumbHoverOffset, 0, 0);
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
END_METADATA

}  // namespace views
