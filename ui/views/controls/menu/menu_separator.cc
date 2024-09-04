// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_separator.h"

#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/dpi.h"
#endif

namespace views {

MenuSeparator::MenuSeparator(ui::MenuSeparatorType type) : type_(type) {
  GetViewAccessibility().SetRole(ax::mojom::Role::kSplitter);
}

void MenuSeparator::OnPaint(gfx::Canvas* canvas) {
  const MenuConfig& menu_config = MenuConfig::instance();
  if (type_ == ui::SPACING_SEPARATOR ||
      width() < menu_config.separator_horizontal_border_padding * 2) {
    return;
  }

  int y = 0;
  int separator_thickness = menu_config.separator_thickness;
  if (type_ == ui::DOUBLE_SEPARATOR)
    separator_thickness = menu_config.double_separator_thickness;
  switch (type_) {
    case ui::LOWER_SEPARATOR:
      y = height() - separator_thickness;
      break;
    case ui::UPPER_SEPARATOR:
      break;
    default:
      y = (height() - separator_thickness) / 2;
      break;
  }

  gfx::Rect paint_rect(
      menu_config.separator_horizontal_border_padding, y,
      width() - menu_config.separator_horizontal_border_padding * 2,
      separator_thickness);
  if (type_ == ui::PADDED_SEPARATOR) {
    paint_rect.Inset(
        gfx::Insets::TLBR(0, menu_config.padded_separator_start_padding, 0, 0));
  }

  if (menu_config.use_outer_border && type_ != ui::PADDED_SEPARATOR) {
    paint_rect.Inset(gfx::Insets::VH(0, 1));
  }

  ui::NativeTheme::MenuSeparatorExtraParams menu_separator;
  menu_separator.paint_rect = &paint_rect;
  menu_separator.color_id =
      MenuController::GetActiveInstance()->GetSeparatorColorId();
  menu_separator.type = type_;
  GetNativeTheme()->Paint(canvas->sk_canvas(), GetColorProvider(),
                          ui::NativeTheme::kMenuPopupSeparator,
                          ui::NativeTheme::kNormal, GetLocalBounds(),
                          ui::NativeTheme::ExtraParams(menu_separator));
}

gfx::Size MenuSeparator::CalculatePreferredSize(
    const SizeBounds& /*available_size*/) const {
  const MenuConfig& menu_config = MenuConfig::instance();
  int height = menu_config.separator_height;
  switch (type_) {
    case ui::SPACING_SEPARATOR:
      height = menu_config.separator_spacing_height;
      break;
    case ui::LOWER_SEPARATOR:
      height = menu_config.separator_lower_height;
      break;
    case ui::UPPER_SEPARATOR:
      height = menu_config.separator_upper_height;
      break;
    case ui::DOUBLE_SEPARATOR:
      height = menu_config.double_separator_height;
      break;
    case ui::PADDED_SEPARATOR:
      height = menu_config.separator_thickness;
      break;
    default:
      height = menu_config.separator_height;
      break;
  }
  return gfx::Size(10,  // Just in case we're the only item in a menu.
                   height);
}

ui::MenuSeparatorType MenuSeparator::GetType() const {
  return type_;
}

void MenuSeparator::SetType(ui::MenuSeparatorType type) {
  if (type_ == type)
    return;

  type_ = type;
  OnPropertyChanged(&type_, kPropertyEffectsPreferredSizeChanged);
}

BEGIN_METADATA(MenuSeparator)
ADD_PROPERTY_METADATA(ui::MenuSeparatorType, Type)
END_METADATA

}  // namespace views
