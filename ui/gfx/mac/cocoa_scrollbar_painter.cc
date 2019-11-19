// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mac/cocoa_scrollbar_painter.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"

namespace gfx {

using Params = CocoaScrollbarPainter::Params;
using Orientation = CocoaScrollbarPainter::Orientation;

namespace {

// The width of the scroller track border.
constexpr int kTrackBorderWidth = 1;

// The amount the thumb is inset from the ends and the inside edge of track
// border.
constexpr int kThumbInset = 3;
constexpr int kThumbInsetOverlay = 2;

// The minimum sizes for the thumb. We will not inset the thumb if it will
// be smaller than this size.
constexpr int kThumbMinGirth = 6;
constexpr int kThumbMinLength = 18;

// Scrollbar thumb colors.
constexpr SkColor kThumbColorDefault = SkColorSetARGB(0x3A, 0, 0, 0);
constexpr SkColor kThumbColorHover = SkColorSetARGB(0x80, 0, 0, 0);
constexpr SkColor kThumbColorDarkMode = SkColorSetRGB(0x6B, 0x6B, 0x6B);
constexpr SkColor kThumbColorDarkModeHover = SkColorSetRGB(0x93, 0x93, 0x93);
constexpr SkColor kThumbColorOverlay = SkColorSetARGB(0x80, 0, 0, 0);
constexpr SkColor kThumbColorOverlayDarkMode =
    SkColorSetARGB(0x80, 0xFF, 0xFF, 0xFF);

// Non-overlay scroller track colors are not transparent. On Safari, they are,
// but on all other macOS applications they are not.
constexpr SkColor kTrackGradientColors[] = {
    SkColorSetRGB(0xFA, 0xFA, 0xFA),
    SkColorSetRGB(0xFA, 0xFA, 0xFA),
};
constexpr SkColor kTrackInnerBorderColor = SkColorSetRGB(0xE8, 0xE8, 0xE8);
constexpr SkColor kTrackOuterBorderColor = SkColorSetRGB(0xED, 0xED, 0xED);

// Non-overlay dark mode scroller track colors.
constexpr SkColor kTrackGradientColorsDarkMode[] = {
    SkColorSetRGB(0x2D, 0x2D, 0x2D),
    SkColorSetRGB(0x2B, 0x2B, 0x2B),
};
constexpr SkColor kTrackInnerBorderColorDarkMode =
    SkColorSetRGB(0x3D, 0x3D, 0x3D);
constexpr SkColor kTrackOuterBorderColorDarkMode =
    SkColorSetRGB(0x51, 0x51, 0x51);

// Overlay scroller track colors are transparent.
constexpr SkColor kTrackGradientColorsOverlay[] = {
    SkColorSetARGB(0xC6, 0xF8, 0xF8, 0xF8),
    SkColorSetARGB(0xC2, 0xF8, 0xF8, 0xF8),
    SkColorSetARGB(0xC2, 0xF8, 0xF8, 0xF8),
    SkColorSetARGB(0xC2, 0xF8, 0xF8, 0xF8),
};
constexpr SkColor kTrackInnerBorderColorOverlay =
    SkColorSetARGB(0xF9, 0xDF, 0xDF, 0xDF);
constexpr SkColor kTrackOuterBorderColorOverlay =
    SkColorSetARGB(0xC6, 0xE8, 0xE8, 0xE8);

// Dark mode overlay scroller track colors.
constexpr SkColor kTrackGradientColorsOverlayDarkMode[] = {
    SkColorSetARGB(0x28, 0xD8, 0xD8, 0xD8),
    SkColorSetARGB(0x26, 0xCC, 0xCC, 0xCC),
    SkColorSetARGB(0x26, 0xCC, 0xCC, 0xCC),
    SkColorSetARGB(0x26, 0xCC, 0xCC, 0xCC),
};
constexpr SkColor kTrackInnerBorderColorOverlayDarkMode =
    SkColorSetARGB(0x33, 0xE5, 0xE5, 0xE5);
constexpr SkColor kTrackOuterBorderColorOverlayDarkMode =
    SkColorSetARGB(0x28, 0xD8, 0xD8, 0xD8);

void ConstrainInsets(int old_width, int min_width, int* left, int* right) {
  int requested_total_inset = *left + *right;
  if (requested_total_inset == 0)
    return;
  int max_total_inset = old_width - min_width;
  if (requested_total_inset < max_total_inset)
    return;
  if (max_total_inset < 0) {
    *left = *right = 0;
    return;
  }
  // Multiply the right/bottom inset by the ratio by which we need to shrink the
  // total inset. This has the effect of rounding down the right/bottom inset,
  // if the two sides are to be affected unevenly.
  *right *= max_total_inset * 1.f / requested_total_inset;
  *left = max_total_inset - *right;
}

void ConstrainedInset(gfx::Rect* rect,
                      int min_width,
                      int min_height,
                      int inset_left,
                      int inset_top,
                      int inset_right,
                      int inset_bottom) {
  ConstrainInsets(rect->width(), min_width, &inset_left, &inset_right);
  ConstrainInsets(rect->height(), min_height, &inset_top, &inset_bottom);
  rect->Inset(inset_left, inset_top, inset_right, inset_bottom);
}

void PaintTrackGradient(gfx::Canvas* canvas,
                        const gfx::Rect& rect,
                        const Params& params,
                        bool is_corner) {
  // Select colors.
  const SkColor* gradient_colors = nullptr;
  size_t gradient_stops = 0;
  if (params.overlay) {
    if (params.dark_mode) {
      gradient_colors = kTrackGradientColorsOverlayDarkMode;
      gradient_stops = base::size(kTrackGradientColorsOverlayDarkMode);
    } else {
      gradient_colors = kTrackGradientColorsOverlay;
      gradient_stops = base::size(kTrackGradientColorsOverlay);
    }
  } else {
    if (params.dark_mode) {
      gradient_colors = kTrackGradientColorsDarkMode;
      gradient_stops = base::size(kTrackGradientColorsDarkMode);
    } else {
      gradient_colors = kTrackGradientColors;
      gradient_stops = base::size(kTrackGradientColors);
    }
  }

  // Set the gradient direction.
  const SkPoint gradient_bounds_vertical[] = {
      gfx::PointToSkPoint(rect.origin()),
      gfx::PointToSkPoint(rect.bottom_left()),
  };
  const SkPoint gradient_bounds_horizontal[] = {
      gfx::PointToSkPoint(rect.origin()),
      gfx::PointToSkPoint(rect.top_right()),
  };
  const SkPoint gradient_bounds_corner_right[] = {
      gfx::PointToSkPoint(rect.origin()),
      gfx::PointToSkPoint(rect.bottom_right()),
  };
  const SkPoint gradient_bounds_corner_left[] = {
      gfx::PointToSkPoint(rect.top_right()),
      gfx::PointToSkPoint(rect.bottom_left()),
  };
  const SkPoint* gradient_bounds = nullptr;
  if (is_corner) {
    if (params.orientation == Orientation::kVerticalOnRight)
      gradient_bounds = gradient_bounds_corner_right;
    else
      gradient_bounds = gradient_bounds_corner_left;
  } else {
    if (params.orientation == Orientation::kHorizontal)
      gradient_bounds = gradient_bounds_horizontal;
    else
      gradient_bounds = gradient_bounds_vertical;
  }

  // And draw.
  cc::PaintFlags gradient;
  gradient.setShader(cc::PaintShader::MakeLinearGradient(
      gradient_bounds, gradient_colors, nullptr, gradient_stops,
      SkTileMode::kClamp));
  canvas->DrawRect(rect, gradient);
}

void PaintTrackInnerBorder(gfx::Canvas* canvas,
                           const gfx::Rect& rect,
                           const Params& params,
                           bool is_corner) {
  // Select the color.
  SkColor inner_border_color = 0;
  if (params.overlay) {
    if (params.dark_mode)
      inner_border_color = kTrackInnerBorderColorOverlayDarkMode;
    else
      inner_border_color = kTrackInnerBorderColorOverlay;
  } else {
    if (params.dark_mode)
      inner_border_color = kTrackInnerBorderColorDarkMode;
    else
      inner_border_color = kTrackInnerBorderColor;
  }

  // Compute the rect for the border.
  gfx::Rect inner_border(rect);
  if (params.orientation == Orientation::kVerticalOnLeft)
    inner_border.set_x(rect.right() - kTrackBorderWidth);
  if (is_corner || params.orientation == Orientation::kHorizontal)
    inner_border.set_height(kTrackBorderWidth);
  if (is_corner || params.orientation != Orientation::kHorizontal)
    inner_border.set_width(kTrackBorderWidth);

  // And draw.
  cc::PaintFlags flags;
  flags.setColor(inner_border_color);
  canvas->DrawRect(inner_border, flags);
}

void PaintTrackOuterBorder(gfx::Canvas* canvas,
                           const gfx::Rect& rect,
                           const Params& params,
                           bool is_corner) {
  // Select the color.
  SkColor outer_border_color = 0;
  if (params.overlay) {
    if (params.dark_mode)
      outer_border_color = kTrackOuterBorderColorOverlayDarkMode;
    else
      outer_border_color = kTrackOuterBorderColorOverlay;
  } else {
    if (params.dark_mode)
      outer_border_color = kTrackOuterBorderColorDarkMode;
    else
      outer_border_color = kTrackOuterBorderColor;
  }
  cc::PaintFlags flags;
  flags.setColor(outer_border_color);

  // Draw the horizontal outer border.
  if (is_corner || params.orientation == Orientation::kHorizontal) {
    gfx::Rect outer_border(rect);
    outer_border.set_height(kTrackBorderWidth);
    outer_border.set_y(rect.bottom() - kTrackBorderWidth);
    canvas->DrawRect(outer_border, flags);
  }

  // Draw the vertial outer border.
  if (is_corner || params.orientation != Orientation::kHorizontal) {
    gfx::Rect outer_border(rect);
    outer_border.set_width(kTrackBorderWidth);
    if (params.orientation == Orientation::kVerticalOnRight)
      outer_border.set_x(rect.right() - kTrackBorderWidth);
    canvas->DrawRect(outer_border, flags);
  }
}

}  // namespace

// static
void CocoaScrollbarPainter::PaintTrack(cc::PaintCanvas* cc_canvas,
                                       const SkIRect& sk_track_rect,
                                       const Params& params) {
  gfx::Canvas canvas(cc_canvas, 1.f);
  const gfx::Rect track_rect(SkIRectToRect(sk_track_rect));
  constexpr bool is_corner = false;
  PaintTrackGradient(&canvas, track_rect, params, is_corner);
  PaintTrackInnerBorder(&canvas, track_rect, params, is_corner);
  PaintTrackOuterBorder(&canvas, track_rect, params, is_corner);
}

// static
void CocoaScrollbarPainter::PaintCorner(cc::PaintCanvas* cc_canvas,
                                        const SkIRect& sk_corner_rect,
                                        const Params& params) {
  // Overlay scrollbars don't have a corner.
  if (params.overlay)
    return;
  gfx::Canvas canvas(cc_canvas, 1.f);
  const gfx::Rect corner_rect(SkIRectToRect(sk_corner_rect));
  constexpr bool is_corner = true;
  PaintTrackGradient(&canvas, corner_rect, params, is_corner);
  PaintTrackInnerBorder(&canvas, corner_rect, params, is_corner);
  PaintTrackOuterBorder(&canvas, corner_rect, params, is_corner);
}

// static
void CocoaScrollbarPainter::PaintThumb(cc::PaintCanvas* cc_canvas,
                                       const SkIRect& sk_bounds,
                                       const Params& params) {
  gfx::Canvas canvas(cc_canvas, 1.f);

  // Select the color.
  SkColor thumb_color = 0;
  if (params.overlay) {
    if (params.dark_mode)
      thumb_color = kThumbColorOverlayDarkMode;
    else
      thumb_color = kThumbColorOverlay;
  } else {
    if (params.dark_mode) {
      if (params.hovered)
        thumb_color = kThumbColorDarkModeHover;
      else
        thumb_color = kThumbColorDarkMode;
    } else {
      if (params.hovered)
        thumb_color = kThumbColorHover;
      else
        thumb_color = kThumbColorDefault;
    }
  }

  // Compute the bounds for the rounded rect for the thumb from the bounds of
  // the thumb.
  gfx::Rect bounds(SkIRectToRect(sk_bounds));
  {
    // Shrink the thumb evenly in length and girth to fit within the track.
    const int thumb_inset = params.overlay ? kThumbInsetOverlay : kThumbInset;
    int inset_left = thumb_inset;
    int inset_top = thumb_inset;
    int inset_right = thumb_inset;
    int inset_bottom = thumb_inset;

    // Also shrink the thumb in girth to not touch the border.
    if (params.orientation == Orientation::kHorizontal) {
      inset_top += kTrackBorderWidth;
      ConstrainedInset(&bounds, kThumbMinLength, kThumbMinGirth, inset_left,
                       inset_top, inset_right, inset_bottom);
    } else {
      inset_left += kTrackBorderWidth;
      ConstrainedInset(&bounds, kThumbMinGirth, kThumbMinLength, inset_left,
                       inset_top, inset_right, inset_bottom);
    }
  }

  // Draw.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(thumb_color);
  const SkScalar radius = std::min(bounds.width(), bounds.height());
  canvas.DrawRoundRect(bounds, radius, flags);
}

}  // namespace gfx
