// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_fluent.h"

#include "base/no_destructor.h"
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
  PaintButton(canvas, color_provider, rect, color_scheme);
  PaintArrow(canvas, color_provider, rect, direction, state, color_scheme);
}

void NativeThemeFluent::PaintScrollbarTrack(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect,
    ColorScheme color_scheme) const {
  const SkColor track_color = color_provider->GetColor(kColorScrollbarTrack);
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

  const SkColor thumb_color = color_provider->GetColor(kColorScrollbarThumb);
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
    ColorScheme color_scheme) const {
  const SkColor corner_color = color_provider->GetColor(kColorScrollbarTrack);

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
                                    const ColorProvider* color_provider,
                                    const gfx::Rect& rect,
                                    ColorScheme color_scheme) const {
  const SkColor button_color = color_provider->GetColor(kColorScrollbarTrack);
  cc::PaintFlags flags;
  flags.setColor(button_color);
  canvas->drawIRect(gfx::RectToSkIRect(rect), flags);
}

void NativeThemeFluent::PaintArrow(cc::PaintCanvas* canvas,
                                   const ColorProvider* color_provider,
                                   const gfx::Rect& rect,
                                   Part part,
                                   State state,
                                   ColorScheme color_scheme) const {
  const ColorId arrow_color_id =
      state == NativeTheme::kPressed || state == NativeTheme::kHovered
          ? kColorScrollbarArrowForegroundPressed
          : kColorScrollbarArrowForeground;
  const SkColor arrow_color = color_provider->GetColor(arrow_color_id);
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
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace ui
