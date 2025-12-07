// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_base.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>
#include <variant>

#include "base/check.h"
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
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/native_theme/features/native_theme_features.h"
#include "ui/native_theme/native_theme.h"

namespace ui {

namespace {

static constexpr gfx::Size kCheckboxSize(13, 13);
static constexpr int kSliderTrackThickness = 8;
static constexpr int kSliderThumbThickness = 16;
static constexpr float kBorderWidth = 1.0f;

SkRect AlignSliderTrack(const gfx::Rect& slider_rect,
                        const NativeTheme::SliderExtraParams& extra_params,
                        bool is_value,
                        float thickness) {
  const gfx::RectF r(slider_rect);
  const gfx::PointF center = r.CenterPoint();
  const float half_track_thickness = thickness / 2;

  if (extra_params.vertical) {
    float top = r.y();
    float bottom = r.bottom();
    if (is_value) {
      // Extend to ensure the thumb completely covers the end of the value rect.
      // Because the thumb radius is greater than `half_track_thickness`,
      // extending that much is guaranteed to be sufficient.
      (extra_params.right_to_left ? top : bottom) =
          top + extra_params.thumb_y + half_track_thickness;
    }
    return SkRect::MakeLTRB(
        std::max(r.x(), center.x() - half_track_thickness), top,
        std::min(r.right(), center.x() + half_track_thickness), bottom);
  }

  float left = r.x();
  float right = r.right();
  if (is_value) {
    (extra_params.right_to_left ? left : right) =
        left + extra_params.thumb_x + half_track_thickness;
  }
  return SkRect::MakeLTRB(
      left, std::max(r.y(), center.y() - half_track_thickness), right,
      std::min(r.bottom(), center.y() + half_track_thickness));
}

}  // namespace

gfx::Size NativeThemeBase::GetPartSize(Part part,
                                       State state,
                                       const ExtraParams& extra_params) const {
  if (part == kCheckbox || part == kRadio) {
    return kCheckboxSize;
  }
  if (part == kSliderThumb) {
    return gfx::Size(kSliderThumbThickness, kSliderThumbThickness);
  }
  if (part != kInnerSpinButton &&
      (part < kScrollbarDownArrow || part > kScrollbarVerticalTrack)) {
    return gfx::Size();  // No default size.
  }
  gfx::Size size =
      (part == kScrollbarHorizontalThumb || part == kScrollbarVerticalThumb)
          ? GetVerticalScrollbarThumbSize()
          : GetVerticalScrollbarButtonSize();
  if (part == kInnerSpinButton || part == kScrollbarHorizontalTrack ||
      part == kScrollbarVerticalTrack) {
    size.set_height(0);
  }
  if (part == kScrollbarLeftArrow || part == kScrollbarRightArrow ||
      part == kScrollbarHorizontalThumb || part == kScrollbarHorizontalTrack) {
    size.Transpose();
  }
  return size;
}

float NativeThemeBase::GetBorderRadiusForPart(Part part,
                                              float width,
                                              float height) const {
  if (part == kCheckbox || part == kPushButton || part == kTextField) {
    return 2.0f;
  }
  if (part == kProgressBar || part == kSliderTrack) {
    // In the common case, the thickness is small enough that this has the same
    // effect as the radio/slider thumb code below.
    return 40.0f;
  }
  return (part == kRadio || part == kSliderThumb)
             ? (std::max(width, height) * 0.5f)
             : 0;
}

SkColor NativeThemeBase::GetScrollbarThumbColor(
    const ColorProvider* color_provider,
    State state,
    const ScrollbarThumbExtraParams& extra_params) const {
  const SkColor bg_color =
      GetContrastingColorForScrollbarPart(extra_params.track_color,
                                          std::nullopt, state)
          .value_or(GetControlColor(kScrollbarTrack, {}, {}, color_provider));
  if (const std::optional<SkColor> color = GetContrastingColorForScrollbarPart(
          extra_params.thumb_color, bg_color, state)) {
    return color.value();
  }
  if (const std::optional<ColorId> id =
          GetScrollbarThumbColorId(state, extra_params)) {
    return color_provider->GetColor(id.value());
  }
  static constexpr auto kScrollbarThumbColors =
      std::to_array({kScrollbarThumb, kScrollbarThumbHovered, kScrollbarThumb,
                     kScrollbarThumbPressed});
  return GetControlColorForState(kScrollbarThumbColors, state, {}, {},
                                 color_provider);
}

NativeThemeBase::~NativeThemeBase() = default;

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
                    contrast, accent_color);
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
                    std::get<MenuListExtraParams>(extra_params), dark_mode,
                    contrast);
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
                  std::get<ButtonExtraParams>(extra_params), dark_mode,
                  contrast);
      break;
    case kRadio:
      PaintRadio(canvas, color_provider, state, rect,
                 std::get<ButtonExtraParams>(extra_params), dark_mode, contrast,
                 accent_color);
      break;
    case kScrollbarDownArrow:
    case kScrollbarUpArrow:
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
      if (!GetVerticalScrollbarButtonSize().IsEmpty()) {
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
                       contrast, accent_color);
      break;
    case kTextField:
      PaintTextField(canvas, color_provider, state, rect,
                     std::get<TextFieldExtraParams>(extra_params), dark_mode,
                     contrast);
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

gfx::Size NativeThemeBase::GetVerticalScrollbarButtonSize() const {
  return gfx::Size(15, 14);
}

gfx::Size NativeThemeBase::GetVerticalScrollbarThumbSize() const {
  const int thickness = GetVerticalScrollbarButtonSize().width();
  return gfx::Size(thickness, thickness * 2);
}

gfx::RectF NativeThemeBase::GetArrowRect(const gfx::Rect& rect,
                                         Part part,
                                         State state) const {
  // Note: Using initializer_list form forces returning by copy, not ref.
  const auto [min_side, max_side] = std::minmax({rect.width(), rect.height()});
  const int side_length_inset = (max_side + 3) / 4;
  const int side_length = std::min(min_side, max_side - side_length_inset * 2);
  // When there are an odd number of pixels, put the extra on the top/left.
  return gfx::RectF(gfx::Rect(rect.x() + (rect.width() - side_length + 1) / 2,
                              rect.y() + (rect.height() - side_length + 1) / 2,
                              side_length, side_length));
}

SkColor NativeThemeBase::GetControlColor(
    ControlColorId color_id,
    bool dark_mode,
    PreferredContrast contrast,
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
       {kCheckboxBackground, kColorWebNativeControlCheckboxBackground},
       {kDisabledCheckboxBackground,
        kColorWebNativeControlCheckboxBackgroundDisabled},
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
       {kSliderBorder, kColorWebNativeControlSliderBorder},
       {kHoveredSliderBorder, kColorWebNativeControlSliderBorderHovered},
       {kPressedSliderBorder, kColorWebNativeControlSliderBorderPressed},
       {kAutoCompleteBackground, kColorWebNativeControlAutoCompleteBackground},
       {kScrollbarArrowBackground, kColorWebNativeControlScrollbarTrack},
       {kScrollbarArrowBackgroundDisabled,
        kColorWebNativeControlScrollbarArrowBackgroundDisabled},
       {kScrollbarArrowBackgroundHovered,
        kColorWebNativeControlScrollbarArrowBackgroundHovered},
       {kScrollbarArrowBackgroundPressed,
        kColorWebNativeControlScrollbarArrowBackgroundPressed},
       {kScrollbarArrow, kColorWebNativeControlScrollbarArrowForeground},
       {kScrollbarArrowDisabled,
        kColorWebNativeControlScrollbarArrowForegroundDisabled},
       {kScrollbarArrowHovered, kColorWebNativeControlScrollbarArrowForeground},
       {kScrollbarArrowPressed,
        kColorWebNativeControlScrollbarArrowForegroundPressed},
       {kScrollbarCornerControlColorId, kColorWebNativeControlScrollbarCorner},
       {kScrollbarTrack, kColorWebNativeControlScrollbarTrack},
       {kScrollbarThumb, kColorWebNativeControlScrollbarThumb},
       {kScrollbarThumbHovered, kColorWebNativeControlScrollbarThumbHovered},
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

std::optional<ColorId> NativeThemeBase::GetScrollbarThumbColorId(
    State state,
    const ScrollbarThumbExtraParams& extra_params) const {
  return std::nullopt;
}

float NativeThemeBase::GetScrollbarPartContrastRatioForState(
    State state) const {
  return 2.15f;
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
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const MenuSeparatorExtraParams& extra_params) const {
  CHECK(color_provider);
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

SkColor NativeThemeBase::GetControlColorForState(
    base::span<const ControlColorId, 4> colors,
    State state,
    bool dark_mode,
    PreferredContrast contrast,
    const ColorProvider* color_provider) const {
  return GetControlColor(colors[state], dark_mode, contrast, color_provider);
}

SkColor NativeThemeBase::GetScrollbarArrowBackgroundColor(
    const ScrollbarArrowExtraParams& extra_params,
    State state,
    bool dark_mode,
    PreferredContrast contrast,
    const ColorProvider* color_provider) const {
  static constexpr auto kScrollbarArrowBackgroundColors = std::to_array(
      {kScrollbarArrowBackgroundDisabled, kScrollbarArrowBackgroundHovered,
       kScrollbarArrowBackground, kScrollbarArrowBackgroundPressed});
  return GetContrastingColorForScrollbarPart(extra_params.track_color,
                                             std::nullopt, state)
      .value_or(GetControlColorForState(kScrollbarArrowBackgroundColors, state,
                                        dark_mode, contrast, color_provider));
}

SkColor NativeThemeBase::GetScrollbarArrowForegroundColor(
    SkColor bg_color,
    const ScrollbarArrowExtraParams& extra_params,
    State state,
    bool dark_mode,
    PreferredContrast contrast,
    const ColorProvider* color_provider) const {
  static constexpr auto kScrollbarArrowColors =
      std::to_array({kScrollbarArrowDisabled, kScrollbarArrowHovered,
                     kScrollbarArrow, kScrollbarArrowPressed});
  return GetContrastingColorForScrollbarPart(extra_params.thumb_color, bg_color,
                                             state)
      .value_or(GetControlColorForState(kScrollbarArrowColors, state, dark_mode,
                                        contrast, color_provider));
}

void NativeThemeBase::PaintLightenLayer(cc::PaintCanvas* canvas,
                                        const ColorProvider* color_provider,
                                        const SkRect& skrect,
                                        State state,
                                        float border_radius,
                                        bool dark_mode,
                                        PreferredContrast contrast) const {
  if (state != kDisabled) {
    return;
  }
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(
      GetControlColor(kLightenLayer, dark_mode, contrast, color_provider));
  canvas->drawRoundRect(skrect, border_radius, border_radius, flags);
}

void NativeThemeBase::PaintArrow(cc::PaintCanvas* canvas,
                                 const gfx::Rect& rect,
                                 Part part,
                                 State state,
                                 SkColor color) const {
  cc::PaintFlags flags;
  flags.setColor(color);
  canvas->drawPath(PathForArrow(GetArrowRect(rect, part, state), part), flags);
}

// static
SkPath NativeThemeBase::PathForArrow(const gfx::RectF& rect, Part part) {
  SkPathBuilder path;
  if (part == kScrollbarUpArrow || part == kScrollbarDownArrow) {
    // Draw up-pointing arrow.
    const int arrow_height = base::ClampRound(rect.height()) / 2 + 1;
    path.moveTo(rect.x(), rect.bottom() - arrow_height / 2 + 1);
    path.rLineTo(rect.width(), 0);
    path.rLineTo(-rect.width() / 2.0f, -arrow_height);
  } else {
    // Draw right-pointing arrow.
    const int arrow_width = base::ClampRound(rect.width()) / 2 + 1;
    path.moveTo(rect.x() + arrow_width / 2, rect.y());
    path.rLineTo(0, rect.height());
    path.rLineTo(arrow_width, -rect.height() / 2.0f);
  }
  path.close();

  // Mirror the above path for down/left.
  if (part == kScrollbarDownArrow || part == kScrollbarLeftArrow) {
    SkMatrix transform;
    const gfx::PointF center = rect.CenterPoint();
    const bool vert = part == kScrollbarDownArrow;
    transform.setScale(vert ? 1 : -1, vert ? -1 : 1, center.x(), center.y());
    path.transform(transform);
  }

  return path.detach();
}

SkColor NativeThemeBase::GetAccentOrControlColorForState(
    std::optional<SkColor> accent_color,
    base::span<const ControlColorId, 4> colors,
    State state,
    bool dark_mode,
    PreferredContrast contrast,
    const ColorProvider* color_provider) const {
  if (state == kDisabled || !accent_color.has_value()) {
    return GetControlColorForState(colors, state, dark_mode, contrast,
                                   color_provider);
  }

  if (state == kNormal) {
    return accent_color.value();
  }

  // Approximates the lightness difference between `kAccent` and
  // `kHoveredAccent`.
  static constexpr double kLightnessAdjust = 0.11;
  double l_adjust = (state == kPressed) ? kLightnessAdjust : -kLightnessAdjust;
  if (dark_mode) {
    l_adjust = -l_adjust;
  }

  color_utils::HSL hsl;
  color_utils::SkColorToHSL(accent_color.value(), &hsl);
  hsl.l = std::clamp(hsl.l + l_adjust, 0.0, 1.0);
  return color_utils::HSLToSkColor(hsl, SkColorGetA(accent_color.value()));
}

std::optional<SkColor> NativeThemeBase::GetContrastingColorForScrollbarPart(
    std::optional<SkColor> fg_color,
    std::optional<SkColor> bg_color,
    State state) const {
  if (!fg_color.has_value() || (state != kHovered && state != kPressed) ||
      SkColorGetA(fg_color.value()) == SK_AlphaTRANSPARENT) {
    return fg_color;
  }
  const SkColor resulting_color =
      color_utils::BlendForMinContrast(
          fg_color.value(), SkColorSetA(fg_color.value(), SK_AlphaOPAQUE),
          std::nullopt, GetScrollbarPartContrastRatioForState(state))
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

void NativeThemeBase::PaintCheckbox(cc::PaintCanvas* canvas,
                                    const ColorProvider* color_provider,
                                    State state,
                                    const gfx::Rect& rect,
                                    const ButtonExtraParams& extra_params,
                                    bool dark_mode,
                                    PreferredContrast contrast,
                                    std::optional<SkColor> accent_color) const {
  // Paint the background and border.
  const float radius =
      GetBorderRadiusForPart(kCheckbox, rect.width(), rect.height());
  const SkRect skrect = PaintCheckboxRadioCommon(
      canvas, color_provider, state, rect, extra_params, true, radius,
      dark_mode, contrast, accent_color);
  if (skrect.isEmpty()) {
    return;
  }

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  if (extra_params.indeterminate || extra_params.checked) {
    // Paint an accent-colored background.
    flags.setColor(GetAccentOrControlColorForState(accent_color, kAccentColors,
                                                   state, dark_mode, contrast,
                                                   color_provider));
    canvas->drawRoundRect(skrect, radius, radius, flags);
  }
  flags.setColor(GetControlColorForState(kCheckboxBackgroundColors, state,
                                         dark_mode, contrast, color_provider));
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
    SkPathBuilder check;
    check.moveTo(skrect.x() + skrect.width() * 0.2f, skrect.centerY());
    check.rLineTo(skrect.width() * 0.2f, skrect.height() * 0.2f);
    check.lineTo(skrect.right() - skrect.width() * 0.2f,
                 skrect.y() + skrect.height() * 0.2f);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(skrect.height() * 0.16f);
    canvas->drawPath(check.detach(), flags);
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
  (extra_params.spin_up ? decrease_state : increase_state) =
      (state == kDisabled) ? kDisabled : kNormal;

  const ScrollbarArrowExtraParams arrow = {.zoom = 1.0f};
  if (extra_params.spin_arrows_direction == SpinArrowsDirection::kUpDown) {
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
                                    bool dark_mode,
                                    PreferredContrast contrast) const {
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
    PaintTextField(canvas, color_provider, state, rect, params, dark_mode,
                   contrast);
  }

  // The arrow base is twice the arrow height, giving 45 degree sides.
  static constexpr float kAspectRatio = 2.0f;

  SkPathBuilder path;
  if (extra_params.arrow_direction == ArrowDirection::kDown) {
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
    path.lineTo(arrow.x() + arrow.width() / 2, arrow.bottom());
    path.lineTo(arrow.right(), arrow.y());
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
    if (arrow.width() != intended_width || arrow.height() != intended_height) {
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
  canvas->drawPath(path.detach(), flags);
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

  const SliderExtraParams slider = {.vertical = !extra_params.is_horizontal};
  // Sliders in a larger area keep the same thickness while progress bars scale,
  // but using the slider sizes in a ratio here means that when both are drawn
  // in a default-sized space (i.e. thickness = `kSliderThumbThickness`), both
  // will be the same thickness.
  const float thickness = (slider.vertical ? rect.width() : rect.height()) *
                          static_cast<float>(kSliderTrackThickness) /
                          kSliderThumbThickness;
  const SkRect track_rect = AlignSliderTrack(rect, slider, false, thickness);
  const float radius = GetBorderRadiusForPart(kProgressBar, track_rect.width(),
                                              track_rect.height());

  // Paint the track.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(GetControlColorForState(kFillColors, state, dark_mode,
                                         contrast, color_provider));
  canvas->drawRoundRect(track_rect, radius, radius, flags);

  // Paint the progress value bar.
  int value_height = extra_params.value_rect_height;
  int value_width = extra_params.value_rect_width;
  int& value_length = slider.vertical ? value_height : value_width;
  if (value_length > 0) {
    value_length = std::max(value_length, 2);
  }
  const SkRect value_rect = AlignSliderTrack(
      gfx::Rect(extra_params.value_rect_x, extra_params.value_rect_y,
                value_width, value_height),
      slider, false, thickness);
  flags.setColor(GetAccentOrControlColorForState(
      accent_color, kAccentColors, state, dark_mode, contrast, color_provider));
  if (extra_params.determinate) {
    canvas->clipRRect(SkRRect::MakeRectXY(track_rect, radius, radius), true);
    canvas->drawRect(value_rect, flags);
  } else {
    canvas->drawRoundRect(value_rect, radius, radius, flags);
  }

  // Paint the border.
  const float border_width =
      AdjustBorderWidthByZoom(kBorderWidth, extra_params.zoom);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(border_width);
  flags.setColor(GetControlColorForState(kSliderBorderColors, state, dark_mode,
                                         contrast, color_provider));
  canvas->drawRoundRect(
      track_rect.makeInset(border_width / 2, border_width / 2), radius, radius,
      flags);
}

void NativeThemeBase::PaintButton(cc::PaintCanvas* canvas,
                                  const ColorProvider* color_provider,
                                  State state,
                                  const gfx::Rect& rect,
                                  const ButtonExtraParams& extra_params,
                                  bool dark_mode,
                                  PreferredContrast contrast) const {
  SkRect skrect = gfx::RectToSkRect(rect);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  static constexpr auto kButtonFillColors =
      std::to_array({kButtonDisabledFill, kButtonHoveredFill, kButtonFill,
                     kButtonPressedFill});
  flags.setColor(GetControlColorForState(kButtonFillColors, state, dark_mode,
                                         contrast, color_provider));

  // If the button is too small, fallback to drawing a solid color rect.
  if (rect.width() < 5 || rect.height() < 5) {
    canvas->drawRect(skrect, flags);
    return;
  }

  const float border_width =
      AdjustBorderWidthByZoom(kBorderWidth, extra_params.zoom);
  skrect.inset(border_width / 2, border_width / 2);
  const float radius = AdjustBorderRadiusByZoom(
      kPushButton,
      GetBorderRadiusForPart(kPushButton, skrect.width(), skrect.height()),
      extra_params.zoom);

  // Paint the background.
  PaintLightenLayer(canvas, color_provider, skrect, state, radius, dark_mode,
                    contrast);
  canvas->drawRoundRect(skrect, radius, radius, flags);

  // Paint the border.
  if (extra_params.has_border) {
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(border_width);
    flags.setColor(GetControlColorForState(
        kButtonBorderColors, state, dark_mode, contrast, color_provider));
    canvas->drawRoundRect(skrect, radius, radius, flags);
  }
}

void NativeThemeBase::PaintRadio(cc::PaintCanvas* canvas,
                                 const ColorProvider* color_provider,
                                 State state,
                                 const gfx::Rect& rect,
                                 const ButtonExtraParams& extra_params,
                                 bool dark_mode,
                                 PreferredContrast contrast,
                                 std::optional<SkColor> accent_color) const {
  // Paint the background and border.
  const float radius =
      GetBorderRadiusForPart(kRadio, rect.width(), rect.height());
  const SkRect skrect = PaintCheckboxRadioCommon(
      canvas, color_provider, state, rect, extra_params, false, radius,
      dark_mode, contrast, accent_color);
  if (skrect.isEmpty() || !extra_params.checked) {
    return;
  }

  // Paint the dot.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(GetAccentOrControlColorForState(
      accent_color, kAccentColors, state, dark_mode, contrast, color_provider));
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
  const float radius = GetBorderRadiusForPart(kSliderTrack, track_rect.width(),
                                              track_rect.height());
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
  flags.setColor(GetControlColorForState(kFillColors, state, dark_mode,
                                         contrast, color_provider));
  canvas->drawRoundRect(track_rect, radius, radius, flags);

  {
    // Paint the value bar, clipped to its extent.
    gfx::Canvas gfx_canvas(canvas, 1.0f);
    gfx::ScopedCanvas scoped_canvas(&gfx_canvas);
    canvas->clipRect(AlignSliderTrack(rect, extra_params, true, track_height),
                     true);
    flags.setColor(GetAccentOrControlColorForState(accent_color, kSliderColors,
                                                   state, dark_mode, contrast,
                                                   color_provider));
    canvas->drawRoundRect(track_rect, radius, radius, flags);
  }

  // Paint the border.
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  const float border_width =
      AdjustBorderWidthByZoom(kBorderWidth, extra_params.zoom);
  flags.setStrokeWidth(border_width);
  flags.setColor(GetControlColorForState(kSliderBorderColors, state, dark_mode,
                                         contrast, color_provider));
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
    PreferredContrast contrast,
    std::optional<SkColor> accent_color) const {
  const SkRect thumb_rect = gfx::RectToSkRect(rect);
  const float radius = GetBorderRadiusForPart(kSliderThumb, thumb_rect.width(),
                                              thumb_rect.height());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(GetAccentOrControlColorForState(
      accent_color, kSliderColors, state, dark_mode, contrast, color_provider));
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
                                     bool dark_mode,
                                     PreferredContrast contrast) const {
  SkRect bounds = gfx::RectToSkRect(rect);
  const float border_width =
      AdjustBorderWidthByZoom(kBorderWidth, extra_params.zoom);
  bounds.inset(border_width / 2, border_width / 2);
  const float radius = AdjustBorderRadiusByZoom(
      kTextField,
      GetBorderRadiusForPart(kTextField, bounds.width(), bounds.height()),
      extra_params.zoom);

  // Paint the background.
  const bool paint_autocomplete_background =
      extra_params.auto_complete_active && state != kDisabled;
  if (paint_autocomplete_background ||
      SkColorGetA(extra_params.background_color)) {
    PaintLightenLayer(canvas, color_provider, bounds, state, radius, dark_mode,
                      contrast);
    cc::PaintFlags bg_flags;
    // TODO(crbug.com/446078854): The current background color seems wrong; it
    // was done in
    // https://crrev.com/c/1792578/5..7/ui/native_theme/native_theme_aura.cc
    // ...due to feedback that the previous version of that patch, which used
    // CSS to add a white hover effect, was causing interop problems.
    // Unfortunately the new version discarded the passed-in background color
    // entirely, likely due to oversight, and affected far more than just the
    // hover state.
    //
    // The disabled branch here contains what pkasting thinks is the correct
    // code; however, this may have significant visual effect and possible web
    // compat ramifications, and may need to be discussed with Blink forms/a11y
    // folks. This might require Blink-side changes and/or WPT updates in
    // addition to simple rebaselines.
    SkColor default_bg_color;
    if constexpr (false) {
      // Proposed behavior
      default_bg_color = GetAccentOrControlColorForState(
          extra_params.background_color,
          kFillColors /* Doesn't matter, won't be used */,
          // Not certain of the correct disabled state behavior
          (state == kDisabled) ? kNormal : state, dark_mode, contrast,
          color_provider);
    } else {
      // Status quo behavior
      default_bg_color =
          GetControlColorForState(kCheckboxBackgroundColors, state, dark_mode,
                                  contrast, color_provider);
    }
    bg_flags.setColor(paint_autocomplete_background
                          ? GetControlColor(kAutoCompleteBackground, dark_mode,
                                            contrast, color_provider)
                          : default_bg_color);
    canvas->drawRoundRect(bounds, radius, radius, bg_flags);
  }

  // Paint the border.
  if (extra_params.has_border) {
    cc::PaintFlags border_flags;
    border_flags.setColor(GetControlColorForState(
        kBorderColors, state, dark_mode, contrast, color_provider));
    border_flags.setStyle(cc::PaintFlags::kStroke_Style);
    border_flags.setStrokeWidth(border_width);
    canvas->drawRoundRect(bounds, radius, radius, border_flags);
  }
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
    PreferredContrast contrast,
    std::optional<SkColor> accent_color) const {
  // Use the largest square that fits inside the provided rectangle. This
  // matches other browsers.
  gfx::RectF rect_f(rect);
  const float min_side = std::min(rect_f.width(), rect_f.height());
  rect_f.ClampToCenteredSize({min_side, min_side});
  const SkRect skrect = gfx::RectFToSkRect(rect_f);
  const SkColor border_color =
      extra_params.checked
          ? GetAccentOrControlColorForState(accent_color, kAccentColors, state,
                                            dark_mode, contrast, color_provider)
          : GetControlColorForState(kBorderColors, state, dark_mode, contrast,
                                    color_provider);

  // If the square is too small then paint only a square.
  cc::PaintFlags flags;
  if (skrect.width() <= 2) {
    flags.setColor(border_color);
    canvas->drawRect(skrect, flags);
    return {};  // Don't draw anything more.
  }

  // Paint the background.
  // Shrink the rect slightly to avoid antialiasing artifacts with the border.
  const auto background_rect =
      skrect.makeInset(kBorderWidth * 0.2f, kBorderWidth * 0.2f);
  PaintLightenLayer(canvas, color_provider, background_rect, state,
                    border_radius, dark_mode, contrast);
  flags.setAntiAlias(true);
  flags.setColor(GetControlColorForState(kCheckboxBackgroundColors, state,
                                         dark_mode, contrast, color_provider));
  canvas->drawRoundRect(background_rect, border_radius, border_radius, flags);

  // Paint the border.
  // Indeterminate and checked checkboxes do not draw a border; they will draw
  // an accent-colored background instead on the caller side.
  if (!is_checkbox || (!extra_params.checked && !extra_params.indeterminate)) {
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
