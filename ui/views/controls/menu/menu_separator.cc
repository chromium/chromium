// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_separator.h"

#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/menu/menu_config.h"

#if defined(OS_WIN)
#include "ui/display/win/dpi.h"
#endif

namespace views {

void MenuSeparator::OnPaint(gfx::Canvas* canvas) {
  if (type_ == ui::SPACING_SEPARATOR)
    return;

  const MenuConfig& menu_config = MenuConfig::instance();
  int pos = 0;
  int separator_thickness = menu_config.separator_thickness;
  if (type_ == ui::DOUBLE_SEPARATOR)
    separator_thickness = menu_config.double_separator_thickness;
  switch (type_) {
    case ui::LOWER_SEPARATOR:
      pos = height() - separator_thickness;
      break;
    case ui::UPPER_SEPARATOR:
      break;
    default:
      pos = (height() - separator_thickness) / 2;
      break;
  }

  gfx::Rect paint_rect(0, pos, width(), separator_thickness);
  if (type_ == ui::PADDED_SEPARATOR)
    paint_rect.Inset(menu_config.padded_separator_left_margin, 0, 0, 0);
  else if (menu_config.use_outer_border)
    paint_rect.Inset(1, 0);

#if defined(OS_WIN)
  // Hack to get the separator to display correctly on Windows where we may
  // have fractional scales. We move the separator 1 pixel down to ensure that
  // it falls within the clipping rect which is scaled up.
  float device_scale = display::win::GetDPIScale();
  bool is_fractional_scale =
      (device_scale - static_cast<int>(device_scale) != 0);
  if (is_fractional_scale && paint_rect.y() == 0)
    paint_rect.set_y(1);
#endif

  ui::NativeTheme::ExtraParams params;
  params.menu_separator.paint_rect = &paint_rect;
  params.menu_separator.type = type_;
  GetNativeTheme()->Paint(canvas->sk_canvas(),
                          ui::NativeTheme::kMenuPopupSeparator,
                          ui::NativeTheme::kNormal, GetLocalBounds(), params);
}

gfx::Size MenuSeparator::CalculatePreferredSize() const {
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

BEGIN_METADATA(MenuSeparator)
METADATA_PARENT_CLASS(View)
END_METADATA()

}  // namespace views
