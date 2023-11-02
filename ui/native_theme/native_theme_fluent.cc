// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_fluent.h"

#include "base/no_destructor.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/native_theme/native_theme_constants_fluent.h"

namespace ui {

NativeThemeFluent::NativeThemeFluent(bool should_only_use_dark_colors)
    : NativeThemeBase(should_only_use_dark_colors) {
  scrollbar_width_ = kFluentScrollbarThickness;
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
    const gfx::Rect& rect,
    Part direction,
    State state,
    ColorScheme color_scheme,
    const ScrollbarArrowExtraParams& arrow) const {
  PaintButton(canvas, rect, color_scheme);
  PaintArrow(canvas, rect, direction, state, color_scheme);
}

void NativeThemeFluent::PaintScrollbarTrack(
    cc::PaintCanvas* canvas,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect,
    ColorScheme color_scheme) const {
  // TODO(crbug.com/1353574): Implement correct color.
  const SkColor track_color = GetControlColor(kScrollbarTrack, color_scheme);
  cc::PaintFlags flags;
  flags.setColor(track_color);
  canvas->drawIRect(gfx::RectToSkIRect(rect), flags);
}

void NativeThemeFluent::PaintScrollbarThumb(cc::PaintCanvas* canvas,
                                            const ColorProvider* color_provider,
                                            Part part,
                                            State state,
                                            const gfx::Rect& rect,
                                            ScrollbarOverlayColorTheme theme,
                                            ColorScheme color_scheme) const {
  DCHECK_NE(state, NativeTheme::kDisabled);

  cc::PaintCanvasAutoRestore auto_restore(canvas, true);
  SkRRect rrect =
      SkRRect::MakeRectXY(gfx::RectToSkRect(rect), kFluentScrollbarThumbRadius,
                          kFluentScrollbarThumbRadius);

  // Clip the canvas to match the round rect and create round corners.
  SkPath path;
  path.addRRect(rrect);
  canvas->clipPath(path, true);

  // TODO(crbug.com/1353574): Implement correct color.
  const SkColor thumb_color = GetControlColor(kScrollbarThumb, color_scheme);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(thumb_color);
  canvas->drawRect(gfx::RectToSkRect(rect), flags);
}

void NativeThemeFluent::PaintScrollbarCorner(cc::PaintCanvas* canvas,
                                             State state,
                                             const gfx::Rect& rect,
                                             ColorScheme color_scheme) const {
  // TODO(crbug.com/1353574): Implement correct color.
  const SkColor corner_color = GetControlColor(kScrollbarTrack, color_scheme);

  cc::PaintFlags flags;
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

void NativeThemeFluent::PaintButton(cc::PaintCanvas* canvas,
                                    const gfx::Rect& rect,
                                    ColorScheme color_scheme) const {
  // TODO(crbug.com/1353574): Implement correct color.
  // A button color should be always the same as track.
  const SkColor button_color = GetControlColor(kScrollbarTrack, color_scheme);
  cc::PaintFlags flags;
  flags.setColor(button_color);
  canvas->drawIRect(gfx::RectToSkIRect(rect), flags);
}

void NativeThemeFluent::PaintArrow(cc::PaintCanvas* canvas,
                                   const gfx::Rect& rect,
                                   Part part,
                                   State state,
                                   ColorScheme color_scheme) const {
  // TODO(crbug.com/1353574): Implement correct colors based on the state.
  const SkColor arrow_color = GetControlColor(kScrollbarThumb, color_scheme);
  cc::PaintFlags flags;
  flags.setColor(arrow_color);

  // TODO(crbug.com/1353576). Paint arrow icons if the font is available on the
  // device.
  const SkPath path = PathForArrow(GetArrowRect(rect, part, state), part);
  canvas->drawPath(path, flags);
}

gfx::Rect NativeThemeFluent::GetArrowRect(const gfx::Rect& rect,
                                          Part part,
                                          State state) const {
  int min_rect_side, max_rect_side;
  std::tie(min_rect_side, max_rect_side) =
      std::minmax(rect.width(), rect.height());
  const int arrow_side = state == kPressed
                             ? kFluentScrollbarPressedArrowRectFallbackLength
                             : kFluentScrollbarArrowRectLength;

  // Calculates the scaling ratio used to determine the arrow rect side length.
  const float arrow_to_button_side_scale_ratio =
      arrow_side / static_cast<float>(kFluentScrollbarButtonSideLength);
  int side_length =
      base::ClampCeil(max_rect_side * arrow_to_button_side_scale_ratio);

  // Add 1px to the side length if the difference between smaller button rect
  // and arrow side length is odd to keep the arrow rect in the center as well
  // as use int coordinates. This avoids the usage of anti-aliasing.
  side_length += (min_rect_side - side_length) % 2;
  gfx::Rect arrow_rect(
      rect.x() + base::ClampFloor((rect.width() - side_length) / 2.0f),
      rect.y() + base::ClampFloor((rect.height() - side_length) / 2.0f),
      side_length, side_length);

  // The end result is a centered arrow rect within the button rect with the
  // applied offset.
  OffsetArrowRect(arrow_rect, part, max_rect_side);
  return arrow_rect;
}

void NativeThemeFluent::OffsetArrowRect(gfx::Rect& arrow_rect,
                                        Part part,
                                        int max_rect_side) const {
  const int scaled_offset =
      base::ClampRound(kFluentScrollbarArrowOffset * max_rect_side /
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

}  // namespace ui
