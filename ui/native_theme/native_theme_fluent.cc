// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_fluent.h"

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/native_theme/native_theme_constants_fluent.h"
#include "ui/native_theme/native_theme_features.h"

namespace ui {

NativeThemeFluent::NativeThemeFluent(bool should_only_use_dark_colors)
    : NativeThemeBase(should_only_use_dark_colors) {
  scrollbar_width_ = kFluentScrollbarThickness;

  const sk_sp<SkFontMgr> font_manager(SkFontMgr::RefDefault());
  typeface_ = sk_sp<SkTypeface>(
      font_manager->matchFamilyStyle(kFluentScrollbarFont, SkFontStyle()));
}

NativeThemeFluent::~NativeThemeFluent() = default;

// static
NativeThemeFluent* NativeThemeFluent::web_instance() {
  static base::NoDestructor<NativeThemeFluent> s_native_theme_for_web(
      /*should_only_use_dark_colors=*/false);
  return s_native_theme_for_web.get();
}

void NativeThemeFluent::PaintArrowButton(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const gfx::Rect& rect,
    Part direction,
    State state,
    ColorScheme color_scheme,
    const ScrollbarArrowExtraParams& arrow) const {
  PaintButton(canvas, color_provider, rect, direction, color_scheme, arrow);
  PaintArrow(canvas, color_provider, rect, direction, state, color_scheme,
             arrow);
}

void NativeThemeFluent::PaintScrollbarTrack(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect,
    ColorScheme color_scheme) const {
  gfx::Rect track_fill_rect = rect;
  if (InForcedColorsMode()) {
    gfx::Insets edge_insets;
    if (part == NativeTheme::Part::kScrollbarHorizontalTrack) {
      edge_insets.set_left_right(-kFluentScrollbarTrackOutlineWidth,
                                 -kFluentScrollbarTrackOutlineWidth);
    } else {
      edge_insets.set_top_bottom(-kFluentScrollbarTrackOutlineWidth,
                                 -kFluentScrollbarTrackOutlineWidth);
    }
    const gfx::InsetsF outline_insets(kFluentScrollbarTrackOutlineWidth / 2.0f);

    gfx::RectF outline_rect(rect);
    outline_rect.Inset(outline_insets + gfx::InsetsF(edge_insets));

    const SkColor track_outline_color =
        color_provider->GetColor(kColorWebNativeControlScrollbarThumb);

    cc::PaintFlags outline_flags;
    outline_flags.setColor(track_outline_color);
    outline_flags.setStyle(cc::PaintFlags::kStroke_Style);
    outline_flags.setStrokeWidth(kFluentScrollbarTrackOutlineWidth);
    canvas->drawRect(gfx::RectFToSkRect(outline_rect), outline_flags);

    // Adjust fill rect to not overlap with the outline stroke rect.
    constexpr gfx::Insets fill_insets(kFluentScrollbarTrackOutlineWidth);
    track_fill_rect.Inset(fill_insets + edge_insets);
  }
  const SkColor track_color =
      extra_params.track_color.has_value()
          ? extra_params.track_color.value()
          : color_provider->GetColor(kColorWebNativeControlScrollbarTrack);
  cc::PaintFlags flags;
  flags.setColor(track_color);
  canvas->drawIRect(gfx::RectToSkIRect(track_fill_rect), flags);
}

void NativeThemeFluent::PaintScrollbarThumb(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    Part part,
    State state,
    const gfx::Rect& rect,
    const ScrollbarThumbExtraParams& extra_params,
    ColorScheme color_scheme) const {
  DCHECK_NE(state, NativeTheme::kDisabled);

  cc::PaintCanvasAutoRestore auto_restore(canvas, true);
  SkRRect rrect =
      SkRRect::MakeRectXY(gfx::RectToSkRect(rect), kFluentScrollbarPartsRadius,
                          kFluentScrollbarPartsRadius);

  // Clip the canvas to match the round rect and create round corners.
  SkPath path;
  path.addRRect(rrect);
  canvas->clipPath(path, true);

  ColorId thumb_color_id = kColorWebNativeControlScrollbarThumb;
  if (state == NativeTheme::kPressed) {
    thumb_color_id = kColorWebNativeControlScrollbarThumbPressed;
  } else if (state == NativeTheme::kHovered) {
    thumb_color_id = kColorWebNativeControlScrollbarThumbHovered;
  }
  const SkColor thumb_color = color_provider->GetColor(thumb_color_id);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(thumb_color);
  canvas->drawRect(gfx::RectToSkRect(rect), flags);
}

void NativeThemeFluent::PaintScrollbarCorner(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const ScrollbarTrackExtraParams& extra_params,
    ColorScheme color_scheme) const {
  cc::PaintFlags flags;
  const SkColor corner_color =
      extra_params.track_color.has_value()
          ? extra_params.track_color.value()
          : color_provider->GetColor(kColorWebNativeControlScrollbarCorner);
  flags.setColor(corner_color);
  canvas->drawIRect(RectToSkIRect(rect), flags);
}

gfx::Size NativeThemeFluent::GetPartSize(Part part,
                                         State state,
                                         const ExtraParams& extra) const {
  switch (part) {
    case kScrollbarHorizontalThumb:
      return gfx::Size(kFluentScrollbarMinimalThumbLength,
                       kFluentScrollbarThumbThickness);
    case kScrollbarVerticalThumb:
      return gfx::Size(kFluentScrollbarThumbThickness,
                       kFluentScrollbarMinimalThumbLength);
    case kScrollbarHorizontalTrack:
      return gfx::Size(0, scrollbar_width_);
    case kScrollbarVerticalTrack:
      return gfx::Size(scrollbar_width_, 0);
    case kScrollbarUpArrow:
    case kScrollbarDownArrow:
      return gfx::Size(scrollbar_width_, kFluentScrollbarButtonSideLength);
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
      return gfx::Size(kFluentScrollbarButtonSideLength, scrollbar_width_);
    default:
      break;
  }

  return NativeThemeBase::GetPartSize(part, state, extra);
}

void NativeThemeFluent::PaintButton(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const gfx::Rect& rect,
    Part direction,
    ColorScheme color_scheme,
    const ScrollbarArrowExtraParams& arrow) const {
  cc::PaintFlags flags;
  const SkColor button_color =
      arrow.track_color.has_value()
          ? arrow.track_color.value()
          : color_provider->GetColor(kColorWebNativeControlScrollbarTrack);
  flags.setColor(button_color);
  gfx::Rect button_fill_rect = rect;
  if (InForcedColorsMode()) {
    const gfx::InsetsF outline_insets(kFluentScrollbarTrackOutlineWidth / 2.0f);
    gfx::Insets edge_insets;
    if (direction == NativeTheme::Part::kScrollbarUpArrow) {
      edge_insets.set_bottom(-kFluentScrollbarTrackOutlineWidth);
    } else if (direction == NativeTheme::Part::kScrollbarDownArrow) {
      edge_insets.set_top(-kFluentScrollbarTrackOutlineWidth);
    } else if (direction == NativeTheme::Part::kScrollbarLeftArrow) {
      edge_insets.set_right(-kFluentScrollbarTrackOutlineWidth);
    } else if (direction == NativeTheme::Part::kScrollbarRightArrow) {
      edge_insets.set_left(-kFluentScrollbarTrackOutlineWidth);
    }

    gfx::RectF outline_rect(rect);
    outline_rect.Inset(outline_insets + gfx::InsetsF(edge_insets));
    const SkColor arrow_outline_color =
        color_provider->GetColor(kColorWebNativeControlScrollbarThumb);

    cc::PaintFlags outline_flags;
    outline_flags.setColor(arrow_outline_color);
    outline_flags.setStyle(cc::PaintFlags::kStroke_Style);
    outline_flags.setStrokeWidth(kFluentScrollbarTrackOutlineWidth);

    if (IsFluentOverlayScrollbarEnabled()) {
      PaintRoundedButton(canvas, gfx::RectFToSkRect(outline_rect),
                         outline_flags, direction);
    } else {
      canvas->drawRect(gfx::RectFToSkRect(outline_rect), outline_flags);
    }

    // Adjust the fill rect to not overlap with the outline stroke rect.
    constexpr gfx::Insets fill_insets(kFluentScrollbarTrackOutlineWidth);
    button_fill_rect.Inset(fill_insets + edge_insets);
  }

  if (IsFluentOverlayScrollbarEnabled()) {
    PaintRoundedButton(canvas, gfx::RectToSkRect(button_fill_rect), flags,
                       direction);
  } else {
    canvas->drawIRect(gfx::RectToSkIRect(button_fill_rect), flags);
  }
}

void NativeThemeFluent::PaintArrow(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const gfx::Rect& rect,
    Part part,
    State state,
    ColorScheme color_scheme,
    const ScrollbarArrowExtraParams& arrow) const {
  const ColorId arrow_color_id =
      state == NativeTheme::kPressed || state == NativeTheme::kHovered
          ? kColorWebNativeControlScrollbarArrowForegroundPressed
          : kColorWebNativeControlScrollbarArrowForeground;
  // TODO(crbug.com/891944): Adjust thumb_color based on `state`.
  const SkColor arrow_color = arrow.thumb_color.has_value()
                                  ? arrow.thumb_color.value()
                                  : color_provider->GetColor(arrow_color_id);
  cc::PaintFlags flags;
  flags.setColor(arrow_color);

  if (!ArrowIconsAvailable()) {
    // Paint regular triangular arrows if the font with arrow icons is not
    // available. GetArrowRect() returns the float rect but it is expected to be
    // the integer rect in this case.
    const SkPath path =
        PathForArrow(ToNearestRect(GetArrowRect(rect, part, state)), part);
    canvas->drawPath(path, flags);
    return;
  }

  const gfx::RectF bounding_rect = GetArrowRect(rect, part, state);
  // The bounding rect for an arrow is a square, so that we can use the width
  // despite the arrow direction.
  DCHECK(typeface_);
  SkFont font(typeface_, bounding_rect.width());
  font.setEdging(SkFont::Edging::kSubpixelAntiAlias);
  font.setSubpixel(true);
  flags.setAntiAlias(true);
  const char* arrow_code_point = GetArrowCodePointForScrollbarPart(part);
  canvas->drawTextBlob(SkTextBlob::MakeFromString(arrow_code_point, font),
                       bounding_rect.x(), bounding_rect.bottom(), flags);
}

gfx::RectF NativeThemeFluent::GetArrowRect(const gfx::Rect& rect,
                                           Part part,
                                           State state) const {
  int min_rect_side, max_rect_side;
  std::tie(min_rect_side, max_rect_side) =
      std::minmax(rect.width(), rect.height());
  const int arrow_side = GetArrowSideLength(state);

  // Calculates the scaling ratio used to determine the arrow rect side length.
  const float arrow_to_button_side_scale_ratio =
      arrow_side / static_cast<float>(kFluentScrollbarButtonSideLength);
  int side_length =
      base::ClampCeil(max_rect_side * arrow_to_button_side_scale_ratio);

  gfx::RectF arrow_rect(rect);
  if (ArrowIconsAvailable()) {
    arrow_rect.ClampToCenteredSize(gfx::SizeF(side_length, side_length));
  } else {
    // Add 1px to the side length if the difference between smaller button rect
    // and arrow side length is odd to keep the arrow rect in the center as well
    // as use int coordinates. This avoids the usage of anti-aliasing.
    side_length += (min_rect_side - side_length) % 2;
    arrow_rect.ClampToCenteredSize(gfx::SizeF(side_length, side_length));
    arrow_rect.set_origin(
        gfx::PointF(std::floor(arrow_rect.x()), std::floor(arrow_rect.y())));
  }

  // The end result is a centered arrow rect within the button rect with the
  // applied offset.
  OffsetArrowRect(arrow_rect, part, max_rect_side);
  return arrow_rect;
}

int NativeThemeFluent::GetArrowSideLength(State state) const {
  if (state == NativeTheme::kPressed)
    return ArrowIconsAvailable()
               ? kFluentScrollbarPressedArrowRectLength
               : kFluentScrollbarPressedArrowRectFallbackLength;

  return kFluentScrollbarArrowRectLength;
}

void NativeThemeFluent::OffsetArrowRect(gfx::RectF& arrow_rect,
                                        Part part,
                                        int max_rect_side) const {
  const float scaled_offset =
      std::round(kFluentScrollbarArrowOffset * max_rect_side /
                 static_cast<float>(kFluentScrollbarButtonSideLength));
  switch (part) {
    case kScrollbarUpArrow:
      arrow_rect.Offset(0, -scaled_offset);
      break;
    case kScrollbarDownArrow:
      arrow_rect.Offset(0, scaled_offset);
      break;
    case kScrollbarLeftArrow:
      arrow_rect.Offset(-scaled_offset, 0);
      break;
    case kScrollbarRightArrow:
      arrow_rect.Offset(scaled_offset, 0);
      break;
    default:
      NOTREACHED();
  }
}

const char* NativeThemeFluent::GetArrowCodePointForScrollbarPart(
    Part part) const {
  switch (part) {
    case Part::kScrollbarUpArrow:
      return kFluentScrollbarUpArrow;
    case Part::kScrollbarDownArrow:
      return kFluentScrollbarDownArrow;
    case Part::kScrollbarLeftArrow:
      return kFluentScrollbarLeftArrow;
    case Part::kScrollbarRightArrow:
      return kFluentScrollbarRightArrow;
    default:
      NOTREACHED_NORETURN();
  }
}

int NativeThemeFluent::GetPaintedScrollbarTrackInset() const {
  return kFluentPaintedScrollbarTrackInset;
}

void NativeThemeFluent::PaintRoundedButton(cc::PaintCanvas* canvas,
                                           SkRect rect,
                                           cc::PaintFlags paint_flags,
                                           NativeTheme::Part direction) const {
  paint_flags.setAntiAlias(true);

  SkScalar upper_left_radius = 0;
  SkScalar lower_left_radius = 0;
  SkScalar upper_right_radius = 0;
  SkScalar lower_right_radius = 0;
  if (direction == NativeTheme::kScrollbarUpArrow) {
    upper_left_radius = kFluentScrollbarPartsRadius;
    upper_right_radius = kFluentScrollbarPartsRadius;
  } else if (direction == NativeTheme::kScrollbarDownArrow) {
    lower_left_radius = kFluentScrollbarPartsRadius;
    lower_right_radius = kFluentScrollbarPartsRadius;
  } else if (direction == NativeTheme::kScrollbarLeftArrow) {
    lower_left_radius = kFluentScrollbarPartsRadius;
    upper_left_radius = kFluentScrollbarPartsRadius;
  } else if (direction == NativeTheme::kScrollbarRightArrow) {
    lower_right_radius = kFluentScrollbarPartsRadius;
    upper_right_radius = kFluentScrollbarPartsRadius;
  }

  gfx::RRectF rounded_rect(
      gfx::SkRectToRectF(rect), upper_left_radius, upper_left_radius,
      upper_right_radius, upper_right_radius, lower_right_radius,
      lower_right_radius, lower_left_radius, lower_left_radius);
  canvas->drawRRect(static_cast<SkRRect>(rounded_rect), paint_flags);
}

}  // namespace ui
