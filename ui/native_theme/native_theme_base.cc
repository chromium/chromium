// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_base.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/native_theme/features/native_theme_features.h"
#include "ui/native_theme/native_theme.h"

namespace ui {

namespace {

static constexpr gfx::Size kCheckboxSize(13, 13);
static constexpr int kSliderTrackThickness = 8;
static constexpr int kSliderThumbThickness = 16;
static constexpr float kBorderWidth = 1.0f;

// Get a color constant based on color-scheme
// TODO(crbug.com/40242489): Move colors defined above to the color pipeline and
// remove this function.
SkColor GetColor(base::span<const SkColor, 2> colors, bool dark_mode) {
  return colors[dark_mode ? 1 : 0];
}

SkColor CustomAccentColorForState(const SkColor& color,
                                  ui::NativeTheme::State state,
                                  bool dark_mode) {
  bool make_lighter = false;
  switch (state) {
    case ui::NativeTheme::kHovered:
      make_lighter = dark_mode;
      break;
    case ui::NativeTheme::kPressed:
      make_lighter = !dark_mode;
      break;
    default:
      return color;
  }
  // Approximates the lightness difference between `kAccent` and
  // `kHoveredAccent`.
  static constexpr double kLightnessAdjust = 0.11;
  double l_adjust = (make_lighter ? 1 : -1) * kLightnessAdjust;

  color_utils::HSL hsl;
  color_utils::SkColorToHSL(color, &hsl);
  hsl.l = std::clamp(hsl.l + l_adjust, 0.0, 1.0);
  return color_utils::HSLToSkColor(hsl, SkColorGetA(color));
}

SkRect AlignSliderTrack(const gfx::Rect& slider_rect,
                        const NativeTheme::SliderExtraParams& extra_params,
                        bool is_value,
                        float thickness) {
  const gfx::RectF r(slider_rect);
  const gfx::PointF center = r.CenterPoint();
  const float half_track_thickness = thickness / 2;

  if (extra_params.vertical) {
    const float top = is_value && extra_params.right_to_left
                          ? r.y() + extra_params.thumb_y + half_track_thickness
                          : r.y();
    const float bottom =
        is_value && !extra_params.right_to_left
            ? r.y() + extra_params.thumb_y + half_track_thickness
            : r.bottom();
    return SkRect::MakeLTRB(
        std::max(r.x(), center.x() - half_track_thickness), top,
        std::min(r.right(), center.x() + half_track_thickness), bottom);
  }

  const float left = is_value && extra_params.right_to_left
                         ? r.x() + extra_params.thumb_x + half_track_thickness
                         : r.x();
  const float right = is_value && !extra_params.right_to_left
                          ? r.x() + extra_params.thumb_x + half_track_thickness
                          : r.right();
  return SkRect::MakeLTRB(
      left, std::max(r.y(), center.y() - half_track_thickness), right,
      std::min(r.bottom(), center.y() + half_track_thickness));
}

}  // namespace

gfx::Size NativeThemeBase::GetPartSize(Part part,
                                       State state,
                                       const ExtraParams& extra_params) const {
  switch (part) {
    case kCheckbox:
    case kRadio:
      return kCheckboxSize;
    case kSliderThumb:
      return gfx::Size(kSliderThumbThickness, kSliderThumbThickness);
    case kInnerSpinButton:
      return gfx::Size(scrollbar_width_, 0);
    case kScrollbarDownArrow:
    case kScrollbarUpArrow:
      return gfx::Size(scrollbar_width_, scrollbar_button_length_);
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
      return gfx::Size(scrollbar_button_length_, scrollbar_width_);
    case kScrollbarHorizontalThumb:
      return gfx::Size(2 * scrollbar_width_, scrollbar_width_);
    case kScrollbarVerticalThumb:
      return gfx::Size(scrollbar_width_, 2 * scrollbar_width_);
    case kScrollbarHorizontalTrack:
      return gfx::Size(0, scrollbar_width_);
    case kScrollbarVerticalTrack:
      return gfx::Size(scrollbar_width_, 0);
    default:
      return gfx::Size();  // No default size.
  }
}

float NativeThemeBase::GetBorderRadiusForPart(Part part,
                                              float width,
                                              float height) const {
  switch (part) {
    case kCheckbox:
    case kPushButton:
    case kTextField:
      return 2.0f;
    case kProgressBar:
    case kSliderTrack:
      // In the common case, the thickness is small enough that this has the
      // same effect as the radio/slider thumb code below.
      return 40.0f;
    case kRadio:
    case kSliderThumb:
      return std::max(width, height) * 0.5;
    default:
      return 0;
  }
}

void NativeThemeBase::PaintImpl(cc::PaintCanvas* canvas,
                                const ColorProvider* color_provider,
                                Part part,
                                State state,
                                const gfx::Rect& rect,
                                const ExtraParams& extra_params,
                                bool forced_colors,
                                bool dark_mode,
                                PreferredContrast contrast,
                                std::optional<SkColor> accent_color) const {
  switch (part) {
    case kCheckbox:
      PaintCheckbox(canvas, color_provider, state, rect,
                    std::get<ButtonExtraParams>(extra_params), dark_mode,
                    accent_color);
      break;
#if BUILDFLAG(IS_LINUX)
    case kFrameTopArea:
      PaintFrameTopArea(canvas, state, rect,
                        std::get<FrameTopAreaExtraParams>(extra_params));
      break;
#endif
    case kInnerSpinButton:
      PaintInnerSpinButton(canvas, color_provider, state, rect,
                           std::get<InnerSpinButtonExtraParams>(extra_params),
                           forced_colors, dark_mode, contrast);
      break;
    case kMenuList:
      PaintMenuList(canvas, color_provider, state, rect,
                    std::get<MenuListExtraParams>(extra_params), dark_mode);
      break;
    case kMenuPopupBackground:
      PaintMenuPopupBackground(
          canvas, color_provider, rect.size(),
          std::get<MenuBackgroundExtraParams>(extra_params));
      break;
    case kMenuPopupSeparator:
      PaintMenuSeparator(canvas, color_provider, state, rect,
                         std::get<MenuSeparatorExtraParams>(extra_params));
      break;
    case kMenuItemBackground:
      PaintMenuItemBackground(canvas, color_provider, state, rect,
                              std::get<MenuItemExtraParams>(extra_params));
      break;
    case kProgressBar:
      PaintProgressBar(canvas, color_provider, state, rect,
                       std::get<ProgressBarExtraParams>(extra_params),
                       dark_mode, contrast, accent_color);
      break;
    case kPushButton:
      PaintButton(canvas, color_provider, state, rect,
                  std::get<ButtonExtraParams>(extra_params), dark_mode);
      break;
    case kRadio:
      PaintRadio(canvas, color_provider, state, rect,
                 std::get<ButtonExtraParams>(extra_params), dark_mode,
                 accent_color);
      break;
    case kScrollbarDownArrow:
    case kScrollbarUpArrow:
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
      if (scrollbar_button_length_ > 0) {
        PaintArrowButton(canvas, color_provider, rect, part, state,
                         forced_colors, dark_mode, contrast,
                         std::get<ScrollbarArrowExtraParams>(extra_params));
      }
      break;
    case kScrollbarHorizontalThumb:
    case kScrollbarVerticalThumb:
      PaintScrollbarThumb(canvas, color_provider, part, state, rect,
                          std::get<ScrollbarThumbExtraParams>(extra_params));
      break;
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack:
      PaintScrollbarTrack(canvas, color_provider, part, state,
                          std::get<ScrollbarTrackExtraParams>(extra_params),
                          rect, forced_colors, contrast);
      break;
    case kScrollbarHorizontalGripper:
    case kScrollbarVerticalGripper:
      // Paints nothing in this or any subclass.
      break;
    case kScrollbarCorner:
      PaintScrollbarCorner(canvas, color_provider, state, rect,
                           std::get<ScrollbarTrackExtraParams>(extra_params));
      break;
    case kSliderTrack:
      PaintSliderTrack(canvas, color_provider, state, rect,
                       std::get<SliderExtraParams>(extra_params), dark_mode,
                       contrast, accent_color);
      break;
    case kSliderThumb:
      PaintSliderThumb(canvas, color_provider, state, rect,
                       std::get<SliderExtraParams>(extra_params), dark_mode,
                       accent_color);
      break;
    case kTextField:
      PaintTextField(canvas, color_provider, state, rect,
                     std::get<TextFieldExtraParams>(extra_params), dark_mode);
      break;
    case kTabPanelBackground:
    case kTrackbarThumb:
    case kTrackbarTrack:
    case kWindowResizeGripper:
      NOTIMPLEMENTED();
      break;
    default:
      NOTREACHED();
  }
}

NativeThemeBase::~NativeThemeBase() = default;

void NativeThemeBase::AdjustCheckboxRadioRectForPadding(SkRect* rect) const {
  // By default we only take 1px from right and bottom for the drop shadow.
  rect->setLTRB(static_cast<int>(rect->x()), static_cast<int>(rect->y()),
                static_cast<int>(rect->right()) - 1,
                static_cast<int>(rect->bottom()) - 1);
}

SkColor NativeThemeBase::GetControlColor(
    ControlColorId color_id,
    bool dark_mode,
    const ColorProvider* color_provider) const {
  static constexpr auto kColorMap = base::MakeFixedFlatMap<ControlColorId,
                                                           ColorId>(
      {{kBorder, kColorWebNativeControlBorder},
       {kDisabledBorder, kColorWebNativeControlBorderDisabled},
       {kHoveredBorder, kColorWebNativeControlBorderHovered},
       {kPressedBorder, kColorWebNativeControlBorderPressed},
       {kAccent, kColorWebNativeControlAccent},
       {kDisabledAccent, kColorWebNativeControlAccentDisabled},
       {kHoveredAccent, kColorWebNativeControlAccentHovered},
       {kPressedAccent, kColorWebNativeControlAccentPressed},
       {kBackground, kColorWebNativeControlBackground},
       {kDisabledBackground, kColorWebNativeControlBackgroundDisabled},
       {kFill, kColorWebNativeControlFill},
       {kDisabledFill, kColorWebNativeControlFillDisabled},
       {kHoveredFill, kColorWebNativeControlFillHovered},
       {kPressedFill, kColorWebNativeControlFillPressed},
       {kLightenLayer, kColorWebNativeControlLightenLayer},
       {kProgressValue, kColorWebNativeControlProgressValue},
       {kSlider, kColorWebNativeControlSlider},
       {kDisabledSlider, kColorWebNativeControlSliderDisabled},
       {kHoveredSlider, kColorWebNativeControlSliderHovered},
       {kPressedSlider, kColorWebNativeControlSliderPressed},
       {kAutoCompleteBackground, kColorWebNativeControlAutoCompleteBackground},
       {kScrollbarArrowBackground, kColorWebNativeControlScrollbarTrack},
       {kScrollbarArrowBackgroundHovered,
        kColorWebNativeControlScrollbarArrowBackgroundHovered},
       {kScrollbarArrowBackgroundPressed,
        kColorWebNativeControlScrollbarArrowBackgroundPressed},
       {kScrollbarArrow, kColorWebNativeControlScrollbarArrowForeground},
       {kScrollbarArrowHovered, kColorWebNativeControlScrollbarArrowForeground},
       {kScrollbarArrowPressed,
        kColorWebNativeControlScrollbarArrowForegroundPressed},
       {kScrollbarCornerControlColorId, kColorWebNativeControlScrollbarCorner},
       {kScrollbarTrack, kColorWebNativeControlScrollbarTrack},
       {kScrollbarThumb, kColorWebNativeControlScrollbarThumb},
       {kScrollbarThumbHovered, kColorWebNativeControlScrollbarThumbHovered},
       {kScrollbarThumbInactive, kColorWebNativeControlScrollbarThumbInactive},
       {kScrollbarThumbPressed, kColorWebNativeControlScrollbarThumbPressed},
       {kButtonBorder, kColorWebNativeControlButtonBorder},
       {kButtonDisabledBorder, kColorWebNativeControlButtonBorderDisabled},
       {kButtonHoveredBorder, kColorWebNativeControlButtonBorderHovered},
       {kButtonPressedBorder, kColorWebNativeControlButtonBorderPressed},
       {kButtonFill, kColorWebNativeControlButtonFill},
       {kButtonDisabledFill, kColorWebNativeControlButtonFillDisabled},
       {kButtonHoveredFill, kColorWebNativeControlButtonFillHovered},
       {kButtonPressedFill, kColorWebNativeControlButtonFillPressed}});
  CHECK(color_provider);
  return color_provider->GetColor(kColorMap.at(color_id));
}

void NativeThemeBase::PaintFrameTopArea(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const FrameTopAreaExtraParams& extra_params) const {
  cc::PaintFlags flags;
  flags.setColor(extra_params.default_background_color);
  canvas->drawRect(gfx::RectToSkRect(rect), flags);
}

void NativeThemeBase::PaintMenuPopupBackground(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& extra_params) const {
  CHECK(color_provider);
  // TODO(crbug.com/40219248): Remove FromColor and make all SkColor4f.
  canvas->drawColor(
      SkColor4f::FromColor(color_provider->GetColor(kColorMenuBackground)),
      SkBlendMode::kSrc);
}

void NativeThemeBase::PaintMenuSeparator(
    cc::PaintCanvas* canvas,
    const ui::ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const MenuSeparatorExtraParams& extra_params) const {
  DCHECK(color_provider);
  cc::PaintFlags flags;
  flags.setColor(color_provider->GetColor(extra_params.color_id));
  canvas->drawRect(gfx::RectToSkRect(*extra_params.paint_rect), flags);
}

void NativeThemeBase::PaintArrowButton(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const gfx::Rect& rect,
    Part part,
    State state,
    bool forced_colors,
    bool dark_mode,
    PreferredContrast contrast,
    const ScrollbarArrowExtraParams& extra_params) const {
  NOTIMPLEMENTED();
}

void NativeThemeBase::PaintScrollbarThumb(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    Part part,
    State state,
    const gfx::Rect& rect,
    const ScrollbarThumbExtraParams& extra_params) const {
  NOTIMPLEMENTED();
}

void NativeThemeBase::PaintScrollbarTrack(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect,
    bool forced_colors,
    PreferredContrast contrast) const {
  NOTIMPLEMENTED();
}

void NativeThemeBase::PaintScrollbarCorner(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const ScrollbarTrackExtraParams& extra_params) const {
  NOTIMPLEMENTED();
}

SkColor NativeThemeBase::GetArrowColor(
    State state,
    bool dark_mode,
    const ColorProvider* color_provider) const {
  if (state != kDisabled) {
    constexpr SkColor kArrowDisabledColor[2] = {SK_ColorBLACK, SK_ColorWHITE};
    return GetColor(kArrowDisabledColor, dark_mode);
  }

  constexpr SkColor kTrackColor[2] = {SkColorSetRGB(0xD3, 0xD3, 0xD3),
                                      gfx::kPlaceholderColor};
  SkScalar track_hsv[3];
  SkColorToHSV(GetColor(kTrackColor, dark_mode), track_hsv);

  SkScalar thumb_hsv[3];
  SkColorToHSV(
      GetControlColor(kScrollbarThumbInactive, dark_mode, color_provider),
      thumb_hsv);
  return OutlineColor(track_hsv, thumb_hsv);
}

SkColor NativeThemeBase::ControlsAccentColorForState(
    State state,
    bool dark_mode,
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
  return GetControlColor(color_id, dark_mode, color_provider);
}

SkColor NativeThemeBase::ControlsSliderColorForState(
    State state,
    bool dark_mode,
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
  return GetControlColor(color_id, dark_mode, color_provider);
}

SkColor NativeThemeBase::ButtonBorderColorForState(
    State state,
    bool dark_mode,
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
  return GetControlColor(color_id, dark_mode, color_provider);
}

SkColor NativeThemeBase::ButtonFillColorForState(
    State state,
    bool dark_mode,
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
  return GetControlColor(color_id, dark_mode, color_provider);
}

SkColor NativeThemeBase::ControlsBorderColorForState(
    State state,
    bool dark_mode,
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
  return GetControlColor(color_id, dark_mode, color_provider);
}

SkColor NativeThemeBase::ControlsFillColorForState(
    State state,
    bool dark_mode,
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
  return GetControlColor(color_id, dark_mode, color_provider);
}

SkColor NativeThemeBase::SaturateAndBrighten(SkScalar* hsv,
                                             SkScalar saturate_amount,
                                             SkScalar brighten_amount) const {
  SkScalar color[3];
  color[0] = hsv[0];
  color[1] = std::clamp(UNSAFE_TODO(hsv[1]) + saturate_amount, SkScalar{0},
                        SK_Scalar1);
  color[2] = std::clamp(UNSAFE_TODO(hsv[2]) + brighten_amount, SkScalar{0},
                        SK_Scalar1);
  return SkHSVToColor(color);
}

void NativeThemeBase::PaintLightenLayer(cc::PaintCanvas* canvas,
                                        const ColorProvider* color_provider,
                                        const SkRect& skrect,
                                        State state,
                                        float border_radius,
                                        bool dark_mode) const {
  if (state != kDisabled) {
    return;
  }
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(GetControlColor(kLightenLayer, dark_mode, color_provider));
  canvas->drawRoundRect(skrect, border_radius, border_radius, flags);
}

void NativeThemeBase::PaintArrow(cc::PaintCanvas* canvas,
                                 const gfx::Rect& rect,
                                 Part part,
                                 SkColor color) const {
  cc::PaintFlags flags;
  flags.setColor(color);
  canvas->drawPath(PathForArrow(GetArrowRect(rect), part), flags);
}

// static
SkPath NativeThemeBase::PathForArrow(const gfx::RectF& rect, Part part) {
  SkPath path;
  if (part == kScrollbarUpArrow || part == kScrollbarDownArrow) {
    // Draw up-pointing arrow.
    const int arrow_height = rect.height() / 2 + 1;
    path.moveTo(rect.x(), rect.bottom());
    path.rLineTo(rect.width(), 0);
    path.rLineTo(-rect.width() / 2.0f, -arrow_height);
    path.close();
    path.offset(0, -arrow_height / 2 + 1);
  } else {
    // Draw right-pointing arrow.
    int arrow_width = rect.width() / 2 + 1;
    path.moveTo(rect.x(), rect.y());
    path.rLineTo(0, rect.height());
    path.rLineTo(arrow_width, -rect.height() / 2.0f);
    path.close();
    path.offset(arrow_width / 2, 0);
  }

  // Mirror the above path for down/left.
  if (part == kScrollbarDownArrow || part == kScrollbarLeftArrow) {
    SkMatrix transform;
    const gfx::PointF center = rect.CenterPoint();
    const bool vert = part == kScrollbarDownArrow;
    transform.setScale(vert ? 1 : -1, vert ? -1 : 1, center.x(), center.y());
    path.transform(transform);
  }

  return path;
}

std::optional<SkColor> NativeThemeBase::GetContrastingPressedOrHoveredColor(
    std::optional<SkColor> fg_color,
    std::optional<SkColor> bg_color,
    State state,
    Part part) const {
  if (!fg_color.has_value() || (state != kPressed && state != kHovered) ||
      SkColorGetA(fg_color.value()) == SK_AlphaTRANSPARENT) {
    return fg_color;
  }
  const SkColor resulting_color =
      color_utils::BlendForMinContrast(
          fg_color.value(), SkColorSetA(fg_color.value(), SK_AlphaOPAQUE),
          std::nullopt, GetContrastRatioForState(state, part))
          .color;
  if (!bg_color.has_value()) {
    return resulting_color;
  }
  // Guaranteeing contrast with the background is prioritized over having
  // contrast with the original part color. Making a second pass with the
  // transforming function might make the final color not contrast as much with
  // the original color, but the result is better than using
  // `PickGoogleColorTwoBackgrounds()`, which tries to guarantee contrast with
  // both (original and background) colors simultaneously, and ends up creating
  // contrast changes that are too harsh.
  return color_utils::BlendForMinContrast(
             resulting_color, SkColorSetA(bg_color.value(), SK_AlphaOPAQUE),
             std::nullopt, color_utils::kMinimumVisibleContrastRatio)
      .color;
}

float NativeThemeBase::GetContrastRatioForState(State state, Part part) const {
  return 2.15f;
}

void NativeThemeBase::PaintCheckbox(cc::PaintCanvas* canvas,
                                    const ColorProvider* color_provider,
                                    State state,
                                    const gfx::Rect& rect,
                                    const ButtonExtraParams& extra_params,
                                    bool dark_mode,
                                    std::optional<SkColor> accent_color) const {
  // Paint the background and border.
  const float radius =
      GetBorderRadiusForPart(kCheckbox, rect.width(), rect.height());
  const SkRect skrect = PaintCheckboxRadioCommon(
      canvas, color_provider, state, rect, extra_params, true, radius,
      dark_mode, accent_color);
  if (skrect.isEmpty()) {
    return;
  }

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  if (extra_params.indeterminate || extra_params.checked) {
    // Paint an accent-colored background.
    if (accent_color && state != kDisabled) {
      flags.setColor(
          CustomAccentColorForState(*accent_color, state, dark_mode));
    } else {
      flags.setColor(
          ControlsAccentColorForState(state, dark_mode, color_provider));
    }
    canvas->drawRoundRect(skrect, radius, radius, flags);
  }
  flags.setColor(
      ControlsBackgroundColorForState(state, dark_mode, color_provider));
  if (extra_params.indeterminate) {
    // Paint the dash.
    static constexpr gfx::Size kDashSize(8, 2);
    static constexpr float kXInset =
        (kCheckboxSize.width() - kDashSize.width()) / 2.0f;
    static constexpr float kYInset =
        (kCheckboxSize.height() - kDashSize.height()) / 2.0f;
    canvas->drawRoundRect(
        skrect.makeInset(kXInset * skrect.width() / kCheckboxSize.width(),
                         kYInset * skrect.height() / kCheckboxSize.height()),
        radius, radius, flags);
  } else if (extra_params.checked) {
    // Paint the checkmark.
    SkPath check;
    check.moveTo(skrect.x() + skrect.width() * 0.2f, skrect.centerY());
    check.rLineTo(skrect.width() * 0.2f, skrect.height() * 0.2f);
    check.lineTo(skrect.right() - skrect.width() * 0.2f,
                 skrect.y() + skrect.height() * 0.2f);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(skrect.height() * 0.16f);
    canvas->drawPath(check, flags);
  }
}

void NativeThemeBase::PaintInnerSpinButton(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    gfx::Rect rect,
    const InnerSpinButtonExtraParams& extra_params,
    bool forced_colors,
    bool dark_mode,
    PreferredContrast contrast) const {
  if (extra_params.read_only) {
    state = kDisabled;
  }

  State increase_state = state;
  State decrease_state = state;
  if (extra_params.spin_up) {
    decrease_state = decrease_state != kDisabled ? kNormal : kDisabled;
  } else {
    increase_state = increase_state != kDisabled ? kNormal : kDisabled;
  }

  const ScrollbarArrowExtraParams arrow = {.zoom = 1.0f};
  if (extra_params.spin_arrows_direction ==
      ui::NativeTheme::SpinArrowsDirection::kUpDown) {
    rect.set_height(rect.height() / 2);
    PaintArrowButton(canvas, color_provider, rect, kScrollbarUpArrow,
                     increase_state, forced_colors, dark_mode, contrast, arrow);

    rect.set_y(rect.bottom());
    PaintArrowButton(canvas, color_provider, rect, kScrollbarDownArrow,
                     decrease_state, forced_colors, dark_mode, contrast, arrow);
  } else {
    rect.set_width(rect.width() / 2);
    PaintArrowButton(canvas, color_provider, rect, kScrollbarLeftArrow,
                     decrease_state, forced_colors, dark_mode, contrast, arrow);

    rect.set_x(rect.right());
    PaintArrowButton(canvas, color_provider, rect, kScrollbarRightArrow,
                     increase_state, forced_colors, dark_mode, contrast, arrow);
  }
}

void NativeThemeBase::PaintMenuList(cc::PaintCanvas* canvas,
                                    const ColorProvider* color_provider,
                                    State state,
                                    const gfx::Rect& rect,
                                    const MenuListExtraParams& extra_params,
                                    bool dark_mode) const {
  // If a border radius is specified, Blink will paint the background and the
  // border.
  // TODO(pkasting): The comment above seems untrue;
  // `ThemePainterDefault::PaintMenuList()` always returns false, so I believe
  // Blink never paints the CSS border/background?
  if (!extra_params.has_border_radius) {
    TextFieldExtraParams params;
    params.background_color = extra_params.background_color;
    params.has_border = extra_params.has_border;
    params.zoom = extra_params.zoom;
    PaintTextField(canvas, color_provider, state, rect, params, dark_mode);
  }

  // The arrow base is twice the arrow height, giving 45 degree sides.
  static constexpr float kAspectRatio = 2.0f;

  SkPath path;
  if (extra_params.arrow_direction == ui::NativeTheme::ArrowDirection::kDown) {
    int intended_width = extra_params.arrow_size;
    int intended_height = base::ClampFloor(intended_width / kAspectRatio);

    // `extra_params.arrow_x` is the left edge, but `extra_params.arrow_y` is
    // the vertical center.
    // TODO(pkasting): This leads to complication and bugs; change
    // `ThemePainterDefault::SetupMenuListArrow()` to pass the arrow center
    // point and fix `NativeTheme` implementations accordingly.
    gfx::Rect arrow(extra_params.arrow_x,
                    extra_params.arrow_y - (intended_height / 2),
                    intended_width, intended_height);

    // Fit the arrow within the paint rect.
    arrow.Intersect(rect);
    if (arrow.width() != intended_width || arrow.height() != intended_height) {
      // Shrink the arrow so it's not clipped. Pick the dimension that was
      // clipped "more" (keeping in mind that each px of height is worth
      // `kAspectRatio` px of width) and compute the other dimension based on
      // that.
      const int height_clip = (intended_height - arrow.height()) * kAspectRatio;
      const int width_clip = intended_width - arrow.width();
      if (height_clip > width_clip) {
        arrow.set_width(arrow.height() * kAspectRatio);
      } else {
        arrow.set_height(arrow.width() / kAspectRatio);
      }
      arrow.set_origin(
          {extra_params.arrow_x + (intended_width - arrow.width()) / 2,
           extra_params.arrow_y - arrow.height() / 2});
    }

    path.moveTo(arrow.x(), arrow.y());
    path.lineTo(arrow.x() + arrow.width() / 2, arrow.y() + arrow.height());
    path.lineTo(arrow.x() + arrow.width(), arrow.y());
  } else {
    // Arrow direction is either left or right.
    int intended_height = extra_params.arrow_size;
    int intended_width = base::ClampFloor(intended_height / kAspectRatio);

    // `extra_params.arrow_x` is the horizontal center, but
    // `extra_params.arrow_y` is the top edge.
    // TODO(pkasting): This leads to complication and bugs; change
    // `ThemePainterDefault::SetupMenuListArrow()` to pass the arrow center
    // point and fix `NativeTheme` implementations accordingly.
    gfx::Rect arrow(extra_params.arrow_x - (intended_width / 2),
                    extra_params.arrow_y, intended_width, intended_height);

    // Fit the arrow within the paint rect.
    arrow.Intersect(rect);
    if (intended_width != arrow.width() || intended_height != arrow.height()) {
      // Shrink the arrow so it's not clipped. Pick the dimension that was
      // clipped "more" (keeping in mind that each px of width is worth
      // `kAspectRatio` px of height) and compute the other dimension based on
      // that.
      const int height_clip = intended_height - arrow.height();
      const int width_clip = (intended_width - arrow.width()) * kAspectRatio;
      if (height_clip > width_clip) {
        arrow.set_width(arrow.height() / kAspectRatio);
      } else {
        arrow.set_height(arrow.width() * kAspectRatio);
      }
      arrow.set_origin(
          {extra_params.arrow_x - arrow.width() / 2,
           extra_params.arrow_y + (intended_height - arrow.height()) / 2});
    }

    if (extra_params.arrow_direction == ArrowDirection::kLeft) {
      path.moveTo(arrow.right(), arrow.y());
      path.lineTo(arrow.x(), arrow.y() + arrow.height() / 2);
      path.lineTo(arrow.right(), arrow.bottom());
    } else {
      path.moveTo(arrow.x(), arrow.y());
      path.lineTo(arrow.right(), arrow.y() + arrow.height() / 2);
      path.lineTo(arrow.x(), arrow.bottom());
    }
  }
  // NOTE: Do not close the path; we want a "v" shape, not a triangle.

  cc::PaintFlags flags;
  flags.setColor(extra_params.arrow_color);
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(2.0f);
  canvas->drawPath(path, flags);
}

void NativeThemeBase::PaintProgressBar(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const ProgressBarExtraParams& extra_params,
    bool dark_mode,
    PreferredContrast contrast,
    std::optional<SkColor> accent_color) const {
  CHECK(!rect.IsEmpty());

  SliderExtraParams slider;
  float thickness = rect.height();
  constexpr float kTrackBlockRatio =
      static_cast<float>(kSliderTrackThickness) / kSliderThumbThickness;
  if (extra_params.is_horizontal) {
    slider.vertical = false;
    thickness = rect.height() * kTrackBlockRatio;
  } else {
    slider.vertical = true;
    thickness = rect.width() * kTrackBlockRatio;
  }
  const SkRect track_rect = AlignSliderTrack(rect, slider, false, thickness);
  const float radius =
      GetBorderRadiusForPart(kProgressBar, rect.width(), rect.height());

  // Paint the track.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(GetControlColor(kFill, dark_mode, color_provider));
  canvas->drawRoundRect(track_rect, radius, radius, flags);

  // Paint the progress value bar.
  const SkScalar kMinimumProgressInlineValue = 2;
  SkScalar value_height = extra_params.value_rect_height;
  SkScalar value_width = extra_params.value_rect_width;
  // If adjusted thickness is not zero, make sure it is equal or larger than
  // kMinimumProgressInlineValue.
  if (slider.vertical) {
    if (value_height > 0) {
      value_height = std::max(kMinimumProgressInlineValue, value_height);
    }
  } else {
    if (value_width > 0) {
      value_width = std::max(kMinimumProgressInlineValue, value_width);
    }
  }
  const SkRect value_rect = AlignSliderTrack(
      gfx::Rect(extra_params.value_rect_x, extra_params.value_rect_y,
                value_width, value_height),
      slider, false, thickness);
  if (accent_color) {
    flags.setColor(*accent_color);
  } else {
    flags.setColor(GetControlColor(kAccent, dark_mode, color_provider));
  }
  canvas->clipRRect(SkRRect::MakeRectXY(track_rect, radius, radius),
                    SkClipOp::kIntersect, true);
  if (extra_params.determinate) {
    canvas->drawRect(value_rect, flags);
  } else {
    canvas->drawRoundRect(value_rect, radius, radius, flags);
  }

  // Paint the border.
  const float border_width =
      AdjustBorderWidthByZoom(kBorderWidth, extra_params.zoom);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(border_width);
  SkColor border_color = GetControlColor(kBorder, dark_mode, color_provider);
  if (contrast != PreferredContrast::kMore && !dark_mode) {
    border_color = SkColorSetA(border_color, 0x80);
  }
  flags.setColor(border_color);
  canvas->drawRoundRect(
      track_rect.makeInset(border_width / 2, border_width / 2), radius, radius,
      flags);
}

void NativeThemeBase::PaintButton(cc::PaintCanvas* canvas,
                                  const ColorProvider* color_provider,
                                  State state,
                                  const gfx::Rect& rect,
                                  const ButtonExtraParams& extra_params,
                                  bool dark_mode) const {
  SkRect skrect = gfx::RectToSkRect(rect);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(ButtonFillColorForState(state, dark_mode, color_provider));

  // If the button is too small, fall back to drawing a solid color rect.
  if (rect.width() < 5 || rect.height() < 5) {
    flags.setColor(ButtonFillColorForState(state, dark_mode, color_provider));
    canvas->drawRect(skrect, flags);
    return;
  }

  const float border_width =
      AdjustBorderWidthByZoom(kBorderWidth, extra_params.zoom);
  skrect.inset(border_width / 2, border_width / 2);
  const float radius = AdjustBorderRadiusByZoom(
      kPushButton,
      GetBorderRadiusForPart(kPushButton, rect.width(), rect.height()),
      extra_params.zoom);

  // Paint the background.
  PaintLightenLayer(canvas, color_provider, skrect, state, radius, dark_mode);
  canvas->drawRoundRect(skrect, radius, radius, flags);

  // Paint the border.
  if (extra_params.has_border) {
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(border_width);
    flags.setColor(ButtonBorderColorForState(state, dark_mode, color_provider));
    canvas->drawRoundRect(skrect, radius, radius, flags);
  }
}

void NativeThemeBase::PaintRadio(cc::PaintCanvas* canvas,
                                 const ColorProvider* color_provider,
                                 State state,
                                 const gfx::Rect& rect,
                                 const ButtonExtraParams& extra_params,
                                 bool dark_mode,
                                 std::optional<SkColor> accent_color) const {
  // Paint the background and border.
  const float radius =
      GetBorderRadiusForPart(kRadio, rect.width(), rect.height());
  const SkRect skrect = PaintCheckboxRadioCommon(
      canvas, color_provider, state, rect, extra_params, false, radius,
      dark_mode, accent_color);
  if (skrect.isEmpty() || !extra_params.checked) {
    return;
  }

  // Paint the dot.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  if (accent_color && state != kDisabled) {
    flags.setColor(CustomAccentColorForState(*accent_color, state, dark_mode));
  } else {
    flags.setColor(
        ControlsAccentColorForState(state, dark_mode, color_provider));
  }
  canvas->drawRoundRect(
      skrect.makeInset(skrect.width() * 0.2, skrect.height() * 0.2), radius,
      radius, flags);
}

void NativeThemeBase::PaintSliderTrack(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const SliderExtraParams& extra_params,
    bool dark_mode,
    PreferredContrast contrast,
    std::optional<SkColor> accent_color) const {
  const float track_height = kSliderTrackThickness * extra_params.zoom;
  SkRect track_rect = AlignSliderTrack(rect, extra_params, false, track_height);
  const float radius =
      GetBorderRadiusForPart(kSliderTrack, rect.width(), rect.height());
  // Shrink the track by 1 pixel on each end so the thumb can completely
  // cover.
  if (extra_params.vertical) {
    track_rect.inset(0, 1);
  } else {
    track_rect.inset(1, 0);
  }

  // Paint the track.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(ControlsFillColorForState(state, dark_mode, color_provider));
  canvas->drawRoundRect(track_rect, radius, radius, flags);

  // Set the clip to the extent of the value bar.
  canvas->save();
  canvas->clipRect(AlignSliderTrack(rect, extra_params, true, track_height),
                   SkClipOp::kIntersect, true);

  // Paint the value bar, clipped to its extent.
  if (accent_color && state != kDisabled) {
    flags.setColor(CustomAccentColorForState(*accent_color, state, dark_mode));
  } else {
    flags.setColor(
        ControlsSliderColorForState(state, dark_mode, color_provider));
  }
  canvas->drawRRect(SkRRect::MakeRectXY(track_rect, radius, radius), flags);
  canvas->restore();

  // Paint the border.
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  const float border_width =
      AdjustBorderWidthByZoom(kBorderWidth, extra_params.zoom);
  flags.setStrokeWidth(border_width);
  SkColor border_color =
      ControlsBorderColorForState(state, dark_mode, color_provider);
  if (contrast != PreferredContrast::kMore && state != kDisabled &&
      !dark_mode) {
    border_color = SkColorSetA(border_color, 0x80);
  }
  flags.setColor(border_color);
  canvas->drawRoundRect(
      track_rect.makeInset(border_width / 2, border_width / 2), radius, radius,
      flags);
}

void NativeThemeBase::PaintSliderThumb(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const SliderExtraParams& extra_params,
    bool dark_mode,
    std::optional<SkColor> accent_color) const {
  const SkRect thumb_rect = gfx::RectToSkRect(rect);
  const float radius =
      GetBorderRadiusForPart(kSliderThumb, rect.width(), rect.height());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  if (accent_color && state != kDisabled) {
    flags.setColor(CustomAccentColorForState(*accent_color, state, dark_mode));
  } else {
    flags.setColor(
        ControlsSliderColorForState(state, dark_mode, color_provider));
  }
  // TODO(pkasting): This inset appears to be a historical accident; consider
  // removing it and rebaselining the slider appearance.
  canvas->drawRoundRect(thumb_rect.makeInset(0.5f, 0.5f), radius, radius,
                        flags);
}

void NativeThemeBase::PaintTextField(cc::PaintCanvas* canvas,
                                     const ColorProvider* color_provider,
                                     State state,
                                     const gfx::Rect& rect,
                                     const TextFieldExtraParams& extra_params,
                                     bool dark_mode) const {
  SkRect bounds = gfx::RectToSkRect(rect);
  const float border_width =
      AdjustBorderWidthByZoom(kBorderWidth, extra_params.zoom);
  bounds.inset(border_width / 2, border_width / 2);
  const float radius = AdjustBorderRadiusByZoom(
      kTextField,
      GetBorderRadiusForPart(kTextField, rect.width(), rect.height()),
      extra_params.zoom);

  // Paint the background.
  const bool paint_autocomplete_background =
      extra_params.auto_complete_active && state != kDisabled;
  if (extra_params.background_color != 0) {
    PaintLightenLayer(canvas, color_provider, bounds, state, radius, dark_mode);
    cc::PaintFlags bg_flags;
    SkColor default_bg_color =
        ControlsBackgroundColorForState(state, dark_mode, color_provider);
    bg_flags.setColor(paint_autocomplete_background
                          ? GetControlColor(kAutoCompleteBackground, dark_mode,
                                            color_provider)
                          : default_bg_color);
    canvas->drawRoundRect(bounds, radius, radius, bg_flags);
  }

  // Paint the border.
  if (extra_params.has_border) {
    cc::PaintFlags border_flags;
    border_flags.setColor(
        ControlsBorderColorForState(state, dark_mode, color_provider));
    border_flags.setStyle(cc::PaintFlags::kStroke_Style);
    border_flags.setStrokeWidth(border_width);
    canvas->drawRoundRect(bounds, radius, radius, border_flags);
  }
}

gfx::RectF NativeThemeBase::GetArrowRect(const gfx::Rect& rect) const {
  // Note: Using initializer_list form forces returning by copy, not ref.
  const auto [min_side, max_side] = std::minmax({rect.width(), rect.height()});
  const int side_length_inset = 2 * std::ceil(max_side / 4.f);
  const int side_length = std::min(min_side, max_side - side_length_inset);
  // When there are an odd number of pixels, put the extra on the top/left.
  return gfx::RectF(gfx::Rect(rect.x() + (rect.width() - side_length + 1) / 2,
                              rect.y() + (rect.height() - side_length + 1) / 2,
                              side_length, side_length));
}

SkColor NativeThemeBase::ControlsBackgroundColorForState(
    State state,
    bool dark_mode,
    const ColorProvider* color_provider) const {
  ControlColorId color_id;
  if (state == kDisabled) {
    color_id = kDisabledBackground;
  } else {
    color_id = kBackground;
  }
  return GetControlColor(color_id, dark_mode, color_provider);
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
  SkScalar min_diff = std::clamp(
      (UNSAFE_TODO(hsv1[1]) + UNSAFE_TODO(hsv2[1])) * 1.2f, 0.28f, 0.5f);
  SkScalar diff = std::clamp(
      fabsf(UNSAFE_TODO(hsv1[2]) - UNSAFE_TODO(hsv2[2])) / 2, min_diff, 0.5f);

  if (UNSAFE_TODO(hsv1[2]) + UNSAFE_TODO(hsv2[2]) > 1.0) {
    diff = -diff;
  }

  return SaturateAndBrighten(hsv2, -0.2f, diff);
}

SkRect NativeThemeBase::PaintCheckboxRadioCommon(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const ButtonExtraParams& extra_params,
    bool is_checkbox,
    float border_radius,
    bool dark_mode,
    std::optional<SkColor> accent_color) const {
  // Use the largest square that fits inside the provided rectangle. This
  // matches other browsers.
  SkRect skrect = gfx::RectToSkRect(rect);
  if (skrect.width() != skrect.height()) {
    SkScalar size = std::min(skrect.width(), skrect.height());
    skrect.inset((skrect.width() - size) / 2, (skrect.height() - size) / 2);
  }

  // If the square is too small then paint only a square.
  cc::PaintFlags flags;
  if (skrect.width() <= 2) {
    if (accent_color && state != kDisabled) {
      flags.setColor(
          CustomAccentColorForState(*accent_color, state, dark_mode));
    } else {
      flags.setColor(GetControlColor(kBorder, dark_mode, color_provider));
    }
    canvas->drawRect(skrect, flags);
    return {};  // Don't draw anything more.
  }

  // Paint the background.
  // Shrink the rect slightly to avoid antialiasing artifacts with the border.
  const auto background_rect =
      skrect.makeInset(kBorderWidth * 0.2f, kBorderWidth * 0.2f);
  PaintLightenLayer(canvas, color_provider, background_rect, state,
                    border_radius, dark_mode);
  flags.setAntiAlias(true);
  flags.setColor(
      ControlsBackgroundColorForState(state, dark_mode, color_provider));
  canvas->drawRoundRect(background_rect, border_radius, border_radius, flags);

  // Paint the border.
  // Indeterminate and checked checkboxes do not draw a border; they will draw
  // an accent-colored background instead on the caller side.
  if (!is_checkbox || (!extra_params.checked && !extra_params.indeterminate)) {
    SkColor border_color;
    if (extra_params.checked) {
      if (accent_color && state != kDisabled) {
        border_color =
            CustomAccentColorForState(*accent_color, state, dark_mode);
      } else {
        border_color =
            ControlsAccentColorForState(state, dark_mode, color_provider);
      }
    } else {
      border_color =
          ControlsBorderColorForState(state, dark_mode, color_provider);
    }
    flags.setColor(border_color);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kBorderWidth);
    canvas->drawRoundRect(skrect.makeInset(kBorderWidth / 2, kBorderWidth / 2),
                          border_radius, border_radius, flags);
  }

  // Let the caller draw any additional decorations.
  return skrect;
}

}  // namespace ui
