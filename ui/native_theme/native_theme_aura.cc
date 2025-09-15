// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_aura.h"

#include <optional>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_base.h"
#include "ui/native_theme/overlay_scrollbar_constants.h"

namespace ui {

namespace {

// The border is 2 pixels despite the stroke width being 1 so that the inner
// pixel can match the center tile color. This prevents color interpolation
// between the patches.
constexpr gfx::Insets kOverlayScrollbarBorderInsets(2);

// Killswitch for the changed behavior (only drawing rounded corner for form
// controls). Should remove after M120 ships.
BASE_FEATURE(kNewScrollbarArrowRadius, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

gfx::Size NativeThemeAura::GetPartSize(Part part,
                                       State state,
                                       const ExtraParams& extra_params) const {
  if (use_overlay_scrollbar()) {
    constexpr int minimum_length = 32 + 2 * kOverlayScrollbarStrokeWidth;

    // Aura overlay scrollbars need a slight tweak from the base sizes.
    switch (part) {
      case kScrollbarHorizontalThumb:
        return gfx::Size(minimum_length, scrollbar_width_);
      case kScrollbarVerticalThumb:
        return gfx::Size(scrollbar_width_, minimum_length);

      default:
        // TODO(bokan): We should probably make sure code using overlay
        // scrollbars isn't asking for part sizes that don't exist.
        // crbug.com/657159.
        break;
    }
  }

  return NativeThemeBase::GetPartSize(part, state, extra_params);
}

gfx::Insets NativeThemeAura::GetScrollbarSolidColorThumbInsets(
    Part part) const {
  if (use_overlay_scrollbar()) {
    return {};
  }

  // If there are no buttons then provide some padding so that the thumb doesn't
  // touch the end of the track.
  static constexpr int kThumbPadding = 2;
  const int extra_inset = scrollbar_button_length() == 0 ? kThumbPadding : 0;
  if (part == NativeTheme::kScrollbarVerticalThumb) {
    return gfx::Insets::VH(extra_inset, kThumbPadding);
  }
  CHECK_EQ(part, NativeTheme::kScrollbarHorizontalThumb);
  return gfx::Insets::VH(kThumbPadding, extra_inset);
}

bool NativeThemeAura::SupportsNinePatch(Part part) const {
  return use_overlay_scrollbar() &&
         (part == kScrollbarHorizontalThumb || part == kScrollbarVerticalThumb);
}

gfx::Size NativeThemeAura::GetNinePatchCanvasSize(Part part) const {
  CHECK(SupportsNinePatch(part));

  static constexpr int kCenterPatchSize = 1;
  return gfx::Size(kOverlayScrollbarBorderInsets.width() + kCenterPatchSize,
                   kOverlayScrollbarBorderInsets.height() + kCenterPatchSize);
}

gfx::Rect NativeThemeAura::GetNinePatchAperture(Part part) const {
  CHECK(SupportsNinePatch(part));

  gfx::Rect aperture(GetNinePatchCanvasSize(part));
  aperture.Inset(kOverlayScrollbarBorderInsets);
  return aperture;
}

SkColor NativeThemeAura::GetScrollbarThumbColor(
    const ui::ColorProvider* color_provider,
    State state,
    const ScrollbarThumbExtraParams& extra_params) const {
  // Only non-overlay aura scrollbars use solid color thumb.
  CHECK(!use_overlay_scrollbar());
  if (extra_params.thumb_color.has_value()) {
    return GetContrastingPressedOrHoveredColor(
               extra_params.thumb_color, extra_params.track_color, state,
               /*part=*/Part::kScrollbarVerticalThumb)
        .value();
  }
  ColorId color_id = kColorWebNativeControlScrollbarThumb;
  if (state == NativeTheme::kHovered) {
    color_id = kColorWebNativeControlScrollbarThumbHovered;
  } else if (state == NativeTheme::kPressed) {
    color_id = kColorWebNativeControlScrollbarThumbPressed;
  }
  return color_provider->GetColor(color_id);
}

NativeThemeAura::NativeThemeAura(bool use_overlay_scrollbar) {
  set_use_overlay_scrollbar(use_overlay_scrollbar);
  if (use_overlay_scrollbar) {
    scrollbar_width_ =
        kOverlayScrollbarThumbWidthPressed + kOverlayScrollbarStrokeWidth;
  }
  // We don't draw scrollbar buttons.
#if BUILDFLAG(IS_CHROMEOS)
  set_scrollbar_button_length(0);
#endif
}

NativeThemeAura::NativeThemeAura(SystemTheme system_theme)
    : NativeThemeBase(system_theme) {
  // We don't draw scrollbar buttons.
#if BUILDFLAG(IS_CHROMEOS)
  set_scrollbar_button_length(0);
#endif
}

NativeThemeAura::~NativeThemeAura() = default;

float NativeThemeAura::GetContrastRatioForState(State state, Part part) const {
  return state == kPressed ? 1.3f : 1.8f;
}

void NativeThemeAura::PaintMenuPopupBackground(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& extra_params) const {
  DCHECK(color_provider);
  // TODO(crbug.com/40219248): Remove FromColor and make all SkColor4f.
  SkColor4f color =
      SkColor4f::FromColor(color_provider->GetColor(kColorMenuBackground));
  if (extra_params.corner_radius > 0) {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    flags.setColor(color);

    SkPath path;
    SkRect rect = SkRect::MakeWH(SkIntToScalar(size.width()),
                                 SkIntToScalar(size.height()));
    SkScalar radius = SkIntToScalar(extra_params.corner_radius);
    SkScalar radii[8] = {radius, radius, radius, radius,
                         radius, radius, radius, radius};
    path.addRoundRect(rect, radii);

    canvas->drawPath(path, flags);
  } else {
    canvas->drawColor(color, SkBlendMode::kSrc);
  }
}

void NativeThemeAura::PaintArrowButton(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const gfx::Rect& rect,
    Part direction,
    State state,
    bool forced_colors,
    bool dark_mode,
    PreferredContrast contrast,
    const ScrollbarArrowExtraParams& extra_params) const {
  SkColor bg_color =
      GetControlColor(kScrollbarArrowBackground, dark_mode, color_provider);
  // Aura-win uses slightly different arrow colors.
  SkColor arrow_color = gfx::kPlaceholderColor;
  switch (state) {
    case kDisabled:
      arrow_color = GetArrowColor(state, dark_mode, color_provider);
      break;
    case kHovered:
      bg_color = GetControlColor(kScrollbarArrowBackgroundHovered, dark_mode,
                                 color_provider);
      arrow_color =
          GetControlColor(kScrollbarArrowHovered, dark_mode, color_provider);
      break;
    case kNormal:
      arrow_color = GetControlColor(kScrollbarArrow, dark_mode, color_provider);
      break;
    case kPressed:
      bg_color = GetControlColor(kScrollbarArrowBackgroundPressed, dark_mode,
                                 color_provider);
      arrow_color =
          GetControlColor(kScrollbarArrowPressed, dark_mode, color_provider);
      break;
    case kNumStates:
      break;
  }
  if (extra_params.thumb_color.has_value() &&
      extra_params.thumb_color.value() == gfx::kPlaceholderColor) {
    // TODO(crbug.com/40278836): Remove this and the below checks for
    // placeholderColor.
    DLOG(ERROR) << "thumb_color with a placeholderColor value encountered";
  }
  if (extra_params.track_color.has_value() &&
      extra_params.track_color.value() != gfx::kPlaceholderColor) {
    bg_color = GetContrastingPressedOrHoveredColor(extra_params.track_color,
                                                   /*bg_color=*/std::nullopt,
                                                   state, direction)
                   .value();
  }
  if (extra_params.thumb_color.has_value() &&
      extra_params.thumb_color.value() != gfx::kPlaceholderColor) {
    arrow_color = GetContrastingPressedOrHoveredColor(
                      extra_params.thumb_color, bg_color, state, direction)
                      .value();
  }
  DCHECK_NE(arrow_color, gfx::kPlaceholderColor);

  cc::PaintFlags flags;
  flags.setColor(bg_color);

  if (base::FeatureList::IsEnabled(kNewScrollbarArrowRadius) &&
      !extra_params.needs_rounded_corner) {
    canvas->drawIRect(gfx::RectToSkIRect(rect), flags);
  } else {
    // This radius lets scrollbar arrows fit in the default rounded border of
    // some form controls.
    // TODO(crbug.com/40285711): We should probably let blink pass the actual
    // border radii.
    static constexpr SkScalar kUnscaledRadius = 1;
    const SkScalar radius =
        kUnscaledRadius * (extra_params.zoom ? extra_params.zoom : 1.0);
    SkScalar upper_left_radius = 0;
    SkScalar lower_left_radius = 0;
    SkScalar upper_right_radius = 0;
    SkScalar lower_right_radius = 0;
    if (direction == kScrollbarUpArrow) {
      if (extra_params.right_to_left) {
        upper_left_radius = radius;
      } else {
        upper_right_radius = radius;
      }
    } else if (direction == kScrollbarDownArrow) {
      if (extra_params.right_to_left) {
        lower_left_radius = radius;
      } else {
        lower_right_radius = radius;
      }
    }
    gfx::RRectF rounded_rect(
        gfx::RectF(rect), upper_left_radius, upper_left_radius,
        upper_right_radius, upper_right_radius, lower_right_radius,
        lower_right_radius, lower_left_radius, lower_left_radius);
    canvas->drawRRect(static_cast<SkRRect>(rounded_rect), flags);
  }

  PaintArrow(canvas, rect, direction, arrow_color);
}

void NativeThemeAura::PaintScrollbarThumb(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    Part part,
    State state,
    const gfx::Rect& rect,
    const ScrollbarThumbExtraParams& extra_params) const {
  if (state == kDisabled) {
    return;
  }

  TRACE_EVENT0("blink", "NativeThemeAura::PaintScrollbarThumb");

  gfx::Rect fill_rect(rect);
  cc::PaintFlags fill_flags;

  if (use_overlay_scrollbar()) {
    const bool hovered = state != kNormal;

    CHECK(color_provider);
    fill_flags.setColor(extra_params.thumb_color.value_or(
        color_provider->GetColor(hovered ? kColorOverlayScrollbarFillHovered
                                         : kColorOverlayScrollbarFill)));

    // In overlay mode, draw a stroke (border).
    cc::PaintFlags stroke_flags;
    stroke_flags.setColor(
        color_provider->GetColor(hovered ? kColorOverlayScrollbarStrokeHovered
                                         : kColorOverlayScrollbarStroke));
    stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
    stroke_flags.setStrokeWidth(kOverlayScrollbarStrokeWidth);

    gfx::RectF stroke_rect(fill_rect);
    // The edge to which the scrollbar is attached shouldn't have a border.
    gfx::Insets edge_adjust_insets;
    if (part == NativeTheme::kScrollbarHorizontalThumb) {
      edge_adjust_insets.set_bottom(-kOverlayScrollbarStrokeWidth);
    } else {
      edge_adjust_insets.set_right(-kOverlayScrollbarStrokeWidth);
    }
    stroke_rect.Inset(gfx::InsetsF(kOverlayScrollbarStrokeWidth / 2.0f) +
                      static_cast<gfx::InsetsF>(edge_adjust_insets));
    canvas->drawRect(gfx::RectFToSkRect(stroke_rect), stroke_flags);

    // Inset all the edges so we don't fill in the stroke below. For a left
    // vertical scrollbar, we will horizontally flip the canvas in
    // `ScrollbarThemeOverlay::PaintThumb()`.
    gfx::Insets fill_insets(kOverlayScrollbarStrokeWidth);
    fill_rect.Inset(fill_insets + edge_adjust_insets);
  } else {
    fill_rect.Inset(GetScrollbarSolidColorThumbInsets(part));
    fill_flags.setColor(
        GetScrollbarThumbColor(color_provider, state, extra_params));
  }

  canvas->drawIRect(gfx::RectToSkIRect(fill_rect), fill_flags);
}

void NativeThemeAura::PaintScrollbarTrack(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect,
    bool forced_colors,
    PreferredContrast contrast) const {
  CHECK(!use_overlay_scrollbar());

  cc::PaintFlags flags;
  flags.setColor(extra_params.track_color.value_or(
      GetControlColor(kScrollbarTrack, {}, color_provider)));
  canvas->drawIRect(gfx::RectToSkIRect(rect), flags);
}

void NativeThemeAura::PaintScrollbarCorner(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const ScrollbarTrackExtraParams& extra_params) const {
  CHECK(!use_overlay_scrollbar());

  cc::PaintFlags flags;
  flags.setColor(extra_params.track_color.value_or(
      GetControlColor(kScrollbarCornerControlColorId, {}, color_provider)));
  canvas->drawIRect(RectToSkIRect(rect), flags);
}

}  // namespace ui
