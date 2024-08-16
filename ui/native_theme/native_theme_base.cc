// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/native_theme/native_theme_base.h"

#include <algorithm>
#include <limits>
#include <memory>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"

namespace {

// These are the default dimensions of radio buttons and checkboxes.
const int kCheckboxAndRadioWidth = 13;
const int kCheckboxAndRadioHeight = 13;

// Color constant pairs for light/default and dark color-schemes below.
constexpr SkColor kThumbActiveColor[2] = {SkColorSetRGB(0xF4, 0xF4, 0xF4),
                                          gfx::kPlaceholderColor};
constexpr SkColor kThumbInactiveColor[2] = {SkColorSetRGB(0xEA, 0xEA, 0xEA),
                                            SK_ColorWHITE};
constexpr SkColor kTrackColor[2] = {SkColorSetRGB(0xD3, 0xD3, 0xD3),
                                    gfx::kPlaceholderColor};
// We are currently only painting kMenuPopupBackground with the kDefault
// scheme. If that changes, we need to replace gfx::kPlaceholderColor with an
// appropriate dark scheme color. See the DCHECK in PaintMenuPopupBackground().
constexpr SkColor kMenuPopupBackgroundColor[2] = {SkColorSetRGB(210, 225, 246),
                                                  gfx::kPlaceholderColor};
constexpr SkColor kArrowDisabledColor[2] = {SK_ColorBLACK, SK_ColorWHITE};

// The "dash" is 8x2 px by default (the checkbox is 13x13 px).
const SkScalar kIndeterminateInsetWidthRatio = (13 - 8) / 2.0f / 13;
const SkScalar kIndeterminateInsetHeightRatio = (13 - 2) / 2.0f / 13;
const SkScalar kBorderWidth = 1.f;
const SkScalar kSliderTrackHeight = 8.f;
const SkScalar kSliderThumbBorderWidth = 1.f;
const SkScalar kSliderThumbBorderHoveredWidth = 1.f;
// Default block size for progress is 16px and the track is 8px.
const SkScalar kTrackBlockRatio = 8.0f / 16;
const SkScalar kMenuListArrowStrokeWidth = 2.f;
const int kSliderThumbSize = 16;

// This value was created with the following steps:
// 1. Take the SkColors returned by GetControlColor for kAccent and
//    kHoveredAccent.
// 2. use color_utils::SkColorToHSL to convert those colors to HSL.
// 3. Take the difference of the luminance component of the HSL between those
//    two colors.
// 4. Round to the nearest two decimal points.
//
// This is used to emulate the changes in color used for hover and pressed
// states when a custom accent-color is used to draw form controls. It just so
// happens that the luminance difference is the same for hover and press, and it
// also happens that GetDarkModeControlColor has very close values when you run
// these steps, which makes it work well for forced color-scheme for contrast
// with certain accent-colors.
const double kAccentLuminanceAdjust = 0.11;

// Get a color constant based on color-scheme
// TODO(crbug.com/40242489): Move colors defined above to the color pipeline and
// remove this function.
SkColor GetColor(const SkColor colors[2],
                 ui::NativeTheme::ColorScheme color_scheme) {
  return colors[color_scheme == ui::NativeTheme::ColorScheme::kDark ? 1 : 0];
}

SkColor AdjustLuminance(const SkColor& color, double luminance) {
  color_utils::HSL hsl;
  color_utils::SkColorToHSL(color, &hsl);
  hsl.l = std::clamp(hsl.l + luminance, 0., 1.);
  return color_utils::HSLToSkColor(hsl, SkColorGetA(color));
}

SkColor CustomAccentColorForState(
    const SkColor& accent_color,
    ui::NativeTheme::State state,
    const ui::NativeTheme::ColorScheme& color_scheme) {
  bool make_lighter = false;
  bool is_dark_mode = color_scheme == ui::NativeTheme::ColorScheme::kDark;
  switch (state) {
    case ui::NativeTheme::kHovered:
      make_lighter = is_dark_mode;
      break;
    case ui::NativeTheme::kPressed:
      make_lighter = !is_dark_mode;
      break;
    default:
      return accent_color;
  }
  return AdjustLuminance(accent_color,
                         (make_lighter ? 1 : -1) * kAccentLuminanceAdjust);
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
      return gfx::Size(kSliderThumbSize, kSliderThumbSize);
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
      NOTREACHED_IN_MIGRATION() << "Unknown theme part: " << part;
      break;
  }
  return gfx::Size();
}

float NativeThemeBase::GetBorderRadiusForPart(Part part,
                                              float width,
                                              float height) const {
  switch (part) {
    case kCheckbox:
      return 2.f;
    case kPushButton:
    case kTextField:
      return 2.f;
    case kRadio:
      return std::max(width, height) * 0.5;
    case kProgressBar:
    case kSliderTrack:
      // default border radius for progress and range is 40px.
      return 40.f;
    case kSliderThumb:
      return std::max(width, height) * 0.5;
    default:
      break;
  }
  return 0;
}

void NativeThemeBase::Paint(cc::PaintCanvas* canvas,
                            const ui::ColorProvider* color_provider,
                            Part part,
                            State state,
                            const gfx::Rect& rect,
                            const ExtraParams& extra,
                            ColorScheme color_scheme,
                            bool in_forced_colors,
                            const std::optional<SkColor>& accent_color) const {
  if (rect.IsEmpty())
    return;

  canvas->save();
  canvas->clipRect(gfx::RectToSkRect(rect));

  // Form control accents shouldn't be drawn with any transparency.
  std::optional<SkColor> accent_color_opaque;
  if (accent_color) {
    accent_color_opaque = SkColorSetA(accent_color.value(), SK_AlphaOPAQUE);
  }

  switch (part) {
    // Please keep these in the order of NativeTheme::Part.
    case kCheckbox:
      PaintCheckbox(canvas, color_provider, state, rect,
                    absl::get<ButtonExtraParams>(extra), color_scheme,
                    accent_color_opaque);
      break;
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
    case kFrameTopArea:
      PaintFrameTopArea(canvas, state, rect,
                        absl::get<FrameTopAreaExtraParams>(extra),
                        color_scheme);
      break;
#endif
    case kInnerSpinButton:
      PaintInnerSpinButton(canvas, color_provider, state, rect,
                           absl::get<InnerSpinButtonExtraParams>(extra),
                           color_scheme, in_forced_colors);
      break;
    case kMenuList:
      PaintMenuList(canvas, color_provider, state, rect,
                    absl::get<MenuListExtraParams>(extra), color_scheme);
      break;
    case kMenuPopupBackground:
      PaintMenuPopupBackground(canvas, color_provider, rect.size(),
                               absl::get<MenuBackgroundExtraParams>(extra),
                               color_scheme);
      break;
    case kMenuPopupSeparator:
      PaintMenuSeparator(canvas, color_provider, state, rect,
                         absl::get<MenuSeparatorExtraParams>(extra));
      break;
    case kMenuItemBackground:
      PaintMenuItemBackground(canvas, color_provider, state, rect,
                              absl::get<MenuItemExtraParams>(extra),
                              color_scheme);
      break;
    case kProgressBar:
      PaintProgressBar(canvas, color_provider, state, rect,
                       absl::get<ProgressBarExtraParams>(extra), color_scheme,
                       accent_color_opaque);
      break;
    case kPushButton:
      PaintButton(canvas, color_provider, state, rect,
                  absl::get<ButtonExtraParams>(extra), color_scheme);
      break;
    case kRadio:
      PaintRadio(canvas, color_provider, state, rect,
                 absl::get<ButtonExtraParams>(extra), color_scheme,
                 accent_color_opaque);
      break;
    case kScrollbarDownArrow:
    case kScrollbarUpArrow:
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
      if (scrollbar_button_length_ > 0)
        PaintArrowButton(canvas, color_provider, rect, part, state,
                         color_scheme, in_forced_colors,
                         absl::get<ScrollbarArrowExtraParams>(extra));
      break;
    case kScrollbarHorizontalThumb:
    case kScrollbarVerticalThumb:
      PaintScrollbarThumb(canvas, color_provider, part, state, rect,
                          absl::get<ScrollbarThumbExtraParams>(extra),
                          color_scheme);
      break;
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack:
      PaintScrollbarTrack(canvas, color_provider, part, state,
                          absl::get<ScrollbarTrackExtraParams>(extra), rect,
                          color_scheme, in_forced_colors);
      break;
    case kScrollbarHorizontalGripper:
    case kScrollbarVerticalGripper:
      // Invoked by views scrollbar code, don't care about for non-win
      // implementations, so no NOTIMPLEMENTED.
      break;
    case kScrollbarCorner:
      PaintScrollbarCorner(canvas, color_provider, state, rect,
                           absl::get<ScrollbarTrackExtraParams>(extra),
                           color_scheme);
      break;
    case kSliderTrack:
      PaintSliderTrack(canvas, color_provider, state, rect,
                       absl::get<SliderExtraParams>(extra), color_scheme,
                       accent_color_opaque);
      break;
    case kSliderThumb:
      PaintSliderThumb(canvas, color_provider, state, rect,
                       absl::get<SliderExtraParams>(extra), color_scheme,
                       accent_color_opaque);
      break;
    case kTabPanelBackground:
      NOTIMPLEMENTED();
      break;
    case kTextField:
      PaintTextField(canvas, color_provider, state, rect,
                     absl::get<TextFieldExtraParams>(extra), color_scheme);
      break;
    case kTrackbarThumb:
    case kTrackbarTrack:
    case kWindowResizeGripper:
      NOTIMPLEMENTED();
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unknown theme part: " << part;
      break;
  }

  canvas->restore();
}

bool NativeThemeBase::SupportsNinePatch(Part part) const {
  return false;
}

gfx::Size NativeThemeBase::GetNinePatchCanvasSize(Part part) const {
  NOTREACHED_IN_MIGRATION()
      << "NativeThemeBase doesn't support nine-patch resources.";
  return gfx::Size();
}

gfx::Rect NativeThemeBase::GetNinePatchAperture(Part part) const {
  NOTREACHED_IN_MIGRATION()
      << "NativeThemeBase doesn't support nine-patch resources.";
  return gfx::Rect();
}

NativeThemeBase::NativeThemeBase() : NativeThemeBase(false) {}

NativeThemeBase::NativeThemeBase(bool should_only_use_dark_colors,
                                 ui::SystemTheme system_theme)
    : NativeTheme(should_only_use_dark_colors, system_theme) {}

NativeThemeBase::~NativeThemeBase() = default;

void NativeThemeBase::PaintArrowButton(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const gfx::Rect& rect,
    Part direction,
    State state,
    ColorScheme color_scheme,
    bool in_forced_colors,
    const ScrollbarArrowExtraParams& extra_params) const {
  cc::PaintFlags flags;

  // Calculate button color.
  SkScalar track_hsv[3];
  SkColorToHSV(
      extra_params.track_color.value_or(GetColor(kTrackColor, color_scheme)),
      track_hsv);
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

  // TODO(crbug.com/40596569): Adjust thumb_color based on `state`.
  const SkColor arrow_color =
      extra_params.thumb_color.has_value()
          ? extra_params.thumb_color.value()
          : GetArrowColor(state, color_scheme, color_provider);
  PaintArrow(canvas, rect, direction, arrow_color);
}

void NativeThemeBase::PaintArrow(cc::PaintCanvas* gc,
                                 const gfx::Rect& rect,
                                 Part direction,
                                 SkColor color) const {
  cc::PaintFlags flags;
  flags.setColor(color);

  SkPath path = PathForArrow(BoundingRectForArrow(rect), direction);

  gc->drawPath(path, flags);
}

SkPath NativeThemeBase::PathForArrow(const gfx::Rect& bounding_rect,
                                     Part direction) const {
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
    const ColorProvider* color_provider,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect,
    ColorScheme color_scheme,
    bool in_forced_colors) const {
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

void NativeThemeBase::PaintScrollbarThumb(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    Part part,
    State state,
    const gfx::Rect& rect,
    const ScrollbarThumbExtraParams& extra_params,
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

void NativeThemeBase::PaintScrollbarCorner(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const ScrollbarTrackExtraParams& extra_params,
    ColorScheme color_scheme) const {}

void NativeThemeBase::PaintCheckbox(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const ButtonExtraParams& button,
    ColorScheme color_scheme,
    const std::optional<SkColor>& accent_color) const {
  const float border_radius =
      GetBorderRadiusForPart(kCheckbox, rect.width(), rect.height());

  SkRect skrect =
      PaintCheckboxRadioCommon(canvas, color_provider, state, rect, button,
                               true, border_radius, color_scheme, accent_color);

  if (!skrect.isEmpty()) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);

    if (button.indeterminate || button.checked) {
      // Draw the accent background.
      flags.setStyle(cc::PaintFlags::kFill_Style);
      if (accent_color && state != kDisabled) {
        flags.setColor(
            CustomAccentColorForState(*accent_color, state, color_scheme));
      } else {
        flags.setColor(
            ControlsAccentColorForState(state, color_scheme, color_provider));
      }
      canvas->drawRoundRect(skrect, border_radius, border_radius, flags);

      if (button.indeterminate) {
        // Draw the dash.
        flags.setColor(ControlsBackgroundColorForState(state, color_scheme,
                                                       color_provider));
        skrect.inset(skrect.width() * kIndeterminateInsetWidthRatio,
                     skrect.height() * kIndeterminateInsetHeightRatio);
        canvas->drawRoundRect(skrect, border_radius, border_radius, flags);
      } else if (button.checked) {
        // Draw the checkmark.
        SkPath check;
        check.moveTo(skrect.x() + skrect.width() * 0.2, skrect.centerY());
        check.rLineTo(skrect.width() * 0.2, skrect.height() * 0.2);
        check.lineTo(skrect.right() - skrect.width() * 0.2,
                     skrect.y() + skrect.height() * 0.2);
        flags.setStyle(cc::PaintFlags::kStroke_Style);
        flags.setStrokeWidth(SkFloatToScalar(skrect.height() * 0.16));
        SkColor checkmark_color = ControlsBackgroundColorForState(
            state, color_scheme, color_provider);
        flags.setColor(checkmark_color);
        canvas->drawPath(check, flags);
      }
    }
  }
}

// Draws the common elements of checkboxes and radio buttons.
// Returns the rectangle within which any additional decorations should be
// drawn, or empty if none.
SkRect NativeThemeBase::PaintCheckboxRadioCommon(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const ButtonExtraParams& button,
    bool is_checkbox,
    const SkScalar border_radius,
    ColorScheme color_scheme,
    const std::optional<SkColor>& accent_color) const {
  SkRect skrect = gfx::RectToSkRect(rect);

  // Use the largest square that fits inside the provided rectangle.
  // No other browser seems to support non-square widget, so accidentally
  // having non-square sizes is common (eg. amazon and webkit dev tools).
  if (skrect.width() != skrect.height()) {
    SkScalar size = std::min(skrect.width(), skrect.height());
    skrect.inset((skrect.width() - size) / 2, (skrect.height() - size) / 2);
  }

  // If the rectangle is too small then paint only a rectangle. We don't want
  // to have to worry about '- 1' and '+ 1' calculations below having overflow
  // or underflow.
  if (skrect.width() <= 2) {
    cc::PaintFlags flags;
    if (accent_color && state != kDisabled) {
      flags.setColor(
          CustomAccentColorForState(*accent_color, state, color_scheme));
    } else {
      flags.setColor(GetControlColor(kBorder, color_scheme, color_provider));
    }
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
  PaintLightenLayer(canvas, color_provider, background_rect, state,
                    border_radius, color_scheme);
  flags.setColor(
      ControlsBackgroundColorForState(state, color_scheme, color_provider));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->drawRoundRect(background_rect, border_radius, border_radius, flags);

  // For checkbox the border is drawn only when it is unchecked.
  // For radio the border is always drawn.
  if (!is_checkbox || (!button.checked && !button.indeterminate)) {
    // Shrink half border width so the final pixels of the border will be
    // within the rectangle.
    const auto border_rect =
        skrect.makeInset(kBorderWidth / 2, kBorderWidth / 2);

    SkColor border_color;
    if (button.checked) {
      if (accent_color && state != kDisabled) {
        border_color =
            CustomAccentColorForState(*accent_color, state, color_scheme);
      } else {
        border_color =
            ControlsAccentColorForState(state, color_scheme, color_provider);
      }
    } else {
      border_color =
          ControlsBorderColorForState(state, color_scheme, color_provider);
    }
    flags.setColor(border_color);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kBorderWidth);
    canvas->drawRoundRect(border_rect, border_radius, border_radius, flags);
  }
  // Return the rectangle for drawing any additional decorations.
  return skrect;
}

void NativeThemeBase::PaintRadio(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const ButtonExtraParams& button,
    ColorScheme color_scheme,
    const std::optional<SkColor>& accent_color) const {
  // Most of a radio button is the same as a checkbox, except the the rounded
  // square is a circle (i.e. border radius >= 100%).
  const float border_radius =
      GetBorderRadiusForPart(kRadio, rect.width(), rect.height());
  SkRect skrect = PaintCheckboxRadioCommon(canvas, color_provider, state, rect,
                                           button, false, border_radius,
                                           color_scheme, accent_color);
  if (!skrect.isEmpty() && button.checked) {
    // Draw the dot.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    if (accent_color && state != kDisabled) {
      flags.setColor(
          CustomAccentColorForState(*accent_color, state, color_scheme));
    } else {
      flags.setColor(
          ControlsAccentColorForState(state, color_scheme, color_provider));
    }

    skrect.inset(skrect.width() * 0.2, skrect.height() * 0.2);
    // Use drawRoundedRect instead of drawOval to be completely consistent
    // with the border in PaintCheckboxRadioNewCommon.
    canvas->drawRoundRect(skrect, border_radius, border_radius, flags);
  }
}

void NativeThemeBase::PaintButton(cc::PaintCanvas* canvas,
                                  const ColorProvider* color_provider,
                                  State state,
                                  const gfx::Rect& rect,
                                  const ButtonExtraParams& button,
                                  ColorScheme color_scheme) const {
  cc::PaintFlags flags;
  SkRect skrect = gfx::RectToSkRect(rect);
  float border_width = AdjustBorderWidthByZoom(kBorderWidth, button.zoom);

  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  // If the button is too small, fallback to drawing a single, solid color.
  if (rect.width() < 5 || rect.height() < 5) {
    flags.setColor(
        ButtonFillColorForState(state, color_scheme, color_provider));
    canvas->drawRect(skrect, flags);
    return;
  }

  float border_radius =
      GetBorderRadiusForPart(kPushButton, rect.width(), rect.height());
  border_radius =
      AdjustBorderRadiusByZoom(kPushButton, border_radius, button.zoom);
  // Paint the background (is not visible behind the rounded corners).
  skrect.inset(border_width / 2, border_width / 2);
  PaintLightenLayer(canvas, color_provider, skrect, state, border_radius,
                    color_scheme);
  flags.setColor(ButtonFillColorForState(state, color_scheme, color_provider));
  canvas->drawRoundRect(skrect, border_radius, border_radius, flags);

  // Paint the border: 1px solid.
  if (button.has_border) {
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(border_width);
    flags.setColor(
        ButtonBorderColorForState(state, color_scheme, color_provider));
    canvas->drawRoundRect(skrect, border_radius, border_radius, flags);
  }
}

void NativeThemeBase::PaintTextField(cc::PaintCanvas* canvas,
                                     const ColorProvider* color_provider,
                                     State state,
                                     const gfx::Rect& rect,
                                     const TextFieldExtraParams& text,
                                     ColorScheme color_scheme) const {
  SkRect bounds = gfx::RectToSkRect(rect);
  float border_radius =
      GetBorderRadiusForPart(kTextField, rect.width(), rect.height());
  border_radius =
      AdjustBorderRadiusByZoom(kTextField, border_radius, text.zoom);
  float border_width = AdjustBorderWidthByZoom(kBorderWidth, text.zoom);

  // Paint the background (is not visible behind the rounded corners).
  bounds.inset(border_width / 2, border_width / 2);
  cc::PaintFlags fill_flags;
  fill_flags.setStyle(cc::PaintFlags::kFill_Style);
  if (text.background_color != 0) {
    PaintLightenLayer(canvas, color_provider, bounds, state, border_radius,
                      color_scheme);
    SkColor text_field_background_color =
        ControlsBackgroundColorForState(state, color_scheme, color_provider);
    if (text.auto_complete_active && state != kDisabled) {
      text_field_background_color = GetControlColor(
          kAutoCompleteBackground, color_scheme, color_provider);
    }
    fill_flags.setColor(text_field_background_color);
    canvas->drawRoundRect(bounds, border_radius, border_radius, fill_flags);
  }

  // Paint the border: 1px solid.
  if (text.has_border) {
    cc::PaintFlags stroke_flags;
    stroke_flags.setColor(
        ControlsBorderColorForState(state, color_scheme, color_provider));
    stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
    stroke_flags.setStrokeWidth(border_width);
    canvas->drawRoundRect(bounds, border_radius, border_radius, stroke_flags);
  }
}

void NativeThemeBase::PaintMenuList(cc::PaintCanvas* canvas,
                                    const ColorProvider* color_provider,
                                    State state,
                                    const gfx::Rect& rect,
                                    const MenuListExtraParams& menu_list,
                                    ColorScheme color_scheme) const {
  // If a border radius is specified paint the background and the border of
  // the menulist, otherwise let the non-theming code paint the background
  // and the border of the control. The arrow (menulist button) is always
  // painted by the theming code.
  if (!menu_list.has_border_radius) {
    TextFieldExtraParams text_field;
    text_field.background_color = menu_list.background_color;
    text_field.has_border = menu_list.has_border;
    text_field.zoom = menu_list.zoom;
    PaintTextField(canvas, color_provider, state, rect, text_field,
                   color_scheme);
  }

  // Paint the arrow.
  cc::PaintFlags flags;
  flags.setColor(menu_list.arrow_color);
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kMenuListArrowStrokeWidth);

  if (menu_list.arrow_direction == ui::NativeTheme::ArrowDirection::kDown) {
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
  } else {
    // Arrow direction is either left or right
    float arrow_height = menu_list.arrow_size;
    int arrow_width = arrow_height * 0.5;
    gfx::Rect arrow(menu_list.arrow_x - (arrow_width / 2), menu_list.arrow_y,
                    arrow_width, arrow_height);
    arrow.Intersect(rect);

    if (arrow_width != arrow.width() || arrow_height != arrow.height()) {
      // The arrow is clipped after being constrained to the paint rect so we
      // need to recalculate its size.
      int height_clip = arrow_height - arrow.height();
      int width_clip = arrow_width - arrow.width();
      if (height_clip > width_clip) {
        arrow.set_width(arrow.height() * 0.6);
      } else {
        arrow.set_height(arrow.width() * 1.6);
      }
      arrow.set_x(menu_list.arrow_x - (arrow.width() / 2));
    }

    SkPath path;
    if (menu_list.arrow_direction == ui::NativeTheme::ArrowDirection::kLeft) {
      path.moveTo(arrow.x() + arrow.width(), arrow.y());
      path.lineTo(arrow.x(), arrow.y() + arrow.height() / 2);
      path.lineTo(arrow.x() + arrow.width(), arrow.y() + arrow.height());
    } else {
      // Arrow direction is right
      path.moveTo(arrow.x(), arrow.y());
      path.lineTo(arrow.x() + arrow.width(), arrow.y() + arrow.height() / 2);
      path.lineTo(arrow.x(), arrow.y() + arrow.height());
    }
    canvas->drawPath(path, flags);
  }
}

void NativeThemeBase::PaintMenuPopupBackground(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background,
    ColorScheme color_scheme) const {
  // We are currently only painting kMenuPopupBackground with the kDefault
  // scheme. If that changes, we need to add an appropriate dark scheme color to
  // kMenuPopupBackgroundColor.
  DCHECK(color_scheme == ColorScheme::kDefault);

  // TODO(crbug.com/40219248): Remove FromColor and make all SkColor4f.
  canvas->drawColor(
      SkColor4f::FromColor(GetColor(kMenuPopupBackgroundColor, color_scheme)),
      SkBlendMode::kSrc);
}

void NativeThemeBase::PaintMenuItemBackground(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const MenuItemExtraParams& menu_item,
    ColorScheme color_scheme) const {
  // By default don't draw anything over the normal background.
}

void NativeThemeBase::PaintMenuSeparator(
    cc::PaintCanvas* canvas,
    const ui::ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const MenuSeparatorExtraParams& menu_separator) const {
  DCHECK(color_provider);
  cc::PaintFlags flags;
  flags.setColor(color_provider->GetColor(menu_separator.color_id));
  canvas->drawRect(gfx::RectToSkRect(*menu_separator.paint_rect), flags);
}

void NativeThemeBase::PaintSliderTrack(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const SliderExtraParams& slider,
    ColorScheme color_scheme,
    const std::optional<SkColor>& accent_color) const {
  // Paint the entire slider track.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(
      ControlsFillColorForState(state, color_scheme, color_provider));
  const float track_height = kSliderTrackHeight * slider.zoom;
  SkRect track_rect = AlignSliderTrack(rect, slider, false, track_height);
  float border_width = AdjustBorderWidthByZoom(kBorderWidth, slider.zoom);
  // Shrink the track by 1 pixel so the thumb can completely cover the track
  // on both ends.
  if (slider.vertical)
    track_rect.inset(0, 1);
  else
    track_rect.inset(1, 0);
  float border_radius =
      GetBorderRadiusForPart(kSliderTrack, rect.width(), rect.height());
  canvas->drawRoundRect(track_rect, border_radius, border_radius, flags);

  // Set the clip to the extent of the value bar.
  SkRect value_rect = AlignSliderTrack(rect, slider, true, track_height);
  canvas->save();
  canvas->clipRect(value_rect, SkClipOp::kIntersect, true);

  // Draw the full value bar, clipped to its extent.
  if (accent_color && state != kDisabled) {
    flags.setColor(
        CustomAccentColorForState(*accent_color, state, color_scheme));
  } else {
    flags.setColor(
        ControlsSliderColorForState(state, color_scheme, color_provider));
  }
  SkRRect rounded_rect;
  rounded_rect.setRectXY(track_rect, border_radius, border_radius);
  canvas->drawRRect(rounded_rect, flags);
  canvas->restore();

  // Paint the border.
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(border_width);
  SkColor border_color =
      ControlsBorderColorForState(state, color_scheme, color_provider);
  if (!UserHasContrastPreference() && state != kDisabled &&
      color_scheme != ColorScheme::kDark)
    border_color = SkColorSetA(border_color, 0x80);
  flags.setColor(border_color);
  track_rect.inset(border_width / 2, border_width / 2);
  canvas->drawRoundRect(track_rect, border_radius, border_radius, flags);
}

void NativeThemeBase::PaintSliderThumb(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const SliderExtraParams& slider,
    ColorScheme color_scheme,
    const std::optional<SkColor>& accent_color) const {
  const float radius =
      GetBorderRadiusForPart(kSliderThumb, rect.width(), rect.height());
  SkRect thumb_rect = gfx::RectToSkRect(rect);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  SkScalar border_width = kSliderThumbBorderWidth;
  if (state == kHovered || state == kPressed) {
    border_width = kSliderThumbBorderHoveredWidth;
  }

  // Paint the background (is not visible behind the rounded corners).
  thumb_rect.inset(border_width / 2, border_width / 2);
  if (accent_color && state != kDisabled) {
    flags.setColor(
        CustomAccentColorForState(*accent_color, state, color_scheme));
  } else {
    flags.setColor(
        ControlsSliderColorForState(state, color_scheme, color_provider));
  }
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->drawRoundRect(thumb_rect, radius, radius, flags);
}

void NativeThemeBase::PaintInnerSpinButton(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const InnerSpinButtonExtraParams& spin_button,
    ColorScheme color_scheme,
    bool in_forced_colors) const {
  if (spin_button.read_only)
    state = kDisabled;

  State north_state = state;
  State south_state = state;
  if (spin_button.spin_up)
    south_state = south_state != kDisabled ? kNormal : kDisabled;
  else
    north_state = north_state != kDisabled ? kNormal : kDisabled;

  gfx::Rect half = rect;
  ScrollbarArrowExtraParams arrow = ScrollbarArrowExtraParams();
  arrow.zoom = 1.0;
  if (spin_button.spin_arrows_direction ==
      ui::NativeTheme::SpinArrowsDirection::kUpDown) {
    half.set_height(rect.height() / 2);
    PaintArrowButton(canvas, color_provider, half, kScrollbarUpArrow,
                     north_state, color_scheme, in_forced_colors, arrow);

    half.set_y(rect.y() + rect.height() / 2);
    PaintArrowButton(canvas, color_provider, half, kScrollbarDownArrow,
                     south_state, color_scheme, in_forced_colors, arrow);
  } else {
    half.set_width(rect.width() / 2);
    PaintArrowButton(canvas, color_provider, half, kScrollbarLeftArrow,
                     south_state, color_scheme, in_forced_colors, arrow);

    half.set_x(rect.x() + rect.width() / 2);
    PaintArrowButton(canvas, color_provider, half, kScrollbarRightArrow,
                     north_state, color_scheme, in_forced_colors, arrow);
  }
}

void NativeThemeBase::PaintProgressBar(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const ProgressBarExtraParams& progress_bar,
    ColorScheme color_scheme,
    const std::optional<SkColor>& accent_color) const {
  DCHECK(!rect.IsEmpty());
  // Paint the track.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(GetControlColor(kFill, color_scheme, color_provider));
  SliderExtraParams slider;
  float track_block_thickness = rect.height();
  if (progress_bar.is_horizontal) {
    slider.vertical = false;
    track_block_thickness = rect.height() * kTrackBlockRatio;
  } else {
    slider.vertical = true;
    track_block_thickness = rect.width() * kTrackBlockRatio;
  }
  SkRect track_rect =
      AlignSliderTrack(rect, slider, false, track_block_thickness);
  float border_radius =
      GetBorderRadiusForPart(kProgressBar, rect.width(), rect.height());
  canvas->drawRoundRect(track_rect, border_radius, border_radius, flags);

  // Clip the track to create rounded corners for the value bar.
  SkRRect rounded_rect;
  rounded_rect.setRectXY(track_rect, border_radius, border_radius);
  canvas->clipRRect(rounded_rect, SkClipOp::kIntersect, true);

  // Paint the progress value bar.
  const SkScalar kMinimumProgressInlineValue = 2;
  SkScalar adjusted_height = progress_bar.value_rect_height;
  SkScalar adjusted_width = progress_bar.value_rect_width;
  // If adjusted thickness is not zero, make sure it is equal or larger than
  // kMinimumProgressInlineValue.
  if (slider.vertical) {
    if (adjusted_height > 0)
      adjusted_height = std::max(kMinimumProgressInlineValue, adjusted_height);
  } else {
    if (adjusted_width > 0)
      adjusted_width = std::max(kMinimumProgressInlineValue, adjusted_width);
  }
  gfx::Rect original_value_rect(progress_bar.value_rect_x,
                                progress_bar.value_rect_y, adjusted_width,
                                adjusted_height);
  SkRect value_rect = AlignSliderTrack(original_value_rect, slider, false,
                                       track_block_thickness);
  if (accent_color) {
    flags.setColor(*accent_color);
  } else {
    flags.setColor(GetControlColor(kAccent, color_scheme, color_provider));
  }
  if (progress_bar.determinate) {
    canvas->drawRect(value_rect, flags);
  } else {
    canvas->drawRoundRect(value_rect, border_radius, border_radius, flags);
  }

  // Paint the border.
  float border_width = AdjustBorderWidthByZoom(kBorderWidth, progress_bar.zoom);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(border_width);
  SkColor border_color = GetControlColor(kBorder, color_scheme, color_provider);
  if (!UserHasContrastPreference() && color_scheme != ColorScheme::kDark)
    border_color = SkColorSetA(border_color, 0x80);
  flags.setColor(border_color);
  track_rect.inset(border_width / 2, border_width / 2);
  canvas->drawRoundRect(track_rect, border_radius, border_radius, flags);
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
  color[1] = std::clamp(hsv[1] + saturate_amount, SkScalar{0}, SK_Scalar1);
  color[2] = std::clamp(hsv[2] + brighten_amount, SkScalar{0}, SK_Scalar1);
  return SkHSVToColor(color);
}

SkColor NativeThemeBase::GetArrowColor(
    State state,
    ColorScheme color_scheme,
    const ColorProvider* color_provider) const {
  if (state != kDisabled)
    return GetColor(kArrowDisabledColor, color_scheme);

  SkScalar track_hsv[3];
  SkColorToHSV(GetColor(kTrackColor, color_scheme), track_hsv);

  SkScalar thumb_hsv[3];
  SkColorToHSV(
      GetControlColor(kScrollbarThumbInactive, color_scheme, color_provider),
      thumb_hsv);
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
  SkScalar min_diff = std::clamp((hsv1[1] + hsv2[1]) * 1.2f, 0.28f, 0.5f);
  SkScalar diff = std::clamp(fabsf(hsv1[2] - hsv2[2]) / 2, min_diff, 0.5f);

  if (hsv1[2] + hsv2[2] > 1.0)
    diff = -diff;

  return SaturateAndBrighten(hsv2, -0.2f, diff);
}

SkColor NativeThemeBase::ControlsAccentColorForState(
    State state,
    ColorScheme color_scheme,
    const ColorProvider* color_provider) const {
  ControlColorId color_id;
  if (state == kHovered) {
    color_id = kHoveredAccent;
  } else if (state == kPressed) {
    color_id = kPressedAccent;
  } else if (state == kDisabled) {
    color_id = kDisabledAccent;
  } else {
    color_id = kAccent;
  }
  return GetControlColor(color_id, color_scheme, color_provider);
}

SkColor NativeThemeBase::ControlsSliderColorForState(
    State state,
    ColorScheme color_scheme,
    const ColorProvider* color_provider) const {
  ControlColorId color_id;
  if (state == kHovered) {
    color_id = kHoveredSlider;
  } else if (state == kPressed) {
    color_id = kPressedSlider;
  } else if (state == kDisabled) {
    color_id = kDisabledSlider;
  } else {
    color_id = kSlider;
  }
  return GetControlColor(color_id, color_scheme, color_provider);
}

SkColor NativeThemeBase::ControlsBorderColorForState(
    State state,
    ColorScheme color_scheme,
    const ColorProvider* color_provider) const {
  ControlColorId color_id;
  if (state == kHovered) {
    color_id = kHoveredBorder;
  } else if (state == kPressed) {
    color_id = kPressedBorder;
  } else if (state == kDisabled) {
    color_id = kDisabledBorder;
  } else {
    color_id = kBorder;
  }
  return GetControlColor(color_id, color_scheme, color_provider);
}

SkColor NativeThemeBase::ButtonBorderColorForState(
    State state,
    ColorScheme color_scheme,
    const ColorProvider* color_provider) const {
  ControlColorId color_id;
  if (state == kHovered) {
    color_id = kButtonHoveredBorder;
  } else if (state == kPressed) {
    color_id = kButtonPressedBorder;
  } else if (state == kDisabled) {
    color_id = kButtonDisabledBorder;
  } else {
    color_id = kButtonBorder;
  }
  return GetControlColor(color_id, color_scheme, color_provider);
}

SkColor NativeThemeBase::ControlsFillColorForState(
    State state,
    ColorScheme color_scheme,
    const ColorProvider* color_provider) const {
  ControlColorId color_id;
  if (state == kHovered) {
    color_id = kHoveredFill;
  } else if (state == kPressed) {
    color_id = kPressedFill;
  } else if (state == kDisabled) {
    color_id = kDisabledFill;
  } else {
    color_id = kFill;
  }
  return GetControlColor(color_id, color_scheme, color_provider);
}

SkColor NativeThemeBase::ButtonFillColorForState(
    State state,
    ColorScheme color_scheme,
    const ColorProvider* color_provider) const {
  ControlColorId color_id;
  if (state == kHovered) {
    color_id = kButtonHoveredFill;
  } else if (state == kPressed) {
    color_id = kButtonPressedFill;
  } else if (state == kDisabled) {
    color_id = kButtonDisabledFill;
  } else {
    color_id = kButtonFill;
  }
  return GetControlColor(color_id, color_scheme, color_provider);
}

SkColor NativeThemeBase::ControlsBackgroundColorForState(
    State state,
    ColorScheme color_scheme,
    const ColorProvider* color_provider) const {
  ControlColorId color_id;
  if (state == kDisabled) {
    color_id = kDisabledBackground;
  } else {
    color_id = kBackground;
  }
  return GetControlColor(color_id, color_scheme, color_provider);
}

SkColor NativeThemeBase::GetControlColor(
    ControlColorId color_id,
    ColorScheme color_scheme,
    const ColorProvider* color_provider) const {
  if (IsColorPipelineSupportedForControlColorId(color_provider, color_id))
    return GetControlColorFromColorProvider(color_id, color_provider);

  if (color_scheme == ColorScheme::kDark)
    return GetDarkModeControlColor(color_id);

  switch (color_id) {
    case kBorder:
    case kButtonBorder:
      return SkColorSetRGB(0x76, 0x76, 0x76);
    case kHoveredBorder:
    case kButtonHoveredBorder:
      return SkColorSetRGB(0x4F, 0x4F, 0x4F);
    case kPressedBorder:
    case kButtonPressedBorder:
      return SkColorSetRGB(0x8D, 0x8D, 0x8D);
    case kDisabledBorder:
    case kButtonDisabledBorder:
      return SkColorSetARGB(0x4D, 0x76, 0x76, 0x76);
    case kAccent:
      return SkColorSetRGB(0x00, 0x75, 0xFF);
    case kHoveredAccent:
      return SkColorSetRGB(0x00, 0x5C, 0xC8);
    case kPressedAccent:
      return SkColorSetRGB(0x37, 0x93, 0xFF);
    case kDisabledAccent:
      return SkColorSetARGB(0x4D, 0x76, 0x76, 0x76);
    case kBackground:
      return SK_ColorWHITE;
    case kDisabledBackground:
      return SkColorSetA(SK_ColorWHITE, 0x99);
    case kFill:
    case kButtonFill:
      return SkColorSetRGB(0xEF, 0xEF, 0xEF);
    case kHoveredFill:
    case kButtonHoveredFill:
      return SkColorSetRGB(0xE5, 0xE5, 0xE5);
    case kPressedFill:
    case kButtonPressedFill:
      return SkColorSetRGB(0xF5, 0xF5, 0xF5);
    case kDisabledFill:
    case kButtonDisabledFill:
      return SkColorSetARGB(0x4D, 0xEF, 0xEF, 0xEF);
    case kLightenLayer:
      return SkColorSetARGB(0x33, 0xA9, 0xA9, 0xA9);
    case kProgressValue:
      return SkColorSetRGB(0x00, 0x75, 0xFF);
    case kSlider:
      return SkColorSetRGB(0x00, 0x75, 0xFF);
    case kHoveredSlider:
      return SkColorSetRGB(0x00, 0x5C, 0xC8);
    case kPressedSlider:
      return SkColorSetRGB(0x37, 0x93, 0xFF);
    case kDisabledSlider:
      return SkColorSetRGB(0xCB, 0xCB, 0xCB);
    case kAutoCompleteBackground:
      return SkColorSetRGB(0xE8, 0xF0, 0xFE);
    case kScrollbarArrowBackground:
    case kScrollbarTrack:
      return SkColorSetRGB(0xF1, 0xF1, 0xF1);
    case kScrollbarArrowBackgroundHovered:
      return SkColorSetRGB(0xD2, 0xD2, 0xD2);
    case kScrollbarArrowBackgroundPressed:
      return SkColorSetRGB(0x78, 0x78, 0x78);
    case kScrollbarArrowHovered:
    case kScrollbarArrow:
      return SkColorSetRGB(0x50, 0x50, 0x50);
    case kScrollbarArrowPressed:
      return SK_ColorWHITE;
    case kScrollbarCornerControlColorId:
      return SkColorSetRGB(0xDC, 0xDC, 0xDC);
    case kScrollbarThumbInactive:
      return SkColorSetRGB(0xEA, 0xEA, 0xEA);
    case kScrollbarThumbHovered:
      return SkColorSetA(SK_ColorBLACK, 0x4D);
    case kScrollbarThumbPressed:
      return SkColorSetA(SK_ColorBLACK, 0x80);
    case kScrollbarThumb:
      return SkColorSetA(SK_ColorBLACK, 0x33);
  }
  NOTREACHED_IN_MIGRATION();
  return gfx::kPlaceholderColor;
}

SkColor NativeThemeBase::GetDarkModeControlColor(
    ControlColorId color_id) const {
  switch (color_id) {
    case kAccent:
      return SkColorSetRGB(0x99, 0xC8, 0xFF);
    case kHoveredAccent:
      return SkColorSetRGB(0xD1, 0xE6, 0xFF);
    case kPressedAccent:
      return SkColorSetRGB(0x61, 0xA9, 0xFF);
    case kDisabledAccent:
      return SkColorSetRGB(0x75, 0x75, 0x75);
    case kProgressValue:
      return SkColorSetRGB(0x63, 0xAD, 0xE5);
    case kFill:
      return SkColorSetRGB(0x3B, 0x3B, 0x3B);
    case kButtonBorder:
    case kButtonFill:
      return SkColorSetRGB(0x6B, 0x6B, 0x6B);
    case kAutoCompleteBackground:
      return SkColorSetARGB(0x66, 0x46, 0x5a, 0x7e);
    case kLightenLayer:
    case kBackground:
      return SkColorSetRGB(0x3B, 0x3B, 0x3B);
    case kBorder:
      return SkColorSetRGB(0x85, 0x85, 0x85);
    case kSlider:
      return SkColorSetRGB(0x99, 0xC8, 0xFF);
    case kHoveredSlider:
      return SkColorSetRGB(0xD1, 0xE6, 0xFF);
    case kPressedSlider:
      return SkColorSetRGB(0x61, 0xA9, 0xFF);
    case kDisabledSlider:
      return SkColorSetRGB(0x75, 0x75, 0x75);
    case kDisabledBackground:
      return SkColorSetRGB(0x3B, 0x3B, 0x3B);
    case kHoveredBorder:
      return SkColorSetRGB(0xAC, 0xAC, 0xAC);
    case kPressedBorder:
      return SkColorSetRGB(0x6E, 0x6E, 0x6E);
    case kDisabledBorder:
      return SkColorSetRGB(0x62, 0x62, 0x62);
    case kHoveredFill:
      return SkColorSetRGB(0x3B, 0x3B, 0x3B);
    case kButtonHoveredBorder:
    case kButtonHoveredFill:
      return SkColorSetRGB(0x7B, 0x7B, 0x7B);
    case kPressedFill:
      return SkColorSetRGB(0x3B, 0x3B, 0x3B);
    case kButtonPressedBorder:
    case kButtonPressedFill:
      return SkColorSetRGB(0x61, 0x61, 0x61);
    case kDisabledFill:
    case kButtonDisabledBorder:
    case kButtonDisabledFill:
      return SkColorSetRGB(0x36, 0x36, 0x36);
    case kScrollbarArrowBackground:
      return SkColorSetRGB(0x42, 0x42, 0x42);
    case kScrollbarArrowBackgroundHovered:
      return SkColorSetRGB(0x4F, 0x4F, 0x4F);
    case kScrollbarArrowBackgroundPressed:
      return SkColorSetRGB(0xB1, 0xB1, 0xB1);
    case kScrollbarArrowHovered:
    case kScrollbarArrow:
      return SK_ColorWHITE;
    case kScrollbarArrowPressed:
      return SK_ColorBLACK;
    case kScrollbarCornerControlColorId:
      return SkColorSetRGB(0x12, 0x12, 0x12);
    case kScrollbarTrack:
      return SkColorSetRGB(0x42, 0x42, 0x42);
    case kScrollbarThumbInactive:
      return SK_ColorWHITE;
    case kScrollbarThumbHovered:
      return SkColorSetA(SK_ColorWHITE, 0x4D);
    case kScrollbarThumbPressed:
      return SkColorSetA(SK_ColorWHITE, 0x80);
    case kScrollbarThumb:
      return SkColorSetA(SK_ColorWHITE, 0x33);
  }
  NOTREACHED_IN_MIGRATION();
  return gfx::kPlaceholderColor;
}

SkColor NativeThemeBase::GetControlColorFromColorProvider(
    ControlColorId color_id,
    const ColorProvider* color_provider) const {
  DCHECK(IsColorPipelineSupportedForControlColorId(color_provider, color_id));
  switch (color_id) {
    case kBorder:
      return color_provider->GetColor(kColorWebNativeControlBorder);
    case kDisabledBorder:
      return color_provider->GetColor(kColorWebNativeControlBorderDisabled);
    case kHoveredBorder:
      return color_provider->GetColor(kColorWebNativeControlBorderHovered);
    case kPressedBorder:
      return color_provider->GetColor(kColorWebNativeControlBorderPressed);
    case kAccent:
      return color_provider->GetColor(kColorWebNativeControlAccent);
    case kDisabledAccent:
      return color_provider->GetColor(kColorWebNativeControlAccentDisabled);
    case kHoveredAccent:
      return color_provider->GetColor(kColorWebNativeControlAccentHovered);
    case kPressedAccent:
      return color_provider->GetColor(kColorWebNativeControlAccentPressed);
    case kBackground:
      return color_provider->GetColor(kColorWebNativeControlBackground);
    case kDisabledBackground:
      return color_provider->GetColor(kColorWebNativeControlBackgroundDisabled);
    case kFill:
      return color_provider->GetColor(kColorWebNativeControlFill);
    case kDisabledFill:
      return color_provider->GetColor(kColorWebNativeControlFillDisabled);
    case kHoveredFill:
      return color_provider->GetColor(kColorWebNativeControlFillHovered);
    case kPressedFill:
      return color_provider->GetColor(kColorWebNativeControlFillPressed);
    case kLightenLayer:
      return color_provider->GetColor(kColorWebNativeControlLightenLayer);
    case kProgressValue:
      return color_provider->GetColor(kColorWebNativeControlProgressValue);
    case kSlider:
      return color_provider->GetColor(kColorWebNativeControlSlider);
    case kDisabledSlider:
      return color_provider->GetColor(kColorWebNativeControlSliderDisabled);
    case kHoveredSlider:
      return color_provider->GetColor(kColorWebNativeControlSliderHovered);
    case kPressedSlider:
      return color_provider->GetColor(kColorWebNativeControlSliderPressed);
    case kAutoCompleteBackground:
      return color_provider->GetColor(
          kColorWebNativeControlAutoCompleteBackground);
    case kScrollbarArrowBackground:
    case kScrollbarTrack:
      return color_provider->GetColor(kColorWebNativeControlScrollbarTrack);
    case kScrollbarArrowBackgroundHovered:
      return color_provider->GetColor(
          kColorWebNativeControlScrollbarArrowBackgroundHovered);
    case kScrollbarArrowBackgroundPressed:
      return color_provider->GetColor(
          kColorWebNativeControlScrollbarArrowBackgroundPressed);
    case kScrollbarArrow:
    case kScrollbarArrowHovered:
      return color_provider->GetColor(
          kColorWebNativeControlScrollbarArrowForeground);
    case kScrollbarArrowPressed:
      return color_provider->GetColor(
          kColorWebNativeControlScrollbarArrowForegroundPressed);
    case kScrollbarCornerControlColorId:
      return color_provider->GetColor(kColorWebNativeControlScrollbarCorner);
    case kScrollbarThumb:
      return color_provider->GetColor(kColorWebNativeControlScrollbarThumb);
    case kScrollbarThumbHovered:
      return color_provider->GetColor(
          kColorWebNativeControlScrollbarThumbHovered);
    case kScrollbarThumbInactive:
      return color_provider->GetColor(
          kColorWebNativeControlScrollbarThumbInactive);
    case kScrollbarThumbPressed:
      return color_provider->GetColor(
          kColorWebNativeControlScrollbarThumbPressed);
    case kButtonBorder:
      return color_provider->GetColor(kColorWebNativeControlButtonBorder);
    case kButtonDisabledBorder:
      return color_provider->GetColor(
          kColorWebNativeControlButtonBorderDisabled);
    case kButtonHoveredBorder:
      return color_provider->GetColor(
          kColorWebNativeControlButtonBorderHovered);
    case kButtonPressedBorder:
      return color_provider->GetColor(
          kColorWebNativeControlButtonBorderPressed);
    case kButtonFill:
      return color_provider->GetColor(kColorWebNativeControlButtonFill);
    case kButtonDisabledFill:
      return color_provider->GetColor(kColorWebNativeControlButtonFillDisabled);
    case kButtonHoveredFill:
      return color_provider->GetColor(kColorWebNativeControlButtonFillHovered);
    case kButtonPressedFill:
      return color_provider->GetColor(kColorWebNativeControlButtonFillPressed);
    default:
      break;
  }
  NOTREACHED();
}

void NativeThemeBase::PaintLightenLayer(cc::PaintCanvas* canvas,
                                        const ColorProvider* color_provider,
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
    flags.setColor(
        GetControlColor(kLightenLayer, color_scheme, color_provider));
    canvas->drawRoundRect(skrect, border_radius, border_radius, flags);
  }
}

SkRect NativeThemeBase::AlignSliderTrack(
    const gfx::Rect& slider_rect,
    const NativeTheme::SliderExtraParams& slider,
    bool is_value,
    float track_block_thickness) const {
  const float kAlignment = track_block_thickness / 2;
  const float mid_x = slider_rect.x() + slider_rect.width() / 2.0f;
  const float mid_y = slider_rect.y() + slider_rect.height() / 2.0f;
  SkRect aligned_rect;

  if (slider.vertical) {
    const float top = is_value && slider.right_to_left
                          ? slider_rect.y() + slider.thumb_y + kAlignment
                          : slider_rect.y();
    const float bottom = is_value && !slider.right_to_left
                             ? slider_rect.y() + slider.thumb_y + kAlignment
                             : slider_rect.bottom();
    aligned_rect.setLTRB(
        std::max(float(slider_rect.x()), mid_x - kAlignment), top,
        std::min(float(slider_rect.right()), mid_x + kAlignment), bottom);
  } else {
    const float right = is_value && !slider.right_to_left
                            ? slider_rect.x() + slider.thumb_x + kAlignment
                            : slider_rect.right();
    const float left = is_value && slider.right_to_left
                           ? slider_rect.x() + slider.thumb_x + kAlignment
                           : slider_rect.x();

    aligned_rect.setLTRB(
        left, std::max(float(slider_rect.y()), mid_y - kAlignment), right,
        std::min(float(slider_rect.bottom()), mid_y + kAlignment));
  }

  return aligned_rect;
}

bool NativeThemeBase::IsColorPipelineSupportedForControlColorId(
    const ColorProvider* color_provider,
    ControlColorId color_id) const {
  // Color providers are not yet supported on Android so we need to check that
  // the color_provider is not null here.
  if (!color_provider) {
    return false;
  }

  static constexpr auto kControlColorIdsSet =
      base::MakeFixedFlatSet<ControlColorId>({kBorder,
                                              kDisabledBorder,
                                              kHoveredBorder,
                                              kPressedBorder,
                                              kAccent,
                                              kDisabledAccent,
                                              kHoveredAccent,
                                              kPressedAccent,
                                              kBackground,
                                              kDisabledBackground,
                                              kFill,
                                              kDisabledFill,
                                              kHoveredFill,
                                              kPressedFill,
                                              kLightenLayer,
                                              kProgressValue,
                                              kSlider,
                                              kDisabledSlider,
                                              kHoveredSlider,
                                              kPressedSlider,
                                              kAutoCompleteBackground,
                                              kScrollbarArrowBackground,
                                              kScrollbarArrowBackgroundHovered,
                                              kScrollbarArrowBackgroundPressed,
                                              kScrollbarArrow,
                                              kScrollbarArrowHovered,
                                              kScrollbarArrowPressed,
                                              kScrollbarCornerControlColorId,
                                              kScrollbarTrack,
                                              kScrollbarThumb,
                                              kScrollbarThumbHovered,
                                              kScrollbarThumbPressed,
                                              kScrollbarThumbInactive,
                                              kButtonBorder,
                                              kButtonDisabledBorder,
                                              kButtonHoveredBorder,
                                              kButtonPressedBorder,
                                              kButtonFill,
                                              kButtonDisabledFill,
                                              kButtonHoveredFill,
                                              kButtonPressedFill});
  return kControlColorIdsSet.contains(color_id);
}

}  // namespace ui
