// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/focus_ring.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(views::FocusRing*)

namespace views {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kDrawFocusRingBackgroundOutline, false)

namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(FocusRing*, kFocusRingIdKey, nullptr)

constexpr int kMinFocusRingInset = 2;
constexpr float kOutlineThickness = 1.0f;
constexpr float kFocusRingOutset = 2.0f;

bool IsPathUsable(const SkPath& path) {
  return !path.isEmpty() && (path.isRect(nullptr) || path.isOval(nullptr) ||
                             path.isRRect(nullptr));
}

SkColor GetPaintColor(FocusRing* focus_ring, bool valid) {
  const auto* cp = focus_ring->GetColorProvider();
  if (!valid)
    return cp->GetColor(ui::kColorAlertHighSeverity);
  if (auto color_id = focus_ring->GetColorId(); color_id.has_value())
    return cp->GetColor(color_id.value());
  return GetCascadingAccentColor(focus_ring);
}

double GetCornerRadius(float halo_thickness) {
  const double thickness = halo_thickness / 2.f;
  return FocusRing::kDefaultCornerRadiusDp + thickness;
}

SkPath GetHighlightPathInternal(const View* view, float halo_thickness) {
  HighlightPathGenerator* path_generator =
      view->GetProperty(kHighlightPathGeneratorKey);

  if (path_generator) {
    SkPath highlight_path = path_generator->GetHighlightPath(view);
    // The generated path might be empty or otherwise unusable. If that's the
    // case we should fall back on the default path.
    if (IsPathUsable(highlight_path))
      return highlight_path;
  }

  gfx::Rect client_rect = view->GetLocalBounds();
  const double corner_radius = GetCornerRadius(halo_thickness);
  // Make sure we don't return an empty focus ring. This covers narrow views and
  // the case where view->GetLocalBounds() are empty. Doing so prevents
  // DCHECK(IsPathUsable(path)) from failing in GetRingRoundRect() because the
  // resulting path is empty.
  if (client_rect.IsEmpty()) {
    client_rect.Outset(kMinFocusRingInset);
  }
  return SkPath().addRRect(SkRRect::MakeRectXY(RectToSkRect(client_rect),
                                               corner_radius, corner_radius));
}

}  // namespace

constexpr float FocusRing::kDefaultHaloThickness;
constexpr float FocusRing::kDefaultHaloInset;

// static
void FocusRing::Install(View* host) {
  FocusRing::Remove(host);
  auto ring = base::WrapUnique<FocusRing>(new FocusRing());
  ring->InvalidateLayout();
  ring->SchedulePaint();
  ring->SetProperty(kViewIgnoredByLayoutKey, true);
  host->SetProperty(kFocusRingIdKey, host->AddChildView(std::move(ring)));
}

FocusRing* FocusRing::Get(View* host) {
  return host->GetProperty(kFocusRingIdKey);
}

const FocusRing* FocusRing::Get(const View* host) {
  return host->GetProperty(kFocusRingIdKey);
}

void FocusRing::Remove(View* host) {
  // Note that the FocusRing is owned by the View hierarchy, so we can't just
  // clear the key.
  FocusRing* const focus_ring = FocusRing::Get(host);
  if (!focus_ring)
    return;
  host->RemoveChildViewT(focus_ring);
  host->ClearProperty(kFocusRingIdKey);
}

FocusRing::~FocusRing() = default;

void FocusRing::SetPathGenerator(
    std::unique_ptr<HighlightPathGenerator> generator) {
  path_generator_ = std::move(generator);
  InvalidateLayout();
  SchedulePaint();
}

void FocusRing::SetInvalid(bool invalid) {
  invalid_ = invalid;
  SchedulePaint();
}

void FocusRing::SetHasFocusPredicate(const ViewPredicate& predicate) {
  has_focus_predicate_ = predicate;
  RefreshLayer();
}

std::optional<ui::ColorId> FocusRing::GetColorId() const {
  return color_id_;
}

void FocusRing::SetColorId(std::optional<ui::ColorId> color_id) {
  if (color_id_ == color_id)
    return;
  color_id_ = color_id;
  OnPropertyChanged(&color_id_, PropertyEffects::kPropertyEffectsPaint);
}

float FocusRing::GetHaloThickness() const {
  return halo_thickness_;
}

float FocusRing::GetHaloInset() const {
  return halo_inset_;
}

void FocusRing::SetHaloThickness(float halo_thickness) {
  if (halo_thickness_ == halo_thickness)
    return;
  halo_thickness_ = halo_thickness;
  OnPropertyChanged(&halo_thickness_, PropertyEffects::kPropertyEffectsPaint);
}

void FocusRing::SetHaloInset(float halo_inset) {
  if (halo_inset_ == halo_inset)
    return;
  halo_inset_ = halo_inset;
  OnPropertyChanged(&halo_inset_, PropertyEffects::kPropertyEffectsPaint);
}

void FocusRing::SetOutsetFocusRingDisabled(bool disable) {
  outset_focus_ring_disabled_ = disable;
}
bool FocusRing::GetOutsetFocusRingDisabled() const {
  return outset_focus_ring_disabled_;
}

bool FocusRing::ShouldPaintForTesting() {
  return ShouldPaint();
}

void FocusRing::Layout(PassKey) {
  // The focus ring handles its own sizing, which is simply to fill the parent
  // and extend a little beyond its borders.
  gfx::Rect focus_bounds = parent()->GetLocalBounds();

  // Make sure the focus-ring path fits.
  // TODO(pbos): Chase down use cases where this path is not in a usable state
  // by the time layout happens. This may be due to synchronous
  // DeprecatedLayoutImmediately() calls.
  const SkPath path = GetPath();
  if (IsPathUsable(path)) {
    const gfx::Rect path_bounds =
        gfx::ToEnclosingRect(gfx::SkRectToRectF(path.getBounds()));
    const gfx::Rect expanded_bounds =
        gfx::UnionRects(focus_bounds, path_bounds);
    // These insets are how much we need to inset `focus_bounds` to enclose the
    // path as well. They'll be either zero or negative (we're effectively
    // outsetting).
    gfx::Insets expansion_insets = focus_bounds.InsetsFrom(expanded_bounds);
    // Make sure we extend the focus-ring bounds symmetrically on the X axis to
    // retain the shared center point with parent(). This is required for canvas
    // flipping to position the focus-ring path correctly after the RTL flip.
    const int min_x_inset =
        std::min(expansion_insets.left(), expansion_insets.right());
    expansion_insets.set_left(min_x_inset);
    expansion_insets.set_right(min_x_inset);
    focus_bounds.Inset(expansion_insets);
  }
  if (ShouldSetOutsetFocusRing()) {
    focus_bounds.Outset(halo_thickness_ + kFocusRingOutset);
  } else {
    focus_bounds.Inset(gfx::Insets(halo_inset_));
    if (parent()->GetProperty(kDrawFocusRingBackgroundOutline)) {
      focus_bounds.Inset(gfx::Insets(-2 * kOutlineThickness));
    }
  }

  SetBoundsRect(focus_bounds);

  // Need to match canvas direction with the parent. This is required to ensure
  // asymmetric focus ring shapes match their respective buttons in RTL mode.
  SetFlipCanvasOnPaintForRTLUI(parent()->GetFlipCanvasOnPaintForRTLUI());
}

void FocusRing::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.child != this)
    return;

  if (details.is_add) {
    // Need to start observing the parent.
    view_observation_.Observe(details.parent);
    RefreshLayer();
  } else if (view_observation_.IsObservingSource(details.parent)) {
    // This view is being removed from its parent. It needs to remove itself
    // from its parent's observer list in the case where the FocusView is
    // removed from its parent but not deleted.
    view_observation_.Reset();
  }
}

void FocusRing::OnPaint(gfx::Canvas* canvas) {
  if (!ShouldPaint()) {
    return;
  }
  SkRRect ring_rect = GetRingRoundRect();
  cc::PaintFlags paint;
  paint.setAntiAlias(true);
  paint.setStyle(cc::PaintFlags::kStroke_Style);
  if (!ShouldSetOutsetFocusRing()) {
    // TODO(crbug.com/40257162): kDrawFocusRingBackgroundOutline should be
    // removed when ChromeRefresh is fully rolled out.
    if (parent()->GetProperty(kDrawFocusRingBackgroundOutline)) {
      // Draw with full stroke width + 2x outline thickness to effectively paint
      // the outline thickness on both sides of the FocusRing.
      paint.setStrokeWidth(halo_thickness_ + 2 * kOutlineThickness);
      paint.setColor(GetCascadingBackgroundColor(this));
      canvas->sk_canvas()->drawRRect(ring_rect, paint);
    }
  }
  paint.setColor(GetPaintColor(this, !invalid_));
  paint.setStrokeWidth(halo_thickness_);
  canvas->sk_canvas()->drawRRect(ring_rect, paint);
}

SkRRect FocusRing::GetRingRoundRect() const {
  const SkPath path = GetPath();

  DCHECK(IsPathUsable(path));
  DCHECK_EQ(GetFlipCanvasOnPaintForRTLUI(),
            parent()->GetFlipCanvasOnPaintForRTLUI());

  SkRect bounds;
  SkRRect rbounds;
  if (path.isRect(&bounds)) {
    AdjustBounds(bounds);
    return RingRectFromPathRect(bounds);
  }

  if (path.isOval(&bounds)) {
    AdjustBounds(bounds);
    gfx::RectF rect = gfx::SkRectToRectF(bounds);
    View::ConvertRectToTarget(parent(), this, &rect);
    return SkRRect::MakeOval(gfx::RectFToSkRect(rect));
  }

  CHECK(path.isRRect(&rbounds));
  AdjustBounds(rbounds);
  return RingRectFromPathRect(rbounds);
}

void FocusRing::OnThemeChanged() {
  View::OnThemeChanged();
  if (invalid_ || color_id_.has_value())
    SchedulePaint();
}

void FocusRing::OnViewFocused(View* view) {
  RefreshLayer();
}

void FocusRing::OnViewBlurred(View* view) {
  RefreshLayer();
}

void FocusRing::OnViewLayoutInvalidated(View* view) {
  InvalidateLayout();
}

FocusRing::FocusRing() {
  // Don't allow the view to process events.
  SetCanProcessEventsWithinSubtree(false);

  // This should never be included in the accessibility tree.
  GetViewAccessibility().SetIsIgnored(true);
}

void FocusRing::AdjustBounds(SkRect& rect) const {
  if (ShouldSetOutsetFocusRing()) {
    float focus_ring_adjustment = halo_thickness_ / 2 + kFocusRingOutset;
    rect.outset(focus_ring_adjustment, focus_ring_adjustment);
  }
}

void FocusRing::AdjustBounds(SkRRect& rect) const {
  if (ShouldSetOutsetFocusRing()) {
    float focus_ring_adjustment = halo_thickness_ / 2 + kFocusRingOutset;
    rect.outset(focus_ring_adjustment, focus_ring_adjustment);
  }
}

SkPath FocusRing::GetPath() const {
  SkPath path;
  if (path_generator_) {
    path = path_generator_->GetHighlightPath(parent());
    if (IsPathUsable(path))
      return path;
  }

  // If there's no path generator or the generated path is unusable, fall back
  // to the default.
  return GetHighlightPathInternal(parent(), halo_thickness_);
}

void FocusRing::RefreshLayer() {
  // TODO(pbos): This always keeps the layer alive if |has_focus_predicate_| is
  // set. This is done because we're not notified when the predicate might
  // return a different result and there are call sites that call SchedulePaint
  // on FocusRings and expect that to be sufficient.
  // The cleanup would be to always call has_focus_predicate_ here and make sure
  // that RefreshLayer gets called somehow whenever |has_focused_predicate_|
  // returns a new value.
  const bool should_paint =
      has_focus_predicate_ || (parent() && parent()->HasFocus());
  SetVisible(should_paint);
  if (should_paint) {
    // A layer is necessary to paint beyond the parent's bounds.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  } else {
    DestroyLayer();
  }
}

bool FocusRing::ShouldSetOutsetFocusRing() const {
  // TODO(crbug.com/40257162): Some places set a custom `halo_inset_` value to
  // move the focus ring away from the host. If those places want to outset the
  // focus ring in the chrome refresh style, they need to be audited separately
  // with UX.
  return !outset_focus_ring_disabled_ &&
         halo_inset_ == FocusRing::kDefaultHaloInset;
}

bool FocusRing::ShouldPaint() {
  // TODO(pbos): Reevaluate if this can turn into a DCHECK, e.g. we should
  // never paint if there's no parent focus.
  return has_focus_predicate_ ? has_focus_predicate_.Run(parent())
                              : parent()->HasFocus();
}

SkRRect FocusRing::RingRectFromPathRect(const SkRect& rect) const {
  const double corner_radius = GetCornerRadius(halo_thickness_);
  return RingRectFromPathRect(
      SkRRect::MakeRectXY(rect, corner_radius, corner_radius));
}

SkRRect FocusRing::RingRectFromPathRect(const SkRRect& rrect) const {
  const double thickness = halo_thickness_ / 2.f;
  gfx::RectF r = gfx::SkRectToRectF(rrect.rect());
  View::ConvertRectToTarget(parent(), this, &r);

  SkRRect skr =
      rrect.makeOffset(r.x() - rrect.rect().x(), r.y() - rrect.rect().y());

  // The focus indicator should hug the normal border, when present (as in the
  // case of text buttons). Since it's drawn outside the parent view, increase
  // the rounding slightly by adding half the ring thickness.
  skr.inset(halo_inset_, halo_inset_);
  skr.inset(thickness, thickness);

  return skr;
}

SkPath GetHighlightPath(const View* view, float halo_thickness) {
  SkPath path = GetHighlightPathInternal(view, halo_thickness);
  if (view->GetFlipCanvasOnPaintForRTLUI() && base::i18n::IsRTL()) {
    gfx::Point center = view->GetLocalBounds().CenterPoint();
    SkMatrix flip;
    flip.setScale(-1, 1, center.x(), center.y());
    path.transform(flip);
  }
  return path;
}

BEGIN_METADATA(FocusRing)
ADD_PROPERTY_METADATA(std::optional<ui::ColorId>, ColorId)
ADD_PROPERTY_METADATA(float, HaloInset)
ADD_PROPERTY_METADATA(float, HaloThickness)
ADD_PROPERTY_METADATA(bool, OutsetFocusRingDisabled)
END_METADATA

}  // namespace views
