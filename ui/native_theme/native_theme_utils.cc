// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_utils.h"

#include "base/containers/fixed_flat_map.h"
#include "ui/color/color_id.h"
#include "ui/native_theme/native_theme_color_id.h"

namespace ui {

base::StringPiece NativeThemeColorIdName(NativeTheme::ColorId color_id) {
  static constexpr const auto color_id_names =
      base::MakeFixedFlatMap<NativeTheme::ColorId, const char*>({
#define OP(enum_name) {NativeTheme::ColorId::enum_name, #enum_name}
          NATIVE_THEME_COLOR_IDS
#undef OP
      });
  auto* it = color_id_names.find(color_id);
  DCHECK_NE(color_id_names.cend(), it);
  return it->second;
}

base::StringPiece NativeThemeColorSchemeName(
    NativeTheme::ColorScheme color_scheme) {
  switch (color_scheme) {
    case NativeTheme::ColorScheme::kDefault:
      return "kDefault";
    case NativeTheme::ColorScheme::kLight:
      return "kLight";
    case NativeTheme::ColorScheme::kDark:
      return "kDark";
    case NativeTheme::ColorScheme::kPlatformHighContrast:
      return "kPlatformHighContrast";
    default:
      NOTREACHED() << "Invalid NativeTheme::ColorScheme";
      return "<invalid>";
  }
}

// clang-format off
absl::optional<ColorId>
NativeThemeColorIdToColorId(NativeTheme::ColorId native_theme_color_id) {
  using NTCID = NativeTheme::ColorId;
  static constexpr const auto map =
      base::MakeFixedFlatMap<NativeTheme::ColorId, ColorId>({
        {NTCID::kColorId_DefaultIconColor, kColorIcon},
        {NTCID::kColorId_FocusedBorderColor, kColorFocusableBorderFocused},
        {NTCID::kColorId_FocusedMenuItemBackgroundColor,
          kColorMenuItemBackgroundSelected},
        {NTCID::kColorId_MenuBackgroundColor, kColorMenuBackground},
        {NTCID::kColorId_MenuIconColor, kColorMenuIcon},
        {NTCID::kColorId_MenuSeparatorColor, kColorMenuSeparator},
        {NTCID::kColorId_OverlayScrollbarThumbFill, kColorOverlayScrollbarFill},
        {NTCID::kColorId_OverlayScrollbarThumbHoveredFill,
          kColorOverlayScrollbarFillHovered},
        {NTCID::kColorId_OverlayScrollbarThumbHoveredStroke,
          kColorOverlayScrollbarStrokeHovered},
        {NTCID::kColorId_OverlayScrollbarThumbStroke,
          kColorOverlayScrollbarStroke},
        {NTCID::kColorId_ProminentButtonColor,
          kColorButtonBackgroundProminent},
        {NTCID::kColorId_TextOnProminentButtonColor,
          kColorButtonForegroundProminent},
        {NTCID::kColorId_ThrobberSpinningColor, kColorThrobber},
        {NTCID::kColorId_ThrobberWaitingColor, kColorThrobberPreconnect},
        {NTCID::kColorId_WindowBackground, kColorWindowBackground},
      });
  auto* color_it = map.find(native_theme_color_id);
  if (color_it != map.cend()) {
    return color_it->second;
  }
  return absl::nullopt;
}
// clang-format on

}  // namespace ui
