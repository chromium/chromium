// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_aura.h"

#include <limits>
#include <utility>

#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme_features.h"
#include "ui/native_theme/native_theme_fluent.h"
#include "ui/native_theme/overlay_scrollbar_constants_aura.h"

namespace ui {

namespace {

// Constants for painting overlay scrollbars. Other properties needed outside
// this painting code are defined in overlay_scrollbar_constants_aura.h.
constexpr int kOverlayScrollbarMinimumLength = 32;
// 2 pixel border with 1 pixel center patch. The border is 2 pixels despite the
// stroke width being 1 so that the inner pixel can match the center tile
// color. This prevents color interpolation between the patches.
constexpr int kOverlayScrollbarBorderPatchWidth = 2;
constexpr int kOverlayScrollbarCenterPatchSize = 1;

// This radius let scrollbar arrows fit in the default rounded border of some
// form controls. TODO(crbug.com/40285711): We should probably let blink pass
// the actual border radii.
const SkScalar kScrollbarArrowRadius = 1;
// Killswitch for the changed behavior (only drawing rounded corner for form
// controls). Should remove after M120 ships.
BASE_FEATURE(kNewScrollbarArrowRadius,
             "NewScrollbarArrowRadius",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NativeTheme:

#if !BUILDFLAG(IS_APPLE)
// static
NativeTheme* NativeTheme::GetInstanceForWeb() {
  if (IsFluentScrollbarEnabled()) {
    return NativeThemeFluent::web_instance();
  }
  return NativeThemeAura::web_instance();
}

#if !BUILDFLAG(IS_WIN)
// static
NativeTheme* NativeTheme::GetInstanceForNativeUi() {
  static base::NoDestructor<NativeThemeAura> s_native_theme(
      /*use_overlay_scrollbars=*/false,
      /*should_only_use_dark_colors=*/false,
      /*system_theme=*/ui::SystemTheme::kDefault,
      /*configure_web_instance=*/true);
  return s_native_theme.get();
}

NativeTheme* NativeTheme::GetInstanceForDarkUI() {
  static base::NoDestructor<NativeThemeAura> s_native_theme(
      /*use_overlay_scrollbars=*/false,
      /*should_only_use_dark_colors=*/true);
  return s_native_theme.get();
}
#endif  // !BUILDFLAG(IS_WIN)
#endif  // !BUILDFLAG(IS_APPLE)

////////////////////////////////////////////////////////////////////////////////
// NativeThemeAura:

NativeThemeAura::NativeThemeAura(bool use_overlay_scrollbars,
                                 bool should_only_use_dark_colors,
                                 ui::SystemTheme system_theme,
                                 bool configure_web_instance)
    : NativeThemeBase(should_only_use_dark_colors, system_theme),
      use_overlay_scrollbars_(use_overlay_scrollbars) {
// We don't draw scrollbar buttons.
#if BUILDFLAG(IS_CHROMEOS)
  set_scrollbar_button_length(0);
#endif

  if (use_overlay_scrollbars_) {
    scrollbar_width_ =
        kOverlayScrollbarThumbWidthPressed + kOverlayScrollbarStrokeWidth;
  }

  // Images and alphas declarations assume the following order.
  static_assert(kDisabled == 0, "states unexpectedly changed");
  static_assert(kHovered == 1, "states unexpectedly changed");
  static_assert(kNormal == 2, "states unexpectedly changed");
  static_assert(kPressed == 3, "states unexpectedly changed");

  if (configure_web_instance) {
    ConfigureWebInstance();
  }
}

NativeThemeAura::~NativeThemeAura() {}

// static
NativeThemeAura* NativeThemeAura::web_instance() {
  static base::NoDestructor<NativeThemeAura> s_native_theme_for_web(
      /*use_overlay_scrollbars=*/IsOverlayScrollbarEnabled(),
      /*should_only_use_dark_colors=*/false);
  return s_native_theme_for_web.get();
}

SkColor4f NativeThemeAura::FocusRingColorForBaseColor(
    SkColor4f base_color) const {
#if BUILDFLAG(IS_APPLE)
  // On Mac OSX, the system Accent Color setting is darkened a bit
  // for better contrast.
  return SkColor4f(base_color.fR, base_color.fG, base_color.fB, 166 / 255.0f);
#else
  return base_color;
#endif  // BUILDFLAG(IS_APPLE)
}

void NativeThemeAura::ConfigureWebInstance() {
  // Add the web native theme as an observer to stay in sync with color scheme
  // changes.
  color_scheme_observer_ =
      std::make_unique<NativeTheme::ColorSchemeNativeThemeObserver>(
          NativeTheme::GetInstanceForWeb());
  AddObserver(color_scheme_observer_.get());
}

void NativeThemeAura::PaintMenuPopupBackground(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background,
    ColorScheme color_scheme) const {
  DCHECK(color_provider);
  // TODO(crbug.com/40219248): Remove FromColor and make all SkColor4f.
  SkColor4f color =
      SkColor4f::FromColor(color_provider->GetColor(kColorMenuBackground));
  if (menu_background.corner_radius > 0) {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    flags.setColor(color);

    SkPath path;
    SkRect rect = SkRect::MakeWH(SkIntToScalar(size.width()),
                                 SkIntToScalar(size.height()));
    SkScalar radius = SkIntToScalar(menu_background.corner_radius);
    SkScalar radii[8] = {radius, radius, radius, radius,
                         radius, radius, radius, radius};
    path.addRoundRect(rect, radii);

    canvas->drawPath(path, flags);
  } else {
    canvas->drawColor(color, SkBlendMode::kSrc);
  }
}

void NativeThemeAura::PaintMenuItemBackground(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const MenuItemExtraParams& menu_item,
    ColorScheme color_scheme) const {
  CommonThemePaintMenuItemBackground(this, color_provider, canvas, state, rect,
                                     menu_item);
}

void NativeThemeAura::PaintArrowButton(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const gfx::Rect& rect,
    Part direction,
    State state,
    ColorScheme color_scheme,
    bool in_forced_colors,
    const ScrollbarArrowExtraParams& extra_params) const {
  SkColor bg_color =
      GetControlColor(kScrollbarArrowBackground, color_scheme, color_provider);
  // Aura-win uses slightly different arrow colors.
  SkColor arrow_color = gfx::kPlaceholderColor;
  switch (state) {
    case kDisabled:
      arrow_color = GetArrowColor(state, color_scheme, color_provider);
      break;
    case kHovered:
      bg_color = GetControlColor(kScrollbarArrowBackgroundHovered, color_scheme,
                                 color_provider);
      arrow_color =
          GetControlColor(kScrollbarArrowHovered, color_scheme, color_provider);
      break;
    case kNormal:
      arrow_color =
          GetControlColor(kScrollbarArrow, color_scheme, color_provider);
      break;
    case kPressed:
      bg_color = GetControlColor(kScrollbarArrowBackgroundPressed, color_scheme,
                                 color_provider);
      arrow_color =
          GetControlColor(kScrollbarArrowPressed, color_scheme, color_provider);
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
  if (extra_params.thumb_color.has_value() &&
      extra_params.thumb_color.value() != gfx::kPlaceholderColor) {
    // TODO(crbug.com/40596569): Adjust thumb_color based on `state`.
    arrow_color = extra_params.thumb_color.value();
  }
  if (extra_params.track_color.has_value() &&
      extra_params.track_color.value() != gfx::kPlaceholderColor) {
    // TODO(crbug.com/40596569): Adjust track_color based on `state`.
    bg_color = extra_params.track_color.value();
  }
  DCHECK_NE(arrow_color, gfx::kPlaceholderColor);

  cc::PaintFlags flags;
  flags.setColor(bg_color);

  if (base::FeatureList::IsEnabled(kNewScrollbarArrowRadius) &&
      !extra_params.needs_rounded_corner) {
    canvas->drawIRect(gfx::RectToSkIRect(rect), flags);
  } else {
    // TODO(crbug.com/40285711): Also draw rounded corner for left and right
    // buttons when needed.
    SkScalar upper_left_radius = 0;
    SkScalar lower_left_radius = 0;
    SkScalar upper_right_radius = 0;
    SkScalar lower_right_radius = 0;
    float zoom = extra_params.zoom ? extra_params.zoom : 1.0;
    if (direction == kScrollbarUpArrow) {
      if (extra_params.right_to_left) {
        upper_left_radius = kScrollbarArrowRadius * zoom;
      } else {
        upper_right_radius = kScrollbarArrowRadius * zoom;
      }
    } else if (direction == kScrollbarDownArrow) {
      if (extra_params.right_to_left) {
        lower_left_radius = kScrollbarArrowRadius * zoom;
      } else {
        lower_right_radius = kScrollbarArrowRadius * zoom;
      }
    }
    DrawPartiallyRoundRect(canvas, rect, upper_left_radius, upper_right_radius,
                           lower_right_radius, lower_left_radius, flags);
  }

  PaintArrow(canvas, rect, direction, arrow_color);
}

void NativeThemeAura::PaintScrollbarTrack(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect,
    ColorScheme color_scheme,
    bool in_forced_colors) const {
  // Overlay Scrollbar should never paint a scrollbar track.
  DCHECK(!use_overlay_scrollbars_);
  cc::PaintFlags flags;
  const SkColor track_color =
      extra_params.track_color.has_value()
          ? extra_params.track_color.value()
          : GetControlColor(kScrollbarTrack, color_scheme, color_provider);
  flags.setColor(track_color);
  canvas->drawIRect(gfx::RectToSkIRect(rect), flags);
}

void NativeThemeAura::PaintScrollbarThumb(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    Part part,
    State state,
    const gfx::Rect& rect,
    const ScrollbarThumbExtraParams& extra_params,
    ColorScheme color_scheme) const {
  // Do not paint if state is disabled.
  if (state == kDisabled)
    return;

  TRACE_EVENT0("blink", "NativeThemeAura::PaintScrollbarThumb");

  gfx::Rect fill_rect(rect);
  cc::PaintFlags fill_flags;

  if (use_overlay_scrollbars_) {
    if (state == NativeTheme::kDisabled)
      return;

    const bool hovered = state != kNormal;

    DCHECK(color_provider);
    fill_flags.setColor(extra_params.thumb_color.value_or(
        color_provider->GetColor(hovered ? kColorOverlayScrollbarFillHovered
                                         : kColorOverlayScrollbarFill)));
    const SkColor stroke_color =
        color_provider->GetColor(hovered ? kColorOverlayScrollbarStrokeHovered
                                         : kColorOverlayScrollbarStroke);

    // In overlay mode, draw a stroke (border).
    constexpr int kStrokeWidth = kOverlayScrollbarStrokeWidth;
    cc::PaintFlags stroke_flags;
    stroke_flags.setColor(stroke_color);
    stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
    stroke_flags.setStrokeWidth(kStrokeWidth);

    gfx::RectF stroke_rect(fill_rect);
    gfx::InsetsF stroke_insets(kStrokeWidth / 2.f);
    // The edge to which the scrollbar is attached shouldn't have a border.
    gfx::Insets edge_adjust_insets;
    if (part == NativeTheme::kScrollbarHorizontalThumb)
      edge_adjust_insets.set_bottom(-kStrokeWidth);
    else
      edge_adjust_insets.set_right(-kStrokeWidth);
    stroke_rect.Inset(stroke_insets + gfx::InsetsF(edge_adjust_insets));
    canvas->drawRect(gfx::RectFToSkRect(stroke_rect), stroke_flags);

    // Inset the all the edges edges so we fill-in the stroke below.
    // For left vertical scrollbar, we will horizontally flip the canvas in
    // ScrollbarThemeOverlay::paintThumb.
    gfx::Insets fill_insets(kStrokeWidth);
    fill_rect.Inset(fill_insets + edge_adjust_insets);
  } else {
    fill_rect.Inset(GetScrollbarSolidColorThumbInsets(part));
    fill_flags.setColor(
        GetScrollbarThumbColor(*color_provider, state, extra_params));
  }

  canvas->drawIRect(gfx::RectToSkIRect(fill_rect), fill_flags);
}

gfx::Insets NativeThemeAura::GetScrollbarSolidColorThumbInsets(
    Part part) const {
  if (use_overlay_scrollbars_) {
    return gfx::Insets();
  }
  // If there are no scroll buttons then provide some inset so that the thumb
  // doesn't touch the top of the track.
  static constexpr int kThumbInset = 2;
  const int extra_inset = scrollbar_button_length() == 0 ? kThumbInset : 0;
  if (part == NativeTheme::kScrollbarVerticalThumb) {
    return gfx::Insets::VH(extra_inset, kThumbInset);
  }
  CHECK_EQ(part, NativeTheme::kScrollbarHorizontalThumb);
  return gfx::Insets::VH(kThumbInset, extra_inset);
}

SkColor4f NativeThemeAura::GetScrollbarThumbColor(
    const ui::ColorProvider& color_provider,
    State state,
    const ScrollbarThumbExtraParams& extra_params) const {
  // Only non-overlay aura scrollbars use solid color thumb.
  CHECK(!use_overlay_scrollbars_);
  // TODO(crbug.com/40596569): Adjust extra param `thumb_color` based on
  // `state`.
  if (extra_params.thumb_color.has_value()) {
    return SkColor4f::FromColor(extra_params.thumb_color.value());
  }
  ColorId color_id = kColorWebNativeControlScrollbarThumb;
  if (state == NativeTheme::kHovered) {
    color_id = kColorWebNativeControlScrollbarThumbHovered;
  } else if (state == NativeTheme::kPressed) {
    color_id = kColorWebNativeControlScrollbarThumbPressed;
  }
  return SkColor4f::FromColor(color_provider.GetColor(color_id));
}

void NativeThemeAura::PaintScrollbarCorner(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const ScrollbarTrackExtraParams& extra_params,
    ColorScheme color_scheme) const {
  // Overlay Scrollbar should never paint a scrollbar corner.
  DCHECK(!use_overlay_scrollbars_);
  const SkColor default_corner_color = GetControlColor(
      kScrollbarCornerControlColorId, color_scheme, color_provider);

  cc::PaintFlags flags;
  flags.setColor(extra_params.track_color.value_or(default_corner_color));
  canvas->drawIRect(RectToSkIRect(rect), flags);
}

gfx::Size NativeThemeAura::GetPartSize(Part part,
                                       State state,
                                       const ExtraParams& extra) const {
  if (use_overlay_scrollbars_) {
    constexpr int minimum_length =
        kOverlayScrollbarMinimumLength + 2 * kOverlayScrollbarStrokeWidth;

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

  return NativeThemeBase::GetPartSize(part, state, extra);
}

void NativeThemeAura::DrawPartiallyRoundRect(cc::PaintCanvas* canvas,
                                             const gfx::Rect& rect,
                                             const SkScalar upper_left_radius,
                                             const SkScalar upper_right_radius,
                                             const SkScalar lower_right_radius,
                                             const SkScalar lower_left_radius,
                                             const cc::PaintFlags& flags) {
  gfx::RRectF rounded_rect(
      gfx::RectF(rect), upper_left_radius, upper_left_radius,
      upper_right_radius, upper_right_radius, lower_right_radius,
      lower_right_radius, lower_left_radius, lower_left_radius);

  canvas->drawRRect(static_cast<SkRRect>(rounded_rect), flags);
}

bool NativeThemeAura::SupportsNinePatch(Part part) const {
  if (!use_overlay_scrollbars_) {
    return false;
  }

  return part == kScrollbarHorizontalThumb || part == kScrollbarVerticalThumb;
}

gfx::Size NativeThemeAura::GetNinePatchCanvasSize(Part part) const {
  DCHECK(SupportsNinePatch(part));

  return gfx::Size(
      kOverlayScrollbarBorderPatchWidth * 2 + kOverlayScrollbarCenterPatchSize,
      kOverlayScrollbarBorderPatchWidth * 2 + kOverlayScrollbarCenterPatchSize);
}

gfx::Rect NativeThemeAura::GetNinePatchAperture(Part part) const {
  DCHECK(SupportsNinePatch(part));

  return gfx::Rect(
      kOverlayScrollbarBorderPatchWidth, kOverlayScrollbarBorderPatchWidth,
      kOverlayScrollbarCenterPatchSize, kOverlayScrollbarCenterPatchSize);
}

}  // namespace ui
