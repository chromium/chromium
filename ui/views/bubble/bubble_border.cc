// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_border.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/path.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/painter.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/view.h"

namespace views {

namespace {

// GetShadowValues and GetBorderAndShadowFlags cache their results. The shadow
// values depend on both the shadow elevation and color, so we create a tuple to
// key the cache.
typedef std::tuple<int, SkColor> ShadowCacheKey;

// The border is stroked at 1px, but for the purposes of reserving space we have
// to deal in dip coordinates, so round up to 1dip.
constexpr int kBorderThicknessDip = 1;

// Utility functions for getting alignment points on the edge of a rectangle.
gfx::Point CenterTop(const gfx::Rect& rect) {
  return gfx::Point(rect.CenterPoint().x(), rect.y());
}

gfx::Point CenterBottom(const gfx::Rect& rect) {
  return gfx::Point(rect.CenterPoint().x(), rect.bottom());
}

gfx::Point LeftCenter(const gfx::Rect& rect) {
  return gfx::Point(rect.x(), rect.CenterPoint().y());
}

gfx::Point RightCenter(const gfx::Rect& rect) {
  return gfx::Point(rect.right(), rect.CenterPoint().y());
}

}  // namespace

const int BubbleBorder::kStroke = 1;

BubbleBorder::BubbleBorder(Arrow arrow, Shadow shadow, SkColor color)
    : arrow_(arrow),
      arrow_offset_(0),
      shadow_(shadow),
      background_color_(color),
      use_theme_background_color_(false) {
  DCHECK(shadow_ < SHADOW_COUNT);
}

BubbleBorder::~BubbleBorder() {}

// static
gfx::Insets BubbleBorder::GetBorderAndShadowInsets(
    base::Optional<int> elevation) {
  // Borders with custom shadow elevations do not draw the 1px border.
  if (elevation.has_value())
    return -gfx::ShadowValue::GetMargin(GetShadowValues(elevation));

  constexpr gfx::Insets blur(kShadowBlur + kBorderThicknessDip);
  constexpr gfx::Insets offset(-kShadowVerticalOffset, 0, kShadowVerticalOffset,
                               0);
  return blur + offset;
}

void BubbleBorder::SetCornerRadius(int corner_radius) {
  corner_radius_ = corner_radius;
}

gfx::Rect BubbleBorder::GetBounds(const gfx::Rect& anchor_rect,
                                  const gfx::Size& contents_size) const {
  // In MD, there are no arrows, so positioning logic is significantly simpler.
  // TODO(estade): handle more anchor positions.
  if (arrow_ == TOP_RIGHT || arrow_ == TOP_LEFT || arrow_ == BOTTOM_CENTER ||
      arrow_ == TOP_CENTER || arrow_ == LEFT_CENTER || arrow_ == RIGHT_CENTER) {
    gfx::Rect contents_bounds(contents_size);
    // Apply the border part of the inset before calculating coordinates because
    // the border should align with the anchor's border. For the purposes of
    // positioning, the border is rounded up to a dip, which may mean we have
    // misalignment in scale factors greater than 1. Borders with custom shadow
    // elevations do not draw the 1px border.
    // TODO(estade): when it becomes possible to provide px bounds instead of
    // dip bounds, fix this.
    const gfx::Insets border_insets =
        shadow_ == NO_ASSETS || md_shadow_elevation_.has_value()
            ? gfx::Insets()
            : gfx::Insets(kBorderThicknessDip);
    const gfx::Insets shadow_insets = GetInsets() - border_insets;
    contents_bounds.Inset(-border_insets);
    if (arrow_ == TOP_RIGHT) {
      contents_bounds +=
          anchor_rect.bottom_right() - contents_bounds.top_right();
    } else if (arrow_ == TOP_LEFT) {
      contents_bounds +=
          anchor_rect.bottom_left() - contents_bounds.origin();
    } else if (arrow_ == BOTTOM_CENTER) {
      contents_bounds += CenterTop(anchor_rect) - CenterBottom(contents_bounds);
    } else if (arrow_ == TOP_CENTER) {
      contents_bounds += CenterBottom(anchor_rect) - CenterTop(contents_bounds);
    } else if (arrow_ == LEFT_CENTER) {
      contents_bounds += RightCenter(anchor_rect) - LeftCenter(contents_bounds);
    } else if (arrow_ == RIGHT_CENTER) {
      contents_bounds += LeftCenter(anchor_rect) - RightCenter(contents_bounds);
    }
    // With NO_ASSETS, there should be further insets, but the same logic is
    // used to position the bubble origin according to |anchor_rect|.
    DCHECK((shadow_ != NO_ASSETS && shadow_ != NO_SHADOW) ||
           shadow_insets.IsEmpty());
    contents_bounds.Inset(-shadow_insets);
    // |arrow_offset_| is used to adjust bubbles that would normally be
    // partially offscreen.
    contents_bounds += gfx::Vector2d(-arrow_offset_, 0);
    return contents_bounds;
  }

  int x = anchor_rect.x();
  int y = anchor_rect.y();
  int w = anchor_rect.width();
  int h = anchor_rect.height();
  const gfx::Size size(GetSizeForContentsSize(contents_size));
  const int stroke_width = shadow_ == NO_ASSETS ? 0 : kStroke;

  // Calculate the bubble coordinates based on the border and arrow settings.
  if (is_arrow_on_horizontal(arrow_)) {
    if (is_arrow_on_left(arrow_)) {
      x += stroke_width;
    } else if (is_arrow_at_center(arrow_)) {
      x += w / 2;
    } else {
      x += w - size.width() - stroke_width;
    }
    y += is_arrow_on_top(arrow_) ? h : -size.height();
  } else if (has_arrow(arrow_)) {
    x += is_arrow_on_left(arrow_) ? w : -size.width();
    if (is_arrow_on_top(arrow_)) {
      y += stroke_width;
    } else if (is_arrow_at_center(arrow_)) {
      y += h / 2;
    } else {
      y += h - size.height() - stroke_width;
    }
  } else {
    x += (w - size.width()) / 2;
    y += (arrow_ == NONE) ? h : (h - size.height()) / 2;
  }

  return gfx::Rect(x, y, size.width(), size.height());
}

int BubbleBorder::GetBorderCornerRadius() const {
  constexpr int kCornerRadius = 2;
  return corner_radius_.value_or(kCornerRadius);
}

void BubbleBorder::Paint(const views::View& view, gfx::Canvas* canvas) {
  if (shadow_ == NO_ASSETS)
    return PaintNoAssets(view, canvas);

  if (shadow_ == NO_SHADOW)
    return PaintNoShadow(view, canvas);

  gfx::ScopedCanvas scoped(canvas);

  SkRRect r_rect = GetClientRect(view);
  canvas->sk_canvas()->clipRRect(r_rect, SkClipOp::kDifference,
                                 true /*doAntiAlias*/);

  DrawBorderAndShadow(std::move(r_rect), &cc::PaintCanvas::drawRRect, canvas,
                      md_shadow_elevation_, md_shadow_color_);
}

gfx::Insets BubbleBorder::GetInsets() const {
  if (shadow_ == NO_ASSETS)
    return gfx::Insets();
  if (shadow_ == NO_SHADOW)
    return gfx::Insets(kBorderThicknessDip);
  return GetBorderAndShadowInsets(md_shadow_elevation_);
}

gfx::Size BubbleBorder::GetMinimumSize() const {
  return GetSizeForContentsSize(gfx::Size());
}

// static
const gfx::ShadowValues& BubbleBorder::GetShadowValues(
    base::Optional<int> elevation,
    SkColor color) {
  // The shadows are always the same for any elevation and color combination, so
  // construct them once and cache.
  static base::NoDestructor<std::map<ShadowCacheKey, gfx::ShadowValues>>
      shadow_map;
  ShadowCacheKey key(elevation.value_or(-1), color);

  if (shadow_map->find(key) != shadow_map->end())
    return shadow_map->find(key)->second;

  gfx::ShadowValues shadows;
  if (elevation.has_value()) {
    DCHECK(elevation.value() >= 0);
    shadows = LayoutProvider::Get()->MakeShadowValues(elevation.value(), color);
  } else {
    constexpr int kSmallShadowVerticalOffset = 2;
    constexpr int kSmallShadowBlur = 4;
    SkColor kSmallShadowColor = SkColorSetA(color, 0x33);
    SkColor kLargeShadowColor = SkColorSetA(color, 0x1A);
    // gfx::ShadowValue counts blur pixels both inside and outside the shape,
    // whereas these blur values only describe the outside portion, hence they
    // must be doubled.
    shadows = gfx::ShadowValues({
        {gfx::Vector2d(0, kSmallShadowVerticalOffset), 2 * kSmallShadowBlur,
         kSmallShadowColor},
        {gfx::Vector2d(0, kShadowVerticalOffset), 2 * kShadowBlur,
         kLargeShadowColor},
    });
  }

  shadow_map->insert({key, shadows});
  return shadow_map->find(key)->second;
}

// static
const cc::PaintFlags& BubbleBorder::GetBorderAndShadowFlags(
    base::Optional<int> elevation,
    SkColor color) {
  // The flags are always the same for any elevation and color combination, so
  // construct them once and cache.
  static base::NoDestructor<std::map<ShadowCacheKey, cc::PaintFlags>> flag_map;
  ShadowCacheKey key(elevation.value_or(-1), color);

  if (flag_map->find(key) != flag_map->end())
    return flag_map->find(key)->second;

  cc::PaintFlags flags;
  constexpr SkColor kBlurredBorderColor = SkColorSetA(SK_ColorBLACK, 0x26);
  flags.setColor(kBlurredBorderColor);
  flags.setAntiAlias(true);
  flags.setLooper(
      gfx::CreateShadowDrawLooper(GetShadowValues(elevation, color)));

  flag_map->insert({key, flags});
  return flag_map->find(key)->second;
}

gfx::Size BubbleBorder::GetSizeForContentsSize(
    const gfx::Size& contents_size) const {
  // Enlarge the contents size by the thickness of the border images.
  gfx::Size size(contents_size);
  const gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

SkRRect BubbleBorder::GetClientRect(const View& view) const {
  gfx::RectF bounds(view.GetLocalBounds());
  bounds.Inset(GetInsets());
  return SkRRect::MakeRectXY(gfx::RectFToSkRect(bounds),
                             GetBorderCornerRadius(), GetBorderCornerRadius());
}

void BubbleBorder::PaintNoAssets(const View& view, gfx::Canvas* canvas) {
  gfx::ScopedCanvas scoped(canvas);
  canvas->sk_canvas()->clipRRect(GetClientRect(view), SkClipOp::kDifference,
                                 true /*doAntiAlias*/);
  canvas->sk_canvas()->drawColor(SK_ColorTRANSPARENT, SkBlendMode::kSrc);
}

void BubbleBorder::PaintNoShadow(const View& view, gfx::Canvas* canvas) {
  gfx::RectF bounds(view.GetLocalBounds());
  bounds.Inset(gfx::InsetsF(kBorderThicknessDip / 2.0f));
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kBorderThicknessDip);
  constexpr SkColor kBorderColor = gfx::kGoogleGrey600;
  flags.setColor(kBorderColor);
  canvas->DrawRoundRect(bounds, GetBorderCornerRadius(), flags);
}

void BubbleBackground::Paint(gfx::Canvas* canvas, views::View* view) const {
  if (border_->shadow() == BubbleBorder::NO_SHADOW_OPAQUE_BORDER)
    canvas->DrawColor(border_->background_color());

  // Fill the contents with a round-rect region to match the border images.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(border_->background_color());
  SkPath path;
  gfx::RectF bounds(view->GetLocalBounds());
  bounds.Inset(gfx::InsetsF(border_->GetInsets()));

  canvas->DrawRoundRect(bounds, border_->GetBorderCornerRadius(), flags);
}

}  // namespace views
