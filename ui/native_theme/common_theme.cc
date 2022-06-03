// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/common_theme.h"

#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_provider_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/native_theme_utils.h"
#include "ui/native_theme/overlay_scrollbar_constants_aura.h"

namespace ui {

namespace {

absl::optional<SkColor> GetHighContrastColor(
    NativeTheme::ColorId color_id,
    NativeTheme::ColorScheme color_scheme) {
  switch (color_id) {
    case NativeTheme::kColorId_MenuSeparatorColor:
      return color_scheme == NativeTheme::ColorScheme::kDark ? SK_ColorWHITE
                                                             : SK_ColorBLACK;
    case NativeTheme::kColorId_FocusedBorderColor:
    case NativeTheme::kColorId_ProminentButtonColor:
      return color_scheme == NativeTheme::ColorScheme::kDark
                 ? gfx::kGoogleBlue100
                 : gfx::kGoogleBlue900;
    default:
      return absl::nullopt;
  }
}

absl::optional<SkColor> GetDarkSchemeColor(NativeTheme::ColorId color_id,
                                           const NativeTheme* base_theme) {
  switch (color_id) {
    case NativeTheme::kColorId_DefaultIconColor:
      return gfx::kGoogleGrey500;
    case NativeTheme::kColorId_FocusedBorderColor:
      return gfx::kGoogleBlue400;
    case NativeTheme::kColorId_MenuSeparatorColor:
      return gfx::kGoogleGrey800;
    case NativeTheme::kColorId_ProminentButtonColor:
      return gfx::kGoogleBlue300;
    case NativeTheme::kColorId_WindowBackground:
      return color_utils::BlendTowardMaxContrast(gfx::kGoogleGrey900, 0x0A);
    default:
      return absl::nullopt;
  }
}

SkColor GetDefaultColor(NativeTheme::ColorId color_id,
                        const NativeTheme* base_theme,
                        NativeTheme::ColorScheme color_scheme) {
  switch (color_id) {
    // Border
    case NativeTheme::kColorId_FocusedBorderColor:
      return gfx::kGoogleBlue500;

    // Button
    case NativeTheme::kColorId_TextOnProminentButtonColor:
      return color_utils::GetColorWithMaxContrast(
          base_theme->GetUnprocessedSystemColor(
              NativeTheme::kColorId_ProminentButtonColor, color_scheme));
    case NativeTheme::kColorId_ProminentButtonColor:
      return gfx::kGoogleBlue600;

    // Icon
    case NativeTheme::kColorId_DefaultIconColor:
      return gfx::kGoogleGrey700;

    // Menu
    case NativeTheme::kColorId_MenuBackgroundColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
    case NativeTheme::kColorId_MenuSeparatorColor:
      return gfx::kGoogleGrey300;
    case NativeTheme::kColorId_FocusedMenuItemBackgroundColor: {
      const SkColor bg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      const SkColor fg = color_utils::GetColorWithMaxContrast(bg);
      return color_utils::AlphaBlend(fg, bg, gfx::kGoogleGreyAlpha200);
    }
    case NativeTheme::kColorId_MenuIconColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_DefaultIconColor, color_scheme);

    // Scrollbar
    case NativeTheme::kColorId_OverlayScrollbarThumbFill:
    case NativeTheme::kColorId_OverlayScrollbarThumbHoveredFill: {
      const SkColor bg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      const SkColor fill = color_utils::GetColorWithMaxContrast(bg);
      const bool hovered =
          color_id == NativeTheme::kColorId_OverlayScrollbarThumbHoveredFill;
      return SkColorSetA(
          fill, hovered ? gfx::kGoogleGreyAlpha800 : gfx::kGoogleGreyAlpha700);
    }
    case NativeTheme::kColorId_OverlayScrollbarThumbStroke:
    case NativeTheme::kColorId_OverlayScrollbarThumbHoveredStroke: {
      const SkColor bg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      const SkColor stroke = color_utils::GetEndpointColorWithMinContrast(bg);
      const bool hovered =
          color_id == NativeTheme::kColorId_OverlayScrollbarThumbHoveredStroke;
      return SkColorSetA(stroke, hovered ? gfx::kGoogleGreyAlpha500
                                         : gfx::kGoogleGreyAlpha400);
    }

    // Throbber
    case NativeTheme::kColorId_ThrobberWaitingColor: {
      const SkColor bg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      const SkColor fg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);
      return color_utils::AlphaBlend(fg, bg, gfx::kGoogleGreyAlpha400);
    }
    case NativeTheme::kColorId_ThrobberSpinningColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);

    // Window
    case NativeTheme::kColorId_WindowBackground:
      return SK_ColorWHITE;

    case NativeTheme::kColorId_NumColors:
      // Keeping the kColorId_NumColors case instead of using the default case
      // allows ColorId additions to trigger compile error for an incomplete
      // switch enumeration.
      NOTREACHED();
      return gfx::kPlaceholderColor;
  }
}

}  // namespace

SkColor GetAlertSeverityColor(ColorId color_id, bool dark) {
  constexpr auto kColorIdMap =
      base::MakeFixedFlatMap<ColorId, std::array<SkColor, 2>>({
          {kColorAlertHighSeverity, {{gfx::kGoogleRed600, gfx::kGoogleRed300}}},
          {kColorAlertLowSeverity,
           {{gfx::kGoogleGreen700, gfx::kGoogleGreen300}}},
          {kColorAlertMediumSeverity,
           {{gfx::kGoogleYellow700, gfx::kGoogleYellow300}}},
      });
  return kColorIdMap.at(color_id)[dark];
}

SkColor GetAuraColor(NativeTheme::ColorId color_id,
                     const NativeTheme* base_theme,
                     NativeTheme::ColorScheme color_scheme) {
  if (color_scheme == NativeTheme::ColorScheme::kDefault)
    color_scheme = base_theme->GetDefaultSystemColorScheme();

  // High contrast overrides the normal colors for certain ColorIds to be much
  // darker or lighter.
  if (base_theme->UserHasContrastPreference()) {
    absl::optional<SkColor> color =
        GetHighContrastColor(color_id, color_scheme);
    if (color.has_value()) {
      DVLOG(2) << "GetHighContrastColor: "
               << "NativeTheme::ColorId: " << NativeThemeColorIdName(color_id)
               << " Color: " << SkColorName(color.value());
      return color.value();
    }
  }

  if (color_scheme == NativeTheme::ColorScheme::kDark) {
    absl::optional<SkColor> color = GetDarkSchemeColor(color_id, base_theme);
    if (color.has_value()) {
      DVLOG(2) << "GetDarkSchemeColor: "
               << "NativeTheme::ColorId: " << NativeThemeColorIdName(color_id)
               << " Color: " << SkColorName(color.value());
      return color.value();
    }
  }

  SkColor color = GetDefaultColor(color_id, base_theme, color_scheme);
  DVLOG(2) << "GetDefaultColor: "
           << "NativeTheme::ColorId: " << NativeThemeColorIdName(color_id)
           << " Color: " << SkColorName(color);
  return color;
}

void CommonThemePaintMenuItemBackground(
    const NativeTheme* theme,
    cc::PaintCanvas* canvas,
    NativeTheme::State state,
    const gfx::Rect& rect,
    const NativeTheme::MenuItemExtraParams& menu_item,
    NativeTheme::ColorScheme color_scheme) {
  cc::PaintFlags flags;
  switch (state) {
    case NativeTheme::kNormal:
    case NativeTheme::kDisabled:
      flags.setColor(theme->GetSystemColor(
          NativeTheme::kColorId_MenuBackgroundColor, color_scheme));
      break;
    case NativeTheme::kHovered:
      flags.setColor(theme->GetSystemColor(
          NativeTheme::kColorId_FocusedMenuItemBackgroundColor, color_scheme));
      break;
    default:
      NOTREACHED() << "Invalid state " << state;
      break;
  }
  if (menu_item.corner_radius > 0) {
    const SkScalar radius = SkIntToScalar(menu_item.corner_radius);
    canvas->drawRoundRect(gfx::RectToSkRect(rect), radius, radius, flags);
    return;
  }
  canvas->drawRect(gfx::RectToSkRect(rect), flags);
}

}  // namespace ui
