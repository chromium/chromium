// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/focus_ring.h"

#include "ui/gfx/canvas.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_properties.h"

namespace {

ui::NativeTheme::ColorId ColorIdForValidity(bool valid) {
  return valid ? ui::NativeTheme::kColorId_FocusedBorderColor
               : ui::NativeTheme::kColorId_AlertSeverityHigh;
}

}  // namespace

namespace views {

const char FocusRing::kViewClassName[] = "FocusRing";

// static
std::unique_ptr<FocusRing> FocusRing::Install(View* parent) {
  auto ring = base::WrapUnique<FocusRing>(new FocusRing(parent));
  ring->set_owned_by_client();
  parent->AddChildView(ring.get());
  parent->AddObserver(ring.get());
  ring->Layout();
  ring->SchedulePaint();
  return ring;
}

// static
bool FocusRing::IsPathUseable(const SkPath& path) {
  return !path.isEmpty() && (path.isRect(nullptr) || path.isOval(nullptr) ||
                             path.isRRect(nullptr));
}

void FocusRing::SetPath(const SkPath& path) {
  path_ = IsPathUseable(path) ? path : SkPath();
  SchedulePaint();
}

void FocusRing::SetInvalid(bool invalid) {
  invalid_ = invalid;
  SchedulePaint();
}

void FocusRing::SetHasFocusPredicate(const ViewPredicate& predicate) {
  has_focus_predicate_ = predicate;
  SchedulePaint();
}

const char* FocusRing::GetClassName() const {
  return kViewClassName;
}

void FocusRing::Layout() {
  // The focus ring handles its own sizing, which is simply to fill the parent
  // and extend a little beyond its borders.
  gfx::Rect focus_bounds = parent()->GetLocalBounds();
  focus_bounds.Inset(gfx::Insets(PlatformStyle::kFocusHaloInset));
  SetBoundsRect(focus_bounds);
}

void FocusRing::OnPaint(gfx::Canvas* canvas) {
  if (!has_focus_predicate_(parent()))
    return;

  SkColor base_color =
      GetNativeTheme()->GetSystemColor(ColorIdForValidity(!invalid_));

  cc::PaintFlags paint;
  paint.setAntiAlias(true);
  paint.setColor(SkColorSetA(base_color, 0x66));
  paint.setStyle(cc::PaintFlags::kStroke_Style);
  paint.setStrokeWidth(PlatformStyle::kFocusHaloThickness);

  SkPath path = path_;
  if (path.isEmpty()) {
    SkPath* highlight_path = parent()->GetProperty(kHighlightPathKey);
    if (highlight_path)
      path = *highlight_path;
  }
  if (path.isEmpty())
    path.addRect(RectToSkRect(parent()->GetLocalBounds()));

  DCHECK(IsPathUseable(path));
  SkRect bounds;
  SkRRect rbounds;
  if (path.isRect(&bounds)) {
    canvas->sk_canvas()->drawRRect(RingRectFromPathRect(bounds), paint);
  } else if (path.isOval(&bounds)) {
    gfx::RectF rect = gfx::SkRectToRectF(bounds);
    View::ConvertRectToTarget(view_, this, &rect);
    canvas->sk_canvas()->drawRRect(SkRRect::MakeOval(gfx::RectFToSkRect(rect)),
                                   paint);
  } else if (path.isRRect(&rbounds)) {
    canvas->sk_canvas()->drawRRect(RingRectFromPathRect(rbounds), paint);
  }
}

void FocusRing::OnViewFocused(View* view) {
  SchedulePaint();
}

void FocusRing::OnViewBlurred(View* view) {
  SchedulePaint();
}

FocusRing::FocusRing(View* parent) : view_(parent) {
  // A layer is necessary to paint beyond the parent's bounds.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  // Don't allow the view to process events.
  set_can_process_events_within_subtree(false);

  has_focus_predicate_ = [](View* p) -> bool { return p->HasFocus(); };
}

FocusRing::~FocusRing() {
  if (parent())
    parent()->RemoveObserver(this);
}

SkRRect FocusRing::RingRectFromPathRect(const SkRect& rect) const {
  double thickness = PlatformStyle::kFocusHaloThickness / 2.f;
  double corner_radius = FocusableBorder::kCornerRadiusDp + thickness;
  return RingRectFromPathRect(
      SkRRect::MakeRectXY(rect, corner_radius, corner_radius));
}

SkRRect FocusRing::RingRectFromPathRect(const SkRRect& rrect) const {
  double thickness = PlatformStyle::kFocusHaloThickness / 2.f;
  gfx::RectF r = gfx::SkRectToRectF(rrect.rect());
  View::ConvertRectToTarget(view_, this, &r);

  SkRRect skr =
      rrect.makeOffset(r.x() - rrect.rect().x(), r.y() - rrect.rect().y());

  // The focus indicator should hug the normal border, when present (as in the
  // case of text buttons). Since it's drawn outside the parent view, increase
  // the rounding slightly by adding half the ring thickness.
  skr.inset(PlatformStyle::kFocusHaloInset, PlatformStyle::kFocusHaloInset);
  skr.inset(thickness, thickness);

  return skr;
}

}  // namespace views
