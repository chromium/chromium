// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_aura.h"

#include <optional>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
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

gfx::Insets NativeThemeAura::GetScrollbarSolidColorThumbInsets(
    Part part) const {
  if (use_overlay_scrollbar()) {
    return {};
  }

  static constexpr int kThumbPadding = 2;
  auto insets = gfx::Insets::VH(
      // If there are no buttons then provide some padding so that the thumb
      // doesn't touch the end of the track.
      GetVerticalScrollbarButtonSize().IsEmpty() ? kThumbPadding : 0,
      kThumbPadding);
  if (part == kScrollbarHorizontalTrack) {
    insets.Transpose();
  }
  return insets;
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

NativeThemeAura::NativeThemeAura(bool use_overlay_scrollbar) {
  set_use_overlay_scrollbar(use_overlay_scrollbar);
}

NativeThemeAura::NativeThemeAura(SystemTheme system_theme)
    : NativeThemeBase(system_theme) {}

NativeThemeAura::~NativeThemeAura() = default;

gfx::Size NativeThemeAura::GetVerticalScrollbarButtonSize() const {
  gfx::Size size = NativeThemeBase::GetVerticalScrollbarButtonSize();
  if (use_overlay_scrollbar()) {
    // NOTE: The overlay scrollbar thumb omits the stroke on the trailing
    // "thickness" edge, so only including its width a single time here is
    // intentional.
    size.set_width(kOverlayScrollbarThumbWidthPressed +
                   kOverlayScrollbarStrokeWidth);
  }
#if BUILDFLAG(IS_CHROMEOS)
  // CrOS does not draw scrollbar buttons. Be careful to leave the width valid,
  // however, as that value is also used for the track width.
  size.set_height(0);
#endif
  return size;
}

gfx::Size NativeThemeAura::GetVerticalScrollbarThumbSize() const {
  if (use_overlay_scrollbar()) {
    return gfx::Size(GetVerticalScrollbarButtonSize().width(),
                     32 + 2 * kOverlayScrollbarStrokeWidth);
  }
  return NativeThemeBase::GetVerticalScrollbarThumbSize();
}

std::optional<ColorId> NativeThemeAura::GetScrollbarThumbColorId(
    State state,
    const ScrollbarThumbExtraParams& extra_params) const {
  if (!use_overlay_scrollbar()) {
    return std::nullopt;
  }
  return (state == kHovered || state == kPressed)
             ? kColorOverlayScrollbarFillHovered
             : kColorOverlayScrollbarFill;
}

float NativeThemeAura::GetScrollbarPartContrastRatioForState(
    State state) const {
  return state == kPressed ? 1.3f : 1.8f;
}

void NativeThemeAura::PaintMenuPopupBackground(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& extra_params) const {
  CHECK(color_provider);
  // TODO(crbug.com/40219248): Use `SkColor4f` everywhere.
  const auto color =
      SkColor4f::FromColor(color_provider->GetColor(kColorMenuBackground));
  if (extra_params.corner_radius > 0) {
    const SkScalar r = SkIntToScalar(extra_params.corner_radius);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(color);
    canvas->drawRoundRect(gfx::RectToSkRect(gfx::Rect(size)), r, r, flags);
  } else {
    canvas->drawColor(color, SkBlendMode::kSrc);
  }
}

void NativeThemeAura::PaintArrowButton(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const gfx::Rect& rect,
    Part part,
    State state,
    bool forced_colors,
    bool dark_mode,
    PreferredContrast contrast,
    const ScrollbarArrowExtraParams& extra_params) const {
  const SkColor bg_color = GetScrollbarArrowBackgroundColor(
      extra_params, state, dark_mode, contrast, color_provider);
  cc::PaintFlags bg_flags;
  bg_flags.setColor(bg_color);

  if (base::FeatureList::IsEnabled(kNewScrollbarArrowRadius) &&
      !extra_params.needs_rounded_corner) {
    canvas->drawRect(gfx::RectToSkRect(rect), bg_flags);
  } else {
    // This radius lets scrollbar arrows fit in the default rounded border of
    // some form controls.
    // TODO(crbug.com/40285711): We should probably let blink pass the actual
    // border radii.
    static constexpr SkScalar kUnscaledRadius = 1;
    const SkScalar radius =
        kUnscaledRadius * (extra_params.zoom ? extra_params.zoom : 1.0);
    SkScalar ul = 0, ll = 0, ur = 0, lr = 0;
    if (part == kScrollbarDownArrow) {
      (extra_params.right_to_left ? ll : lr) = radius;
    } else if (part == kScrollbarLeftArrow) {
      ll = radius;
    } else if (part == kScrollbarRightArrow) {
      lr = radius;
    } else if (part == kScrollbarUpArrow) {
      (extra_params.right_to_left ? ul : ur) = radius;
    }
    const gfx::RRectF rrect(gfx::RectF(rect), ul, ul, ur, ur, lr, lr, ll, ll);
    bg_flags.setAntiAlias(true);
    canvas->drawRRect(static_cast<SkRRect>(rrect), bg_flags);
  }

  PaintArrow(
      canvas, rect, part, state,
      GetScrollbarArrowForegroundColor(bg_color, extra_params, state, dark_mode,
                                       contrast, color_provider));
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
  fill_rect.Inset(GetScrollbarSolidColorThumbInsets(part));
  if (use_overlay_scrollbar()) {
    // Paint a stroke.
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

    cc::PaintFlags stroke_flags;
    CHECK(color_provider);
    stroke_flags.setColor(color_provider->GetColor(
        state == kNormal ? kColorOverlayScrollbarStroke
                         : kColorOverlayScrollbarStrokeHovered));
    stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
    stroke_flags.setStrokeWidth(kOverlayScrollbarStrokeWidth);
    canvas->drawRect(gfx::RectFToSkRect(stroke_rect), stroke_flags);

    // Inset all the edges so we don't fill in the stroke below. For a left
    // vertical scrollbar, we will horizontally flip the canvas in
    // `ScrollbarThemeOverlay::PaintThumb()`.
    fill_rect.Inset(gfx::Insets(kOverlayScrollbarStrokeWidth) +
                    edge_adjust_insets);
  }

  cc::PaintFlags fill_flags;
  fill_flags.setColor(
      GetScrollbarThumbColor(color_provider, state, extra_params));
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
      GetControlColor(kScrollbarTrack, {}, {}, color_provider)));
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
      GetControlColor(kScrollbarCornerControlColorId, {}, {}, color_provider)));
  canvas->drawIRect(RectToSkIRect(rect), flags);
}

}  // namespace ui
