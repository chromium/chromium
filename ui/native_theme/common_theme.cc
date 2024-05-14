// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/common_theme.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"

namespace ui {

void CommonThemePaintMenuItemBackground(
    const NativeTheme* theme,
    const ColorProvider* color_provider,
    cc::PaintCanvas* canvas,
    NativeTheme::State state,
    const gfx::Rect& rect,
    const NativeTheme::MenuItemExtraParams& menu_item) {
  DCHECK(color_provider);
  cc::PaintFlags flags;
  switch (state) {
    case NativeTheme::kNormal:
    case NativeTheme::kDisabled: {
      ui::ColorId id = kColorMenuBackground;
#if BUILDFLAG(IS_CHROMEOS_ASH)
      id = kColorAshSystemUIMenuBackground;
#endif
      flags.setColor(color_provider->GetColor(id));
      break;
    }
    case NativeTheme::kHovered: {
      ui::ColorId id = kColorMenuItemBackgroundSelected;
#if BUILDFLAG(IS_CHROMEOS_ASH)
      id = kColorAshSystemUIMenuItemBackgroundSelected;
#endif
      flags.setColor(color_provider->GetColor(id));
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid state " << state;
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
