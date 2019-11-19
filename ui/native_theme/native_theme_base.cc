// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_base.h"

#include <limits>
#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/common_theme.h"

namespace {

// These are the default dimensions of radio buttons and checkboxes.
const int kCheckboxAndRadioWidth = 13;
const int kCheckboxAndRadioHeight = 13;

// These sizes match the sizes in Chromium Win.
const int kSliderThumbWidth = 11;
const int kSliderThumbHeight = 21;

const int kDefaultScrollbarWidth = 15;
const int kDefaultScrollbarButtonLength = 14;

// Color constant pairs for light/default and dark color-schemes below.
constexpr SkColor kThumbActiveColor[2] = {SkColorSetRGB(0xF4, 0xF4, 0xF4),
                                          gfx::kPlaceholderColor};
constexpr SkColor kThumbInactiveColor[2] = {SkColorSetRGB(0xEA, 0xEA, 0xEA),
                                            gfx::kPlaceholderColor};
constexpr SkColor kTrackColor[2] = {SkColorSetRGB(0xD3, 0xD3, 0xD3),
                                    gfx::kPlaceholderColor};
constexpr SkColor kSliderTrackBackgroundColor[2] = {
    SkColorSetRGB(0xE3, 0xDD, 0xD8), SkColorSetRGB(0x44, 0x44, 0x44)};
constexpr SkColor kSliderThumbBrightColor[2] = {
    SkColorSetRGB(0xF4, 0xF2, 0xEF), SkColorSetRGB(0xD0, 0xD0, 0xD0)};
constexpr SkColor kSliderThumbShadedColor[2] = {
    SkColorSetRGB(0xEA, 0xE5, 0xE0), SkColorSetRGB(0xC4, 0xC4, 0xC4)};
constexpr SkColor kSliderThumbHoveredBrightColor[2] = {
    SK_ColorWHITE, SkColorSetRGB(0xDD, 0xDD, 0xDD)};
constexpr SkColor kSliderThumbHoveredShadedColor[2] = {
    SkColorSetRGB(0xF4, 0xF2, 0xEF), SkColorSetRGB(0xD0, 0xD0, 0xD0)};
constexpr SkColor kSliderThumbBorder[2] = {SkColorSetRGB(0x9D, 0x96, 0x8E),
                                           SkColorSetRGB(0x63, 0x6C, 0x72)};
constexpr SkColor kTextBorderColor[2] = {SkColorSetRGB(0xA9, 0xA9, 0xA9),
                                         SkColorSetRGB(0x60, 0x60, 0x60)};
constexpr SkColor kProgressBorderColor[2] = {SkColorSetRGB(0xA9, 0xA9, 0xA9),
                                             SkColorSetRGB(0x60, 0x60, 0x60)};
constexpr SkColor kProgressTickColor[2] = {SkColorSetRGB(0xED, 0xED, 0xED),
                                           SkColorSetRGB(0x20, 0x20, 0x20)};
constexpr SkColor kProgressValueColor[2] = {gfx::kGoogleBlue300,
                                            gfx::kGoogleBlue700};
// We are currently only painting kMenuPopupBackground with the kDefault
// scheme. If that changes, we need to replace gfx::kPlaceholderColor with an
// appropriate dark scheme color. See the DCHECK in PaintMenuPopupBackground().
constexpr SkColor kMenuPopupBackgroundColor[2] = {SkColorSetRGB(210, 225, 246),
                                                  gfx::kPlaceholderColor};
constexpr SkColor kCheckboxTinyColor[2] = {SK_ColorGRAY, SK_ColorDKGRAY};
constexpr SkColor kCheckboxShadowColor[2] = {SkColorSetA(SK_ColorBLACK, 0x15),
                                             SkColorSetA(SK_ColorWHITE, 0x15)};
constexpr SkColor kCheckboxShadowHoveredColor[2] = {
    SkColorSetA(SK_ColorBLACK, 0x1F), SkColorSetA(SK_ColorWHITE, 0x1F)};
constexpr SkColor kCheckboxShadowDisabledColor[2] = {
    SK_ColorTRANSPARENT, SkColorSetA(SK_ColorWHITE, 0x1F)};
constexpr SkColor kCheckboxGradientStartColor[2] = {
    SkColorSetRGB(0xED, 0xED, 0xED), SkColorSetRGB(0x13, 0x13, 0x13)};
constexpr SkColor kCheckboxGradientEndColor[2] = {
    SkColorSetRGB(0xDE, 0xDE, 0xDE), SkColorSetRGB(0x20, 0x20, 0x20)};
constexpr U8CPU kCheckboxDisabledGradientAlpha = 0x80;
constexpr SkColor kCheckboxPressedGradientStartColor[2] = {
    SkColorSetRGB(0xE7, 0xE7, 0xE7), SkColorSetRGB(0x19, 0x19, 0x19)};
constexpr SkColor kCheckboxPressedGradientEndColor[2] = {
    SkColorSetRGB(0xD7, 0xD7, 0xD7), SkColorSetRGB(0x27, 0x27, 0x27)};
const SkColor kCheckboxHoveredGradientStartColor[2] = {
    SkColorSetRGB(0xF0, 0xF0, 0xF0), SkColorSetRGB(0x16, 0x16, 0x16)};
const SkColor kCheckboxHoveredGradientEndColor[2] = {
    SkColorSetRGB(0xE0, 0xE0, 0xE0), SkColorSetRGB(0x20, 0x20, 0x20)};
constexpr SkColor kCheckboxBorderColor[2] = {SkColorSetA(SK_ColorBLACK, 0x40),
                                             SkColorSetA(SK_ColorWHITE, 0x40)};
constexpr SkColor kCheckboxBorderHoveredColor[2] = {
    SkColorSetA(SK_ColorBLACK, 0x4D), SkColorSetA(SK_ColorWHITE, 0x4D)};
constexpr SkColor kCheckboxBorderDisabledColor[2] = {
    SkColorSetA(SK_ColorBLACK, 0x20), SkColorSetA(SK_ColorWHITE, 0x20)};
constexpr SkColor kCheckboxStrokeColor[2] = {SkColorSetA(SK_ColorBLACK, 0xB3),
                                             SkColorSetA(SK_ColorWHITE, 0xB3)};
constexpr SkColor kCheckboxStrokeDisabledColor[2] = {
    SkColorSetA(SK_ColorBLACK, 0x59), SkColorSetA(SK_ColorWHITE, 0x59)};
constexpr SkColor kRadioDotColor[2] = {SkColorSetRGB(0x66, 0x66, 0x66),
                                       SkColorSetRGB(0xDD, 0xDD, 0xDD)};
constexpr SkColor kRadioDotDisabledColor[2] = {
    SkColorSetARGB(0x80, 0x66, 0x66, 0x66),
    SkColorSetARGB(0x80, 0xDD, 0xDD, 0xDD)};
constexpr SkColor kArrowDisabledColor[2] = {SK_ColorBLACK, SK_ColorWHITE};
constexpr SkColor kButtonBorderColor[2] = {SK_ColorBLACK, SK_ColorWHITE};
constexpr SkColor kProgressBackgroundColor[2] = {SK_ColorWHITE, SK_ColorBLACK};

const int kCheckboxBorderRadius = 2;
// The "dash" is 8x2 px by default (the checkbox is 13x13 px).
const SkScalar kIndeterminateInsetWidthRatio = (13 - 8) / 2.0f / 13;
const SkScalar kIndeterminateInsetHeightRatio = (13 - 2) / 2.0f / 13;
const SkScalar kBorderWidth = 1.f;
const SkScalar kSliderTrackHeight = 8.f;
const SkScalar kSliderTrackBorderRadius = 40.f;
const SkScalar kSliderThumbBorderWidth = 1.f;
const SkScalar kSliderThumbBorderHoveredWidth = 1.f;
// Default height for progress is 16px and the track is 8px.
const SkScalar kTrackHeightRatio = 8.0f / 16;
const SkScalar kMenuListArrowStrokeWidth = 2.f;
const int kSliderThumbSize = 16;
const int kInputBorderRadius = 2;

// Get a color constant based on color-scheme
SkColor GetColor(const SkColor colors[2],
                 ui::NativeTheme::ColorScheme color_scheme) {
  return colors[color_scheme == ui::NativeTheme::ColorScheme::kDark ? 1 : 0];
}

// Get lightness adjusted color.
SkColor BrightenColor(const color_utils::HSL& hsl, SkAlpha alpha,
    double lightness_amount) {
  color_utils::HSL adjusted = hsl;
  adjusted.l += lightness_amount;
  if (adjusted.l > 1.0)
    adjusted.l = 1.0;
  if (adjusted.l < 0.0)
    adjusted.l = 0.0;

  return color_utils::HSLToSkColor(adjusted, alpha);
}

}  // namespace

namespace ui {

gfx::Size NativeThemeBase::GetPartSize(Part part,
                                       State state,
                                       const ExtraParams& extra) const {
  switch (part) {
    // Please keep these in the order of NativeTheme::Part.
    case kCheckbox:
      return gfx::Size(kCheckboxAndRadioWidth, kCheckboxAndRadioHeight);
    case kInnerSpinButton:
      return gfx::Size(scrollbar_width_, 0);
    case kMenuList:
      return gfx::Size();  // No default size.
    case kMenuPopupBackground:
      return gfx::Size();  // No default size.
    case kMenuItemBackground:
    case kProgressBar:
    case kPushButton:
      return gfx::Size();  // No default size.
    case kRadio:
      return gfx::Size(kCheckboxAndRadioWidth, kCheckboxAndRadioHeight);
    case kScrollbarDownArrow:
    case kScrollbarUpArrow:
      return gfx::Size(scrollbar_width_, scrollbar_button_length_);
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
      return gfx::Size(scrollbar_button_length_, scrollbar_width_);
    case kScrollbarHorizontalThumb:
      // This matches Firefox on Linux.
      return gfx::Size(2 * scrollbar_width_, scrollbar_width_);
    case kScrollbarVerticalThumb:
      // This matches Firefox on Linux.
      return gfx::Size(scrollbar_width_, 2 * scrollbar_width_);
    case kScrollbarHorizontalTrack:
      return gfx::Size(0, scrollbar_width_);
    case kScrollbarVerticalTrack:
      return gfx::Size(scrollbar_width_, 0);
    case kScrollbarHorizontalGripper:
    case kScrollbarVerticalGripper:
      NOTIMPLEMENTED();
      break;
    case kSliderTrack:
      return gfx::Size();  // No default size.
    case kSliderThumb:
      // These sizes match the sizes in Chromium Win.
      if (features::IsFormControlsRefreshEnabled())
        return gfx::Size(kSliderThumbSize, kSliderThumbSize);
      return gfx::Size(kSliderThumbWidth, kSliderThumbHeight);
    case kTabPanelBackground:
      NOTIMPLEMENTED();
      break;
    case kTextField:
      return gfx::Size();  // No default size.
    case kTrackbarThumb:
    case kTrackbarTrack:
    case kWindowResizeGripper:
      NOTIMPLEMENTED();
      break;
    default:
      NOTREACHED() << "Unknown theme part: " << part;
      break;
  }
  return gfx::Size();
}

void NativeThemeBase::Paint(cc::PaintCanvas* canvas,
                            Part part,
                            State state,
                            const gfx::Rect& rect,
                            const ExtraParams& extra,
                            ColorScheme color_scheme) const {
  if (rect.IsEmpty())
    return;

  canvas->save();
  canvas->clipRect(gfx::RectToSkRect(rect));

  switch (part) {
    // Please keep these in the order of NativeTheme::Part.
    case kCheckbox:
      PaintCheckbox(canvas, state, rect, extra.button, color_scheme);
      break;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    case kFrameTopArea:
      PaintFrameTopArea(canvas, state, rect, extra.frame_top_area,
                        color_scheme);
      break;
#endif
    case kInnerSpinButton:
      PaintInnerSpinButton(canvas, state, rect, extra.inner_spin, color_scheme);
      break;
    case kMenuList:
      PaintMenuList(canvas, state, rect, extra.menu_list, color_scheme);
      break;
    case kMenuPopupBackground:
      PaintMenuPopupBackground(canvas, rect.size(), extra.menu_background,
                               color_scheme);
      break;
    case kMenuPopupSeparator:
      PaintMenuSeparator(canvas, state, rect, extra.menu_separator,
                         color_scheme);
      break;
    case kMenuItemBackground:
      PaintMenuItemBackground(canvas, state, rect, extra.menu_item,
                              color_scheme);
      break;
    case kProgressBar:
      PaintProgressBar(canvas, state, rect, extra.progress_bar, color_scheme);
      break;
    case kPushButton:
      PaintButton(canvas, state, rect, extra.button, color_scheme);
      break;
    case kRadio:
      PaintRadio(canvas, state, rect, extra.button, color_scheme);
      break;
    case kScrollbarDownArrow:
    case kScrollbarUpArrow:
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
      if (scrollbar_button_length_ > 0)
        PaintArrowButton(canvas, rect, part, state, color_scheme,
                         extra.scrollbar_arrow);
      break;
    case kScrollbarHorizontalThumb:
    case kScrollbarVerticalThumb:
      PaintScrollbarThumb(canvas, part, state, rect,
                          extra.scrollbar_thumb.scrollbar_theme, color_scheme);
      break;
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack:
      PaintScrollbarTrack(canvas, part, state, extra.scrollbar_track, rect,
                          color_scheme);
      break;
    case kScrollbarHorizontalGripper:
    case kScrollbarVerticalGripper:
      // Invoked by views scrollbar code, don't care about for non-win
      // implementations, so no NOTIMPLEMENTED.
      break;
    case kScrollbarCorner:
      PaintScrollbarCorner(canvas, state, rect, color_scheme);
      break;
    case kSliderTrack:
      PaintSliderTrack(canvas, state, rect, extra.slider, color_scheme);
      break;
    case kSliderThumb:
      PaintSliderThumb(canvas, state, rect, extra.slider, color_scheme);
      break;
    case kTabPanelBackground:
      NOTIMPLEMENTED();
      break;
    case kTextField:
      PaintTextField(canvas, state, rect, extra.text_field, color_scheme);
      break;
    case kTrackbarThumb:
    case kTrackbarTrack:
    case kWindowResizeGripper:
      NOTIMPLEMENTED();
      break;
    default:
      NOTREACHED() << "Unknown theme part: " << part;
      break;
  }

  canvas->restore();
}

bool NativeThemeBase::SupportsNinePatch(Part part) const {
  return false;
}

gfx::Size NativeThemeBase::GetNinePatchCanvasSize(Part part) const {
  NOTREACHED() << "NativeThemeBase doesn't support nine-patch resources.";
  return gfx::Size();
}

gfx::Rect NativeThemeBase::GetNinePatchAperture(Part part) const {
  NOTREACHED() << "NativeThemeBase doesn't support nine-patch resources.";
  return gfx::Rect();
}

NativeThemeBase::NativeThemeBase()
    : scrollbar_width_(kDefaultScrollbarWidth),
      scrollbar_button_length_(kDefaultScrollbarButtonLength) {
}

NativeThemeBase::~NativeThemeBase() {
}

void NativeThemeBase::PaintArrowButton(
    cc::PaintCanvas* canvas,
    const gfx::Rect& rect,
    Part direction,
    State state,
    ColorScheme color_scheme,
    const ScrollbarArrowExtraParams& arrow) const {
  cc::PaintFlags flags;

  // Calculate button color.
  SkScalar track_hsv[3];
  SkColorToHSV(GetColor(kTrackColor, color_scheme), track_hsv);
  SkColor button_color = SaturateAndBrighten(track_hsv, 0, 0.2f);
  SkColor background_color = button_color;
  if (state == kPressed) {
    SkScalar button_hsv[3];
    SkColorToHSV(button_color, button_hsv);
    button_color = SaturateAndBrighten(button_hsv, 0, -0.1f);
  } else if (state == kHovered) {
    SkScalar button_hsv[3];
    SkColorToHSV(button_color, button_hsv);
    button_color = SaturateAndBrighten(button_hsv, 0, 0.05f);
  }

  SkIRect skrect;
  skrect.setXYWH(rect.x(), rect.y(), rect.width(), rect.height());
  // Paint the background (the area visible behind the rounded corners).
  flags.setColor(background_color);
  canvas->drawIRect(skrect, flags);

  // Paint the button's outline and fill the middle
  SkPath outline;
  switch (direction) {
    case kScrollbarUpArrow:
      outline.moveTo(rect.x() + 0.5, rect.y() + rect.height() + 0.5);
      outline.rLineTo(0, -(rect.height() - 2));
      outline.rLineTo(2, -2);
      outline.rLineTo(rect.width() - 5, 0);
      outline.rLineTo(2, 2);
      outline.rLineTo(0, rect.height() - 2);
      break;
    case kScrollbarDownArrow:
      outline.moveTo(rect.x() + 0.5, rect.y() - 0.5);
      outline.rLineTo(0, rect.height() - 2);
      outline.rLineTo(2, 2);
      outline.rLineTo(rect.width() - 5, 0);
      outline.rLineTo(2, -2);
      outline.rLineTo(0, -(rect.height() - 2));
      break;
    case kScrollbarRightArrow:
      outline.moveTo(rect.x() - 0.5, rect.y() + 0.5);
      outline.rLineTo(rect.width() - 2, 0);
      outline.rLineTo(2, 2);
      outline.rLineTo(0, rect.height() - 5);
      outline.rLineTo(-2, 2);
      outline.rLineTo(-(rect.width() - 2), 0);
      break;
    case kScrollbarLeftArrow:
      outline.moveTo(rect.x() + rect.width() + 0.5, rect.y() + 0.5);
      outline.rLineTo(-(rect.width() - 2), 0);
      outline.rLineTo(-2, 2);
      outline.rLineTo(0, rect.height() - 5);
      outline.rLineTo(2, 2);
      outline.rLineTo(rect.width() - 2, 0);
      break;
    default:
      break;
  }
  outline.close();

  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(button_color);
  canvas->drawPath(outline, flags);

  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  SkScalar thumb_hsv[3];
  SkColorToHSV(GetColor(kThumbInactiveColor, color_scheme), thumb_hsv);
  flags.setColor(OutlineColor(track_hsv, thumb_hsv));
  canvas->drawPath(outline, flags);

  PaintArrow(canvas, rect, direction, GetArrowColor(state, color_scheme));
}

void NativeThemeBase::PaintArrow(cc::PaintCanvas* gc,
                                 const gfx::Rect& rect,
                                 Part direction,
                                 SkColor color) const {
  cc::PaintFlags flags;
  flags.setColor(color);

  SkPath path = PathForArrow(rect, direction);

  gc->drawPath(path, flags);
}

SkPath NativeThemeBase::PathForArrow(const gfx::Rect& rect,
                                     Part direction) const {
  gfx::Rect bounding_rect = BoundingRectForArrow(rect);
  const gfx::PointF center = gfx::RectF(bounding_rect).CenterPoint();
  SkPath path;
  SkMatrix transform;
  transform.setIdentity();
  if (direction == kScrollbarUpArrow || direction == kScrollbarDownArrow) {
    int arrow_altitude = bounding_rect.height() / 2 + 1;
    path.moveTo(bounding_rect.x(), bounding_rect.bottom());
    path.rLineTo(bounding_rect.width(), 0);
    path.rLineTo(-bounding_rect.width() / 2.0f, -arrow_altitude);
    path.close();
    path.offset(0, -arrow_altitude / 2 + 1);
    if (direction == kScrollbarDownArrow) {
      transform.setScale(1, -1, center.x(), center.y());
    }
  } else {
    int arrow_altitude = bounding_rect.width() / 2 + 1;
    path.moveTo(bounding_rect.x(), bounding_rect.y());
    path.rLineTo(0, bounding_rect.height());
    path.rLineTo(arrow_altitude, -bounding_rect.height() / 2.0f);
    path.close();
    path.offset(arrow_altitude / 2, 0);
    if (direction == kScrollbarLeftArrow) {
      transform.setScale(-1, 1, center.x(), center.y());
    }
  }
  path.transform(transform);

  return path;
}

gfx::Rect NativeThemeBase::BoundingRectForArrow(const gfx::Rect& rect) const {
  const std::pair<int, int> rect_sides =
      std::minmax(rect.width(), rect.height());
  const int side_length_inset = 2 * std::ceil(rect_sides.second / 4.f);
  const int side_length =
      std::min(rect_sides.first, rect_sides.second - side_length_inset);
  // When there are an odd number of pixels, put the extra on the top/left.
  return gfx::Rect(rect.x() + (rect.width() - side_length + 1) / 2,
                   rect.y() + (rect.height() - side_length + 1) / 2,
                   side_length, side_length);
}

void NativeThemeBase::PaintScrollbarTrack(
    cc::PaintCanvas* canvas,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect,
    ColorScheme color_scheme) const {
  cc::PaintFlags flags;
  SkIRect skrect;

  skrect.setLTRB(rect.x(), rect.y(), rect.right(), rect.bottom());
  SkScalar track_hsv[3];
  SkColorToHSV(GetColor(kTrackColor, color_scheme), track_hsv);
  flags.setColor(SaturateAndBrighten(track_hsv, 0, 0));
  canvas->drawIRect(skrect, flags);

  SkScalar thumb_hsv[3];
  SkColorToHSV(GetColor(kThumbInactiveColor, color_scheme), thumb_hsv);

  flags.setColor(OutlineColor(track_hsv, thumb_hsv));
  DrawBox(canvas, rect, flags);
}

void NativeThemeBase::PaintScrollbarThumb(cc::PaintCanvas* canvas,
                                          Part part,
                                          State state,
                                          const gfx::Rect& rect,
                                          ScrollbarOverlayColorTheme,
                                          ColorScheme color_scheme) const {
  const bool hovered = state == kHovered;
  const int midx = rect.x() + rect.width() / 2;
  const int midy = rect.y() + rect.height() / 2;
  const bool vertical = part == kScrollbarVerticalThumb;

  SkScalar thumb[3];
  SkColorToHSV(
      GetColor(hovered ? kThumbActiveColor : kThumbInactiveColor, color_scheme),
      thumb);

  cc::PaintFlags flags;
  flags.setColor(SaturateAndBrighten(thumb, 0, 0.02f));

  SkIRect skrect;
  if (vertical)
    skrect.setLTRB(rect.x(), rect.y(), midx + 1, rect.y() + rect.height());
  else
    skrect.setLTRB(rect.x(), rect.y(), rect.x() + rect.width(), midy + 1);

  canvas->drawIRect(skrect, flags);

  flags.setColor(SaturateAndBrighten(thumb, 0, -0.02f));

  if (vertical) {
    skrect.setLTRB(midx + 1, rect.y(), rect.x() + rect.width(),
                   rect.y() + rect.height());
  } else {
    skrect.setLTRB(rect.x(), midy + 1, rect.x() + rect.width(),
                   rect.y() + rect.height());
  }

  canvas->drawIRect(skrect, flags);

  SkScalar track[3];
  SkColorToHSV(GetColor(kTrackColor, color_scheme), track);
  flags.setColor(OutlineColor(track, thumb));
  DrawBox(canvas, rect, flags);

  if (rect.height() > 10 && rect.width() > 10) {
    const int grippy_half_width = 2;
    const int inter_grippy_offset = 3;
    if (vertical) {
      DrawHorizLine(canvas, midx - grippy_half_width, midx + grippy_half_width,
                    midy - inter_grippy_offset, flags);
      DrawHorizLine(canvas, midx - grippy_half_width, midx + grippy_half_width,
                    midy, flags);
      DrawHorizLine(canvas, midx - grippy_half_width, midx + grippy_half_width,
                    midy + inter_grippy_offset, flags);
    } else {
      DrawVertLine(canvas, midx - inter_grippy_offset, midy - grippy_half_width,
                   midy + grippy_half_width, flags);
      DrawVertLine(canvas, midx, midy - grippy_half_width,
                   midy + grippy_half_width, flags);
      DrawVertLine(canvas, midx + inter_grippy_offset, midy - grippy_half_width,
                   midy + grippy_half_width, flags);
    }
  }
}

void NativeThemeBase::PaintScrollbarCorner(cc::PaintCanvas* canvas,
                                           State state,
                                           const gfx::Rect& rect,
                                           ColorScheme color_scheme) const {}

void NativeThemeBase::PaintCheckbox(cc::PaintCanvas* canvas,
                                    State state,
                                    const gfx::Rect& rect,
                                    const ButtonExtraParams& button,
                                    ColorScheme color_scheme) const {
  if (features::IsFormControlsRefreshEnabled()) {
    const float border_radius =
        SkIntToScalar(kCheckboxBorderRadius) * button.zoom;
    SkRect skrect = PaintCheckboxRadioCommon(canvas, state, rect, button, true,
                                             border_radius, color_scheme);

    if (!skrect.isEmpty()) {
      cc::PaintFlags flags;
      flags.setAntiAlias(true);

      if (button.indeterminate) {
        // Draw the dash.
        flags.setColor(ControlsBorderColorForState(state, color_scheme));
        const auto indeterminate =
            skrect.makeInset(skrect.width() * kIndeterminateInsetWidthRatio,
                             skrect.height() * kIndeterminateInsetHeightRatio);
        flags.setStyle(cc::PaintFlags::kFill_Style);
        canvas->drawRoundRect(indeterminate, border_radius, border_radius,
                              flags);
      } else if (button.checked) {
        // Draw the accent background.
        flags.setStyle(cc::PaintFlags::kFill_Style);
        flags.setColor(ControlsAccentColorForState(state, color_scheme));
        canvas->drawRoundRect(skrect, border_radius, border_radius, flags);

        // Draw the checkmark.
        SkPath check;
        check.moveTo(skrect.x() + skrect.width() * 0.2, skrect.centerY());
        check.rLineTo(skrect.width() * 0.2, skrect.height() * 0.2);
        check.lineTo(skrect.right() - skrect.width() * 0.2,
                     skrect.y() + skrect.height() * 0.2);
        flags.setStyle(cc::PaintFlags::kStroke_Style);
        flags.setStrokeWidth(SkFloatToScalar(skrect.height() * 0.16));
        SkColor checkmark_color =
            ControlsBackgroundColorForState(state, color_scheme);
        flags.setColor(checkmark_color);
        canvas->drawPath(check, flags);
      }
    }
    return;
  }

  SkRect skrect = PaintCheckboxRadioCommon(canvas, state, rect, button, true,
                                           SkIntToScalar(2), color_scheme);
  if (!skrect.isEmpty()) {
    // Draw the checkmark / dash.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setColor(GetColor(state == kDisabled ? kCheckboxStrokeDisabledColor
                                               : kCheckboxStrokeColor,
                            color_scheme));
    if (button.indeterminate) {
      SkPath dash;
      dash.moveTo(skrect.x() + skrect.width() * 0.16,
                  (skrect.y() + skrect.bottom()) / 2);
      dash.rLineTo(skrect.width() * 0.68, 0);
      flags.setStrokeWidth(SkFloatToScalar(skrect.height() * 0.2));
      canvas->drawPath(dash, flags);
    } else if (button.checked) {
      SkPath check;
      check.moveTo(skrect.x() + skrect.width() * 0.2,
                   skrect.y() + skrect.height() * 0.5);
      check.rLineTo(skrect.width() * 0.2, skrect.height() * 0.2);
      flags.setStrokeWidth(SkFloatToScalar(skrect.height() * 0.23));
      check.lineTo(skrect.right() - skrect.width() * 0.2,
                   skrect.y() + skrect.height() * 0.2);
      canvas->drawPath(check, flags);
    }
  }
}

// Draws the common elements of checkboxes and radio buttons.
// Returns the rectangle within which any additional decorations should be
// drawn, or empty if none.
SkRect NativeThemeBase::PaintCheckboxRadioCommon(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const ButtonExtraParams& button,
    bool is_checkbox,
    const SkScalar border_radius,
    ColorScheme color_scheme) const {
  if (features::IsFormControlsRefreshEnabled()) {
    SkRect skrect = gfx::RectToSkRect(rect);

    // Use the largest square that fits inside the provided rectangle.
    // No other browser seems to support non-square widget, so accidentally
    // having non-square sizes is common (eg. amazon and webkit dev tools).
    if (skrect.width() != skrect.height()) {
      SkScalar size = SkMinScalar(skrect.width(), skrect.height());
      skrect.inset((skrect.width() - size) / 2, (skrect.height() - size) / 2);
    }

    // If the rectangle is too small then paint only a rectangle. We don't want
    // to have to worry about '- 1' and '+ 1' calculations below having overflow
    // or underflow.
    if (skrect.width() <= 2) {
      cc::PaintFlags flags;
      flags.setColor(GetControlColor(kBorder, color_scheme));
      flags.setStyle(cc::PaintFlags::kFill_Style);
      canvas->drawRect(skrect, flags);
      // Too small to draw anything more.
      return SkRect::MakeEmpty();
    }

    cc::PaintFlags flags;
    flags.setAntiAlias(true);

    // Paint the background (is not visible behind the rounded corners).
    // Note we need to shrink the rect for background a little bit so we don't
    // see artifacts introduced by antialiasing between the border and the
    // background near the rounded corners of checkbox.
    const auto background_rect =
        skrect.makeInset(kBorderWidth * 0.2f, kBorderWidth * 0.2f);
    PaintLightenLayer(canvas, background_rect, state, border_radius,
                      color_scheme);
    flags.setColor(ControlsBackgroundColorForState(state, color_scheme));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->drawRoundRect(background_rect, border_radius, border_radius, flags);

    // Draw the border.
    if (!(is_checkbox && button.checked)) {
      // Shrink half border width so the final pixels of the border will be
      // within the rectangle.
      const auto border_rect =
          skrect.makeInset(kBorderWidth / 2, kBorderWidth / 2);
      SkColor border_color =
          button.checked ? ControlsAccentColorForState(state, color_scheme)
                         : ControlsBorderColorForState(state, color_scheme);
      flags.setColor(border_color);
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(kBorderWidth);
      canvas->drawRoundRect(border_rect, border_radius, border_radius, flags);
    }

    // Return the rectangle for drawing any additional decorations.
    return skrect;
  }

  SkRect skrect = gfx::RectToSkRect(rect);

  // Use the largest square that fits inside the provided rectangle.
  // No other browser seems to support non-square widget, so accidentally
  // having non-square sizes is common (eg. amazon and webkit dev tools).
  if (skrect.width() != skrect.height()) {
    SkScalar size = SkMinScalar(skrect.width(), skrect.height());
    skrect.inset((skrect.width() - size) / 2, (skrect.height() - size) / 2);
  }

  // If the rectangle is too small then paint only a rectangle.  We don't want
  // to have to worry about '- 1' and '+ 1' calculations below having overflow
  // or underflow.
  if (skrect.width() <= 2) {
    cc::PaintFlags flags;
    flags.setColor(GetColor(kCheckboxTinyColor, color_scheme));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->drawRect(skrect, flags);
    // Too small to draw anything more.
    return SkRect::MakeEmpty();
  }

  // Make room for padding/drop shadow.
  AdjustCheckboxRadioRectForPadding(&skrect);

  // Draw the drop shadow below the widget.
  if (state != kPressed) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    SkRect shadow_rect = skrect;
    shadow_rect.offset(0, 1);
    if (state == kDisabled)
      flags.setColor(GetColor(kCheckboxShadowDisabledColor, color_scheme));
    else if (state == kHovered)
      flags.setColor(GetColor(kCheckboxShadowHoveredColor, color_scheme));
    else
      flags.setColor(GetColor(kCheckboxShadowColor, color_scheme));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->drawRoundRect(shadow_rect, border_radius, border_radius, flags);
  }

  // Draw the gradient-filled rectangle
  SkPoint gradient_bounds[3];
  gradient_bounds[0].set(skrect.x(), skrect.y());
  gradient_bounds[1].set(skrect.x(), skrect.y() + skrect.height() * 0.38);
  gradient_bounds[2].set(skrect.x(), skrect.bottom());
  SkColor start_color;
  SkColor end_color;
  if (state == kPressed) {
    start_color = GetColor(kCheckboxPressedGradientStartColor, color_scheme);
    end_color = GetColor(kCheckboxPressedGradientEndColor, color_scheme);
  } else if (state == kHovered) {
    start_color = GetColor(kCheckboxHoveredGradientStartColor, color_scheme);
    end_color = GetColor(kCheckboxHoveredGradientEndColor, color_scheme);
  } else /* kNormal or kDisabled */ {
    start_color = GetColor(kCheckboxGradientStartColor, color_scheme);
    end_color = GetColor(kCheckboxGradientEndColor, color_scheme);
    if (state == kDisabled) {
      start_color = SkColorSetA(start_color, kCheckboxDisabledGradientAlpha);
      end_color = SkColorSetA(end_color, kCheckboxDisabledGradientAlpha);
    }
  }
  SkColor colors[3] = {start_color, start_color, end_color};
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setShader(cc::PaintShader::MakeLinearGradient(
      gradient_bounds, colors, nullptr, 3, SkTileMode::kClamp));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->drawRoundRect(skrect, border_radius, border_radius, flags);
  flags.setShader(nullptr);

  // Draw the border.
  if (state == kHovered)
    flags.setColor(GetColor(kCheckboxBorderHoveredColor, color_scheme));
  else if (state == kDisabled)
    flags.setColor(GetColor(kCheckboxBorderDisabledColor, color_scheme));
  else
    flags.setColor(GetColor(kCheckboxBorderColor, color_scheme));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(SkIntToScalar(1));
  skrect.inset(SkFloatToScalar(.5f), SkFloatToScalar(.5f));
  canvas->drawRoundRect(skrect, border_radius, border_radius, flags);

  // Return the rectangle excluding the drop shadow for drawing any additional
  // decorations.
  return skrect;
}

void NativeThemeBase::PaintRadio(cc::PaintCanvas* canvas,
                                 State state,
                                 const gfx::Rect& rect,
                                 const ButtonExtraParams& button,
                                 ColorScheme color_scheme) const {
  if (features::IsFormControlsRefreshEnabled()) {
    // Most of a radio button is the same as a checkbox, except the the rounded
    // square is a circle (i.e. border radius >= 100%).
    const SkScalar radius = SkFloatToScalar(
        static_cast<float>(std::max(rect.width(), rect.height())) * 0.5);

    SkRect skrect = PaintCheckboxRadioCommon(canvas, state, rect, button, false,
                                             radius, color_scheme);
    if (!skrect.isEmpty() && button.checked) {
      // Draw the dot.
      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      flags.setStyle(cc::PaintFlags::kFill_Style);
      flags.setColor(ControlsAccentColorForState(state, color_scheme));

      skrect.inset(skrect.width() * 0.2, skrect.height() * 0.2);
      // Use drawRoundedRect instead of drawOval to be completely consistent
      // with the border in PaintCheckboxRadioNewCommon.
      canvas->drawRoundRect(skrect, radius, radius, flags);
    }
    return;
  }

  // Most of a radio button is the same as a checkbox, except the the rounded
  // square is a circle (i.e. border radius >= 100%).
  const SkScalar radius = SkFloatToScalar(
      static_cast<float>(std::max(rect.width(), rect.height())) / 2);
  SkRect skrect = PaintCheckboxRadioCommon(canvas, state, rect, button, false,
                                           radius, color_scheme);
  if (!skrect.isEmpty() && button.checked) {
    // Draw the dot.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(
        GetColor(state == kDisabled ? kRadioDotDisabledColor : kRadioDotColor,
                 color_scheme));
    skrect.inset(skrect.width() * 0.25, skrect.height() * 0.25);
    // Use drawRoundedRect instead of drawOval to be completely consistent
    // with the border in PaintCheckboxRadioNewCommon.
    canvas->drawRoundRect(skrect, radius, radius, flags);
  }
}

void NativeThemeBase::PaintButton(cc::PaintCanvas* canvas,
                                  State state,
                                  const gfx::Rect& rect,
                                  const ButtonExtraParams& button,
                                  ColorScheme color_scheme) const {
  if (features::IsFormControlsRefreshEnabled()) {
    cc::PaintFlags flags;
    SkRect skrect = gfx::RectToSkRect(rect);

    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);

    // If the button is too small, fallback to drawing a single, solid color.
    if (rect.width() < 5 || rect.height() < 5) {
      flags.setColor(ControlsFillColorForState(state, color_scheme));
      canvas->drawRect(skrect, flags);
      return;
    }

    // Paint the background (is not visible behind the rounded corners).
    skrect.inset(kBorderWidth / 2, kBorderWidth / 2);
    PaintLightenLayer(canvas, skrect, state, kInputBorderRadius, color_scheme);
    flags.setColor(ControlsFillColorForState(state, color_scheme));
    canvas->drawRoundRect(skrect, kInputBorderRadius, kInputBorderRadius,
                          flags);

    // Paint the border: 1px solid.
    if (button.has_border) {
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(kBorderWidth);
      flags.setColor(ControlsBorderColorForState(state, color_scheme));
      canvas->drawRoundRect(skrect, kInputBorderRadius, kInputBorderRadius,
                            flags);
    }
    return;
  }

  cc::PaintFlags flags;
  SkRect skrect = gfx::RectToSkRect(rect);
  SkColor base_color = button.background_color;

  color_utils::HSL base_hsl;
  color_utils::SkColorToHSL(base_color, &base_hsl);

  // Our standard gradient is from 0xDD to 0xF8. This is the amount of
  // increased luminance between those values.
  SkColor light_color(BrightenColor(base_hsl, SkColorGetA(base_color), 0.105));

  // If the button is too small, fallback to drawing a single, solid color
  if (rect.width() < 5 || rect.height() < 5) {
    flags.setColor(base_color);
    canvas->drawRect(skrect, flags);
    return;
  }

  flags.setColor(GetColor(kButtonBorderColor, color_scheme));
  SkPoint gradient_bounds[2] = {
    gfx::PointToSkPoint(rect.origin()),
    gfx::PointToSkPoint(rect.bottom_left() - gfx::Vector2d(0, 1))
  };
  if (state == kPressed)
    std::swap(gradient_bounds[0], gradient_bounds[1]);
  SkColor colors[2] = { light_color, base_color };

  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setShader(cc::PaintShader::MakeLinearGradient(
      gradient_bounds, colors, nullptr, 2, SkTileMode::kClamp));

  canvas->drawRoundRect(skrect, SkIntToScalar(1), SkIntToScalar(1), flags);
  flags.setShader(nullptr);

  if (button.has_border) {
    int border_alpha = state == kHovered ? 0x80 : 0x55;
    if (button.is_focused) {
      border_alpha = 0xFF;
      flags.setColor(GetSystemColor(kColorId_FocusedBorderColor, color_scheme));
    }
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(SkIntToScalar(1));
    flags.setAlpha(border_alpha);
    skrect.inset(SkFloatToScalar(.5f), SkFloatToScalar(.5f));
    canvas->drawRoundRect(skrect, SkIntToScalar(1), SkIntToScalar(1), flags);
  }
}

void NativeThemeBase::PaintTextField(cc::PaintCanvas* canvas,
                                     State state,
                                     const gfx::Rect& rect,
                                     const TextFieldExtraParams& text,
                                     ColorScheme color_scheme) const {
  if (features::IsFormControlsRefreshEnabled()) {
    SkRect bounds = gfx::RectToSkRect(rect);
    const SkScalar borderRadius = SkIntToScalar(kInputBorderRadius);

    // Paint the background (is not visible behind the rounded corners).
    bounds.inset(kBorderWidth / 2, kBorderWidth / 2);
    cc::PaintFlags fill_flags;
    fill_flags.setStyle(cc::PaintFlags::kFill_Style);
    if (text.background_color != 0) {
      PaintLightenLayer(canvas, bounds, state, borderRadius, color_scheme);
      fill_flags.setColor(ControlsBackgroundColorForState(state, color_scheme));
      canvas->drawRoundRect(bounds, borderRadius, borderRadius, fill_flags);
    }

    // Paint the border: 1px solid.
    cc::PaintFlags stroke_flags;
    stroke_flags.setColor(ControlsBorderColorForState(state, color_scheme));
    stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
    stroke_flags.setStrokeWidth(kBorderWidth);
    canvas->drawRoundRect(bounds, borderRadius, borderRadius, stroke_flags);
    return;
  }

  SkRect bounds;
  bounds.setLTRB(rect.x(), rect.y(), rect.right() - 1, rect.bottom() - 1);

  cc::PaintFlags fill_flags;
  fill_flags.setStyle(cc::PaintFlags::kFill_Style);
  fill_flags.setColor(text.background_color);
  canvas->drawRect(bounds, fill_flags);

  // Text INPUT, listbox SELECT, and TEXTAREA have consistent borders.
  // border: 1px solid #a9a9a9
  cc::PaintFlags stroke_flags;
  stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
  stroke_flags.setColor(GetColor(kTextBorderColor, color_scheme));
  canvas->drawRect(bounds, stroke_flags);
}

void NativeThemeBase::PaintMenuList(cc::PaintCanvas* canvas,
                                    State state,
                                    const gfx::Rect& rect,
                                    const MenuListExtraParams& menu_list,
                                    ColorScheme color_scheme) const {
  if (features::IsFormControlsRefreshEnabled()) {
    // If a border radius is specified paint the background and the border of
    // the menulist, otherwise let the non-theming code paint the background
    // and the border of the control. The arrow (menulist button) is always
    // painted by the theming code.
    if (!menu_list.has_border_radius) {
      TextFieldExtraParams text_field = {0};
      text_field.background_color = menu_list.background_color;
      PaintTextField(canvas, state, rect, text_field, color_scheme);
    }

    // Paint the arrow.
    cc::PaintFlags flags;
    flags.setColor(menu_list.arrow_color);
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kMenuListArrowStrokeWidth);

    float arrow_width = menu_list.arrow_size;
    int arrow_height = arrow_width * 0.5;
    gfx::Rect arrow(menu_list.arrow_x, menu_list.arrow_y - (arrow_height / 2),
                    arrow_width, arrow_height);
    arrow.Intersect(rect);

    if (arrow_width != arrow.width() || arrow_height != arrow.height()) {
      // The arrow is clipped after being constrained to the paint rect so we
      // need to recalculate its size.
      int height_clip = arrow_height - arrow.height();
      int width_clip = arrow_width - arrow.width();
      if (height_clip > width_clip) {
        arrow.set_width(arrow.height() * 1.6);
      } else {
        arrow.set_height(arrow.width() * 0.6);
      }
      arrow.set_y(menu_list.arrow_y - (arrow.height() / 2));
    }

    SkPath path;
    path.moveTo(arrow.x(), arrow.y());
    path.lineTo(arrow.x() + arrow.width() / 2, arrow.y() + arrow.height());
    path.lineTo(arrow.x() + arrow.width(), arrow.y());
    canvas->drawPath(path, flags);
    return;
  }

  // If a border radius is specified, we let the WebCore paint the background
  // and the border of the control.
  if (!menu_list.has_border_radius) {
    ButtonExtraParams button = { 0 };
    button.background_color = menu_list.background_color;
    button.has_border = menu_list.has_border;
    PaintButton(canvas, state, rect, button, color_scheme);
  }

  cc::PaintFlags flags;
  flags.setColor(menu_list.arrow_color);
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  int arrow_size = menu_list.arrow_size;
  gfx::Rect arrow(
    menu_list.arrow_x,
    menu_list.arrow_y - (arrow_size / 2),
    arrow_size,
    arrow_size);

  // Constrain to the paint rect.
  arrow.Intersect(rect);

  SkPath path;
  path.moveTo(arrow.x(), arrow.y());
  path.lineTo(arrow.right(), arrow.y());
  path.lineTo(arrow.x() + arrow.width() / 2, arrow.bottom());
  path.close();
  canvas->drawPath(path, flags);
}

void NativeThemeBase::PaintMenuPopupBackground(
    cc::PaintCanvas* canvas,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background,
    ColorScheme color_scheme) const {
  // We are currently only painting kMenuPopupBackground with the kDefault
  // scheme. If that changes, we need to add an appropriate dark scheme color to
  // kMenuPopupBackgroundColor.
  DCHECK(color_scheme == ColorScheme::kDefault);
  canvas->drawColor(GetColor(kMenuPopupBackgroundColor, color_scheme),
                    SkBlendMode::kSrc);
}

void NativeThemeBase::PaintMenuItemBackground(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuItemExtraParams& menu_item,
    ColorScheme color_scheme) const {
  // By default don't draw anything over the normal background.
}

void NativeThemeBase::PaintMenuSeparator(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuSeparatorExtraParams& menu_separator,
    ColorScheme color_scheme) const {
  cc::PaintFlags flags;
  flags.setColor(GetSystemColor(ui::NativeTheme::kColorId_MenuSeparatorColor,
                                color_scheme));
  canvas->drawRect(gfx::RectToSkRect(*menu_separator.paint_rect), flags);
}

void NativeThemeBase::PaintSliderTrack(cc::PaintCanvas* canvas,
                                       State state,
                                       const gfx::Rect& rect,
                                       const SliderExtraParams& slider,
                                       ColorScheme color_scheme) const {
  if (features::IsFormControlsRefreshEnabled()) {
    // Paint the entire slider track.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(ControlsFillColorForState(state, color_scheme));
    const float track_height = kSliderTrackHeight * slider.zoom;
    SkRect track_rect = AlignSliderTrack(rect, slider, false, track_height);
    // Shrink the track by 1 pixel so the thumb can completely cover the track
    // on both ends.
    if (slider.vertical)
      track_rect.inset(0, 1);
    else
      track_rect.inset(1, 0);
    canvas->drawRoundRect(track_rect, kSliderTrackBorderRadius,
                          kSliderTrackBorderRadius, flags);

    // Clip the track to create rounded corners for the value bar.
    SkRRect rounded_rect;
    rounded_rect.setRectXY(track_rect, kSliderTrackBorderRadius,
                           kSliderTrackBorderRadius);
    canvas->clipRRect(rounded_rect, SkClipOp::kIntersect, true);

    // Paint the value slider track.
    flags.setColor(ControlsSliderColorForState(state, color_scheme));
    SkRect value_rect = AlignSliderTrack(rect, slider, true, track_height);
    canvas->drawRect(value_rect, flags);

    // Paint the border.
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kBorderWidth);
    SkColor border_color = ControlsBorderColorForState(state, color_scheme);
    if (!UsesHighContrastColors() && state != kDisabled)
      border_color = SkColorSetA(border_color, 0x80);
    flags.setColor(border_color);
    track_rect.inset(kBorderWidth / 2, kBorderWidth / 2);
    canvas->drawRoundRect(track_rect, kSliderTrackBorderRadius,
                          kSliderTrackBorderRadius, flags);
    return;
  }

  const int kMidX = rect.x() + rect.width() / 2;
  const int kMidY = rect.y() + rect.height() / 2;

  cc::PaintFlags flags;
  flags.setColor(GetColor(kSliderTrackBackgroundColor, color_scheme));

  SkRect skrect;
  if (slider.vertical) {
    skrect.setLTRB(std::max(rect.x(), kMidX - 2), rect.y(),
                   std::min(rect.right(), kMidX + 2), rect.bottom());
  } else {
    skrect.setLTRB(rect.x(), std::max(rect.y(), kMidY - 2), rect.right(),
                   std::min(rect.bottom(), kMidY + 2));
  }
  canvas->drawRect(skrect, flags);
}

void NativeThemeBase::PaintSliderThumb(cc::PaintCanvas* canvas,
                                       State state,
                                       const gfx::Rect& rect,
                                       const SliderExtraParams& slider,
                                       ColorScheme color_scheme) const {
  if (features::IsFormControlsRefreshEnabled()) {
    const SkScalar radius = SkFloatToScalar(
        static_cast<float>(std::max(rect.width(), rect.height())) * 0.5);
    SkRect thumb_rect = gfx::RectToSkRect(rect);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    SkScalar border_width = kSliderThumbBorderWidth;
    if (state == kHovered || state == kPressed) {
      border_width = kSliderThumbBorderHoveredWidth;
    }

    // Paint the background (is not visible behind the rounded corners).
    thumb_rect.inset(border_width / 2, border_width / 2);
    flags.setColor(ControlsSliderColorForState(state, color_scheme));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->drawRoundRect(thumb_rect, radius, radius, flags);
    return;
  }

  const bool hovered = (state == kHovered) || slider.in_drag;
  const int kMidX = rect.x() + rect.width() / 2;
  const int kMidY = rect.y() + rect.height() / 2;

  cc::PaintFlags flags;
  flags.setColor(GetColor(
      hovered ? kSliderThumbHoveredBrightColor : kSliderThumbBrightColor,
      color_scheme));

  SkIRect skrect;
  if (slider.vertical)
    skrect.setLTRB(rect.x(), rect.y(), kMidX + 1, rect.bottom());
  else
    skrect.setLTRB(rect.x(), rect.y(), rect.right(), kMidY + 1);

  canvas->drawIRect(skrect, flags);

  flags.setColor(GetColor(
      hovered ? kSliderThumbHoveredShadedColor : kSliderThumbShadedColor,
      color_scheme));

  if (slider.vertical)
    skrect.setLTRB(kMidX + 1, rect.y(), rect.right(), rect.bottom());
  else
    skrect.setLTRB(rect.x(), kMidY + 1, rect.right(), rect.bottom());

  canvas->drawIRect(skrect, flags);

  flags.setColor(GetColor(kSliderThumbBorder, color_scheme));
  DrawBox(canvas, rect, flags);

  if (rect.height() > 10 && rect.width() > 10) {
    DrawHorizLine(canvas, kMidX - 2, kMidX + 2, kMidY, flags);
    DrawHorizLine(canvas, kMidX - 2, kMidX + 2, kMidY - 3, flags);
    DrawHorizLine(canvas, kMidX - 2, kMidX + 2, kMidY + 3, flags);
  }
}

void NativeThemeBase::PaintInnerSpinButton(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const InnerSpinButtonExtraParams& spin_button,
    ColorScheme color_scheme) const {
  if (spin_button.read_only)
    state = kDisabled;

  State north_state = state;
  State south_state = state;
  if (spin_button.spin_up)
    south_state = south_state != kDisabled ? kNormal : kDisabled;
  else
    north_state = north_state != kDisabled ? kNormal : kDisabled;

  gfx::Rect half = rect;
  half.set_height(rect.height() / 2);
  ScrollbarArrowExtraParams arrow = ScrollbarArrowExtraParams();
  arrow.zoom = 1.0;
  PaintArrowButton(canvas, half, kScrollbarUpArrow, north_state, color_scheme,
                   arrow);

  half.set_y(rect.y() + rect.height() / 2);
  PaintArrowButton(canvas, half, kScrollbarDownArrow, south_state, color_scheme,
                   arrow);
}

void NativeThemeBase::PaintProgressBar(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const ProgressBarExtraParams& progress_bar,
    ColorScheme color_scheme) const {
  if (features::IsFormControlsRefreshEnabled()) {
    DCHECK(!rect.IsEmpty());

    // Paint the track.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(GetControlColor(kFill, color_scheme));
    SliderExtraParams slider;
    slider.vertical = false;
    float track_height = rect.height() * kTrackHeightRatio;
    SkRect track_rect = AlignSliderTrack(rect, slider, false, track_height);
    canvas->drawRoundRect(track_rect, kSliderTrackBorderRadius,
                          kSliderTrackBorderRadius, flags);

    // Clip the track to create rounded corners for the value bar.
    SkRRect rounded_rect;
    rounded_rect.setRectXY(track_rect, kSliderTrackBorderRadius,
                           kSliderTrackBorderRadius);
    canvas->clipRRect(rounded_rect, SkClipOp::kIntersect, true);

    // Paint the progress value bar.
    const SkScalar kMinimumProgressValueWidth = 2;
    SkScalar adjusted_width = progress_bar.value_rect_width;
    if (adjusted_width > 0 && adjusted_width < kMinimumProgressValueWidth)
      adjusted_width = kMinimumProgressValueWidth;
    gfx::Rect original_value_rect(progress_bar.value_rect_x,
                                  progress_bar.value_rect_y, adjusted_width,
                                  progress_bar.value_rect_height);
    SkRect value_rect =
        AlignSliderTrack(original_value_rect, slider, false, track_height);
    flags.setColor(GetControlColor(kAccent, color_scheme));
    if (progress_bar.determinate) {
      canvas->drawRect(value_rect, flags);
    } else {
      canvas->drawRoundRect(value_rect, kSliderTrackBorderRadius,
                            kSliderTrackBorderRadius, flags);
    }

    // Paint the border.
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kBorderWidth);
    SkColor border_color = GetControlColor(kBorder, color_scheme);
    if (!UsesHighContrastColors())
      border_color = SkColorSetA(border_color, 0x80);
    flags.setColor(border_color);
    track_rect.inset(kBorderWidth / 2, kBorderWidth / 2);
    canvas->drawRoundRect(track_rect, kSliderTrackBorderRadius,
                          kSliderTrackBorderRadius, flags);
    return;
  }

  DCHECK(!rect.IsEmpty());

  canvas->drawColor(GetColor(kProgressBackgroundColor, color_scheme));

  // Draw the tick marks. The spacing between the tick marks is adjusted to
  // evenly divide into the width.
  SkPath path;
  int stroke_width = std::max(1, rect.height() / 18);
  int tick_width = 16 * stroke_width;
  int ticks = rect.width() / tick_width + (rect.width() % tick_width ? 1 : 0);
  SkScalar tick_spacing = SkIntToScalar(rect.width()) / ticks;
  for (int i = 1; i < ticks; ++i) {
    path.moveTo(rect.x() + i * tick_spacing, rect.y());
    path.rLineTo(0, rect.height());
  }
  cc::PaintFlags stroke_flags;
  stroke_flags.setColor(GetColor(kProgressTickColor, color_scheme));
  stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
  stroke_flags.setStrokeWidth(stroke_width);
  canvas->drawPath(path, stroke_flags);

  // Draw progress.
  gfx::Rect progress_rect(progress_bar.value_rect_x, progress_bar.value_rect_y,
                          progress_bar.value_rect_width,
                          progress_bar.value_rect_height);
  cc::PaintFlags progress_flags;
  progress_flags.setColor(GetColor(kProgressValueColor, color_scheme));
  progress_flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->drawRect(gfx::RectToSkRect(progress_rect), progress_flags);

  // Draw the border.
  gfx::RectF border_rect(rect);
  border_rect.Inset(stroke_width / 2.0f, stroke_width / 2.0f);
  stroke_flags.setColor(GetColor(kProgressBorderColor, color_scheme));
  canvas->drawRect(gfx::RectFToSkRect(border_rect), stroke_flags);
}

void NativeThemeBase::PaintFrameTopArea(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const FrameTopAreaExtraParams& frame_top_area,
    ColorScheme color_scheme) const {
  cc::PaintFlags flags;
  flags.setColor(frame_top_area.default_background_color);
  canvas->drawRect(gfx::RectToSkRect(rect), flags);
}

void NativeThemeBase::AdjustCheckboxRadioRectForPadding(SkRect* rect) const {
  // By default we only take 1px from right and bottom for the drop shadow.
  rect->setLTRB(static_cast<int>(rect->x()), static_cast<int>(rect->y()),
                static_cast<int>(rect->right()) - 1,
                static_cast<int>(rect->bottom()) - 1);
}

SkColor NativeThemeBase::SaturateAndBrighten(SkScalar* hsv,
                                             SkScalar saturate_amount,
                                             SkScalar brighten_amount) const {
  SkScalar color[3];
  color[0] = hsv[0];
  color[1] =
      base::ClampToRange(hsv[1] + saturate_amount, SkScalar{0}, SK_Scalar1);
  color[2] =
      base::ClampToRange(hsv[2] + brighten_amount, SkScalar{0}, SK_Scalar1);
  return SkHSVToColor(color);
}

SkColor NativeThemeBase::GetArrowColor(State state,
                                       ColorScheme color_scheme) const {
  if (state != kDisabled)
    return GetColor(kArrowDisabledColor, color_scheme);

  SkScalar track_hsv[3];
  SkColorToHSV(GetColor(kTrackColor, color_scheme), track_hsv);

  SkScalar thumb_hsv[3];
  SkColorToHSV(GetColor(kThumbInactiveColor, color_scheme), thumb_hsv);
  return OutlineColor(track_hsv, thumb_hsv);
}

void NativeThemeBase::DrawVertLine(cc::PaintCanvas* canvas,
                                   int x,
                                   int y1,
                                   int y2,
                                   const cc::PaintFlags& flags) const {
  SkIRect skrect;
  skrect.setLTRB(x, y1, x + 1, y2 + 1);
  canvas->drawIRect(skrect, flags);
}

void NativeThemeBase::DrawHorizLine(cc::PaintCanvas* canvas,
                                    int x1,
                                    int x2,
                                    int y,
                                    const cc::PaintFlags& flags) const {
  SkIRect skrect;
  skrect.setLTRB(x1, y, x2 + 1, y + 1);
  canvas->drawIRect(skrect, flags);
}

void NativeThemeBase::DrawBox(cc::PaintCanvas* canvas,
                              const gfx::Rect& rect,
                              const cc::PaintFlags& flags) const {
  const int right = rect.x() + rect.width() - 1;
  const int bottom = rect.y() + rect.height() - 1;
  DrawHorizLine(canvas, rect.x(), right, rect.y(), flags);
  DrawVertLine(canvas, right, rect.y(), bottom, flags);
  DrawHorizLine(canvas, rect.x(), right, bottom, flags);
  DrawVertLine(canvas, rect.x(), rect.y(), bottom, flags);
}

SkColor NativeThemeBase::OutlineColor(SkScalar* hsv1, SkScalar* hsv2) const {
  // GTK Theme engines have way too much control over the layout of
  // the scrollbar. We might be able to more closely approximate its
  // look-and-feel, if we sent whole images instead of just colors
  // from the browser to the renderer. But even then, some themes
  // would just break.
  //
  // So, instead, we don't even try to 100% replicate the look of
  // the native scrollbar. We render our own version, but we make
  // sure to pick colors that blend in nicely with the system GTK
  // theme. In most cases, we can just sample a couple of pixels
  // from the system scrollbar and use those colors to draw our
  // scrollbar.
  //
  // This works fine for the track color and the overall thumb
  // color. But it fails spectacularly for the outline color used
  // around the thumb piece.  Not all themes have a clearly defined
  // outline. For some of them it is partially transparent, and for
  // others the thickness is very unpredictable.
  //
  // So, instead of trying to approximate the system theme, we
  // instead try to compute a reasonable looking choice based on the
  // known color of the track and the thumb piece. This is difficult
  // when trying to deal both with high- and low-contrast themes,
  // and both with positive and inverted themes.
  //
  // The following code has been tested to look OK with all of the
  // default GTK themes.
  SkScalar min_diff =
      base::ClampToRange((hsv1[1] + hsv2[1]) * 1.2f, 0.28f, 0.5f);
  SkScalar diff =
      base::ClampToRange(fabs(hsv1[2] - hsv2[2]) / 2, min_diff, 0.5f);

  if (hsv1[2] + hsv2[2] > 1.0)
    diff = -diff;

  return SaturateAndBrighten(hsv2, -0.2f, diff);
}

SkColor NativeThemeBase::ControlsAccentColorForState(
    State state,
    ColorScheme color_scheme) const {
  ControlColorId color_id;
  if (state == kHovered) {
    color_id = kHoveredAccent;
  } else if (state == kPressed) {
    color_id = kHoveredAccent;
  } else if (state == kDisabled) {
    color_id = kDisabledAccent;
  } else {
    color_id = kAccent;
  }
  return GetControlColor(color_id, color_scheme);
}

SkColor NativeThemeBase::ControlsSliderColorForState(
    State state,
    ColorScheme color_scheme) const {
  ControlColorId color_id;
  if (state == kHovered) {
    color_id = kHoveredSlider;
  } else if (state == kPressed) {
    color_id = kHoveredSlider;
  } else if (state == kDisabled) {
    color_id = kDisabledSlider;
  } else {
    color_id = kSlider;
  }
  return GetControlColor(color_id, color_scheme);
}

SkColor NativeThemeBase::ControlsBorderColorForState(
    State state,
    ColorScheme color_scheme) const {
  ControlColorId color_id;
  if (state == kHovered) {
    color_id = kHoveredBorder;
  } else if (state == kDisabled) {
    color_id = kDisabledBorder;
  } else {
    color_id = kBorder;
  }
  return GetControlColor(color_id, color_scheme);
}

SkColor NativeThemeBase::ControlsFillColorForState(
    State state,
    ColorScheme color_scheme) const {
  ControlColorId color_id;
  if (state == kHovered) {
    color_id = kHoveredFill;
  } else if (state == kPressed) {
    color_id = kHoveredFill;
  } else if (state == kDisabled) {
    color_id = kDisabledFill;
  } else {
    color_id = kFill;
  }
  return GetControlColor(color_id, color_scheme);
}

SkColor NativeThemeBase::ControlsBackgroundColorForState(
    State state,
    ColorScheme color_scheme) const {
  ControlColorId color_id;
  if (state == kDisabled) {
    color_id = kDisabledBackground;
  } else {
    color_id = kBackground;
  }
  return GetControlColor(color_id, color_scheme);
}

SkColor NativeThemeBase::GetControlColor(ControlColorId color_id,
                                         ColorScheme color_scheme) const {
  if (UsesHighContrastColors())
    return GetHighContrastControlColor(color_id, color_scheme);

  switch (color_id) {
    case kBorder:
      return SkColorSetRGB(0x76, 0x76, 0x76);
    case kHoveredBorder:
      return SkColorSetRGB(0x4F, 0x4F, 0x4F);
    case kDisabledBorder:
      return SkColorSetARGB(0x4D, 0x76, 0x76, 0x76);
    case kAccent:
      return SkColorSetRGB(0x00, 0x75, 0xFF);
    case kHoveredAccent:
      return SkColorSetRGB(0x00, 0x5C, 0xC8);
    case kDisabledAccent:
      return SkColorSetARGB(0x4D, 0x76, 0x76, 0x76);
    case kBackground:
      return SK_ColorWHITE;
    case kDisabledBackground:
      return SkColorSetA(SK_ColorWHITE, 0x99);
    case kFill:
      return SkColorSetRGB(0xEF, 0xEF, 0xEF);
    case kHoveredFill:
      return SkColorSetRGB(0xE5, 0xE5, 0xE5);
    case kDisabledFill:
      return SkColorSetARGB(0x4D, 0xEF, 0xEF, 0xEF);
    case kLightenLayer:
      return SkColorSetARGB(0x33, 0xA9, 0xA9, 0xA9);
    case kProgressValue:
      return SkColorSetRGB(0x00, 0x75, 0xFF);
    case kSlider:
      return SkColorSetRGB(0x00, 0x75, 0xFF);
    case kHoveredSlider:
      return SkColorSetRGB(0x00, 0x5C, 0xC8);
    case kDisabledSlider:
      return SkColorSetRGB(0xCB, 0xCB, 0xCB);
  }
  NOTREACHED();
  return gfx::kPlaceholderColor;
}

SkColor NativeThemeBase::GetHighContrastControlColor(
    ControlColorId color_id,
    ColorScheme color_scheme) const {
  if (!system_colors_.empty()) {
    switch (color_id) {
      case kDisabledBorder:
      case kDisabledAccent:
      case kDisabledSlider:
        return system_colors_[SystemThemeColor::kGrayText];
      case kBorder:
      case kHoveredBorder:
        return system_colors_[SystemThemeColor::kButtonText];
      case kAccent:
      case kHoveredAccent:
      case kProgressValue:
      case kSlider:
      case kHoveredSlider:
        return system_colors_[SystemThemeColor::kHighlight];
      case kBackground:
      case kDisabledBackground:
      case kFill:
      case kHoveredFill:
      case kDisabledFill:
      case kLightenLayer:
        return system_colors_[SystemThemeColor::kWindow];
    }
  } else {
    // Default high contrast colors (used in web test mode)
    switch (color_id) {
      case kDisabledBorder:
      case kDisabledAccent:
      case kDisabledSlider:
        return SK_ColorGREEN;
      case kBorder:
      case kHoveredBorder:
        return SK_ColorWHITE;
      case kAccent:
      case kHoveredAccent:
      case kProgressValue:
      case kSlider:
      case kHoveredSlider:
        return SK_ColorCYAN;
      case kBackground:
      case kDisabledBackground:
      case kFill:
      case kHoveredFill:
      case kDisabledFill:
      case kLightenLayer:
        return SK_ColorBLACK;
    }
  }
  NOTREACHED();
  return gfx::kPlaceholderColor;
}

void NativeThemeBase::PaintLightenLayer(cc::PaintCanvas* canvas,
                                        SkRect skrect,
                                        State state,
                                        SkScalar border_radius,
                                        ColorScheme color_scheme) const {
  if (state == kDisabled) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    // Draw the lighten layer to lighten the background so the translucent
    // disabled color works regardless of what it's over.
    flags.setColor(GetControlColor(kLightenLayer, color_scheme));
    canvas->drawRoundRect(skrect, border_radius, border_radius, flags);
  }
}

SkRect NativeThemeBase::AlignSliderTrack(
    const gfx::Rect& slider_rect,
    const NativeTheme::SliderExtraParams& slider,
    bool is_value,
    float track_height) const {
  const float kAlignment = track_height / 2;
  const float mid_x = slider_rect.x() + slider_rect.width() / 2.0f;
  const float mid_y = slider_rect.y() + slider_rect.height() / 2.0f;
  SkRect aligned_rect;

  if (slider.vertical) {
    const float top = is_value ? slider_rect.y() + slider.thumb_y + kAlignment
                               : slider_rect.y();
    aligned_rect.setLTRB(
        std::max(float(slider_rect.x()), mid_x - kAlignment), top,
        std::min(float(slider_rect.right()), mid_x + kAlignment),
        slider_rect.bottom());
  } else {
    const float right = is_value ? slider_rect.x() + slider.thumb_x + kAlignment
                                 : slider_rect.right();
    aligned_rect.setLTRB(
        slider_rect.x(), std::max(float(slider_rect.y()), mid_y - kAlignment),
        right, std::min(float(slider_rect.bottom()), mid_y + kAlignment));
  }

  return aligned_rect;
}

}  // namespace ui
