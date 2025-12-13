// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_mobile.h"

#include "base/notreached.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/native_theme/native_theme_base.h"

namespace ui {

gfx::Size NativeThemeMobile::GetPartSize(
    Part part,
    State state,
    const ExtraParams& extra_params) const {
  // Radio buttons and checkboxes are slightly bigger than the defaults in
  // `NativeThemeBase`, to make touch easier on small form factor devices.
  return (part == kCheckbox || part == kRadio)
             ? gfx::Size(16, 16)
             : NativeThemeBase::GetPartSize(part, state, extra_params);
}

SkColor NativeThemeMobile::GetControlColor(
    ControlColorId color_id,
    bool dark_mode,
    PreferredContrast contrast,
    const ColorProvider* color_provider) const {
  // TODO(pkasting): Ensure the relevant bits of //ui/color/ are built on
  // Android, then have `WebThemeEngineAndroid::Paint()` pass a web-only
  // provider that provides the colors below, eliminating the need for this
  // override.
  const auto maybe_add_alpha = [&](ControlColorId id) {
    const SkColor color =
        GetControlColor(id, dark_mode, contrast, color_provider);
    return (dark_mode || contrast == PreferredContrast::kMore)
               ? color
               : SkColorSetA(color, 0x80);
  };
  switch (color_id) {
    case kBorder:
      return dark_mode ? SkColorSetRGB(0x85, 0x85, 0x85)
                       : SkColorSetRGB(0x76, 0x76, 0x76);
    case kDisabledBorder:
      return dark_mode ? SkColorSetRGB(0x62, 0x62, 0x62)
                       : SkColorSetARGB(0x4D, 0x76, 0x76, 0x76);
    case kHoveredBorder:
      return dark_mode ? SkColorSetRGB(0xAC, 0xAC, 0xAC)
                       : SkColorSetRGB(0x4F, 0x4F, 0x4F);
    case kPressedBorder:
      return dark_mode ? SkColorSetRGB(0x6E, 0x6E, 0x6E)
                       : SkColorSetRGB(0x8D, 0x8D, 0x8D);
    case kAccent:
      return dark_mode ? SkColorSetRGB(0x99, 0xC8, 0xFF)
                       : SkColorSetRGB(0x00, 0x75, 0xFF);
    case kDisabledAccent:
      return dark_mode ? SkColorSetRGB(0x75, 0x75, 0x75)
                       : SkColorSetARGB(0x4D, 0x76, 0x76, 0x76);
    case kHoveredAccent:
      return dark_mode ? SkColorSetRGB(0xD1, 0xE6, 0xFF)
                       : SkColorSetRGB(0x00, 0x5C, 0xC8);
    case kPressedAccent:
      return dark_mode ? SkColorSetRGB(0x61, 0xA9, 0xFF)
                       : SkColorSetRGB(0x37, 0x93, 0xFF);
    case kCheckboxBackground:
      return dark_mode ? SkColorSetRGB(0x3B, 0x3B, 0x3B) : SK_ColorWHITE;
    case kDisabledCheckboxBackground:
      return dark_mode ? SkColorSetRGB(0x3B, 0x3B, 0x3B)
                       : SkColorSetA(SK_ColorWHITE, 0x99);
    case kFill:
      return dark_mode ? SkColorSetRGB(0x3B, 0x3B, 0x3B)
                       : SkColorSetRGB(0xEF, 0xEF, 0xEF);
    case kDisabledFill:
      return dark_mode ? SkColorSetRGB(0x36, 0x36, 0x36)
                       : SkColorSetARGB(0x4D, 0xEF, 0xEF, 0xEF);
    case kHoveredFill:
      return dark_mode ? SkColorSetRGB(0x3B, 0x3B, 0x3B)
                       : SkColorSetRGB(0xE5, 0xE5, 0xE5);
    case kPressedFill:
      return dark_mode ? SkColorSetRGB(0x3B, 0x3B, 0x3B)
                       : SkColorSetRGB(0xF5, 0xF5, 0xF5);
    case kLightenLayer:
      return dark_mode ? SkColorSetRGB(0x3B, 0x3B, 0x3B)
                       : SkColorSetARGB(0x33, 0xA9, 0xA9, 0xA9);
    case kProgressValue:
      return dark_mode ? SkColorSetRGB(0x63, 0xAD, 0xE5)
                       : SkColorSetRGB(0x00, 0x75, 0xFF);
    case kSlider:
      return dark_mode ? SkColorSetRGB(0x99, 0xC8, 0xFF)
                       : SkColorSetRGB(0x00, 0x75, 0xFF);
    case kDisabledSlider:
      return dark_mode ? SkColorSetRGB(0x75, 0x75, 0x75)
                       : SkColorSetRGB(0xCB, 0xCB, 0xCB);
    case kHoveredSlider:
      return dark_mode ? SkColorSetRGB(0xD1, 0xE6, 0xFF)
                       : SkColorSetRGB(0x00, 0x5C, 0xC8);
    case kPressedSlider:
      return dark_mode ? SkColorSetRGB(0x61, 0xA9, 0xFF)
                       : SkColorSetRGB(0x37, 0x93, 0xFF);
    case kSliderBorder:
      return maybe_add_alpha(kBorder);
    case kHoveredSliderBorder:
      return maybe_add_alpha(kHoveredBorder);
    case kPressedSliderBorder:
      return maybe_add_alpha(kPressedBorder);
    case kAutoCompleteBackground:
      return dark_mode ? SkColorSetARGB(0x66, 0x46, 0x5A, 0x7E)
                       : SkColorSetRGB(0xE8, 0xF0, 0xFE);
    case kScrollbarArrow:
    case kScrollbarArrowHovered:
    case kScrollbarArrowPressed:
      // Even though Android does not paint scrollbars, these are used for the
      // arrow buttons that comprise a web "inner spin button" control.
      return dark_mode ? SK_ColorWHITE : SK_ColorBLACK;
    case kScrollbarArrowDisabled:
      return dark_mode ? SkColorSetRGB(0x55, 0x55, 0x55)
                       : SkColorSetRGB(0xA3, 0xA3, 0xA3);
    case kScrollbarCornerControlColorId:
    case kScrollbarTrack:
    case kScrollbarThumb:
    case kScrollbarThumbPressed:
    case kScrollbarThumbHovered:
      // These colors are unused because Android does not paint scrollbars.
      NOTREACHED();
    case kButtonBorder:
      return dark_mode ? SkColorSetRGB(0x6B, 0x6B, 0x6B)
                       : SkColorSetRGB(0x76, 0x76, 0x76);
    case kButtonDisabledBorder:
      return dark_mode ? SkColorSetRGB(0x36, 0x36, 0x36)
                       : SkColorSetARGB(0x4D, 0x76, 0x76, 0x76);
    case kButtonHoveredBorder:
      return dark_mode ? SkColorSetRGB(0x7B, 0x7B, 0x7B)
                       : SkColorSetRGB(0x4F, 0x4F, 0x4F);
    case kButtonPressedBorder:
      return dark_mode ? SkColorSetRGB(0x61, 0x61, 0x61)
                       : SkColorSetRGB(0x8D, 0x8D, 0x8D);
    case kButtonFill:
    case kScrollbarArrowBackground:
      return dark_mode ? SkColorSetRGB(0x6B, 0x6B, 0x6B)
                       : SkColorSetRGB(0xEF, 0xEF, 0xEF);
    case kButtonDisabledFill:
    case kScrollbarArrowBackgroundDisabled:
      return dark_mode ? SkColorSetRGB(0x36, 0x36, 0x36)
                       : SkColorSetARGB(0x4D, 0xEF, 0xEF, 0xEF);
    case kButtonHoveredFill:
    case kScrollbarArrowBackgroundHovered:
      return dark_mode ? SkColorSetRGB(0x7B, 0x7B, 0x7B)
                       : SkColorSetRGB(0xE5, 0xE5, 0xE5);
    case kButtonPressedFill:
    case kScrollbarArrowBackgroundPressed:
      return dark_mode ? SkColorSetRGB(0x61, 0x61, 0x61)
                       : SkColorSetRGB(0xF5, 0xF5, 0xF5);
  }
  NOTREACHED();
}

void NativeThemeMobile::PaintArrowButton(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const gfx::Rect& rect,
    Part part,
    State state,
    bool forced_colors,
    bool dark_mode,
    PreferredContrast contrast,
    const ScrollbarArrowExtraParams& extra_params) const {
  // Paint the background.
  PaintLightenLayer(canvas, color_provider, gfx::RectToSkRect(rect), state, 0,
                    dark_mode, contrast);

  // Paint the button's outline and fill the middle.
  SkPathBuilder outline;
  if (part == kScrollbarUpArrow) {
    outline.moveTo(rect.x() + 0.5f, rect.y() + rect.height() + 0.5f);
    outline.rLineTo(0, -(rect.height() - 2));
    outline.rLineTo(2, -2);
    outline.rLineTo(rect.width() - 5, 0);
    outline.rLineTo(2, 2);
    outline.rLineTo(0, rect.height() - 2);
  } else if (part == kScrollbarDownArrow) {
    outline.moveTo(rect.x() + 0.5f, rect.y() - 0.5f);
    outline.rLineTo(0, rect.height() - 2);
    outline.rLineTo(2, 2);
    outline.rLineTo(rect.width() - 5, 0);
    outline.rLineTo(2, -2);
    outline.rLineTo(0, -(rect.height() - 2));
  } else if (part == kScrollbarRightArrow) {
    outline.moveTo(rect.x() - 0.5f, rect.y() + 0.5f);
    outline.rLineTo(rect.width() - 2, 0);
    outline.rLineTo(2, 2);
    outline.rLineTo(0, rect.height() - 5);
    outline.rLineTo(-2, 2);
    outline.rLineTo(-(rect.width() - 2), 0);
  } else {
    CHECK_EQ(kScrollbarLeftArrow, part);
    outline.moveTo(rect.x() + rect.width() + 0.5f, rect.y() + 0.5f);
    outline.rLineTo(-(rect.width() - 2), 0);
    outline.rLineTo(-2, 2);
    outline.rLineTo(0, rect.height() - 5);
    outline.rLineTo(2, 2);
    outline.rLineTo(rect.width() - 2, 0);
  }
  outline.close();
  const SkPath outline_path = outline.detach();

  const SkColor bg_color = GetScrollbarArrowBackgroundColor(
      extra_params, state, dark_mode, contrast, color_provider);
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(bg_color);
  canvas->drawPath(outline_path, flags);

  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setColor(GetControlColorForState(kButtonBorderColors, state, dark_mode,
                                         contrast, color_provider));
  canvas->drawPath(outline_path, flags);

  PaintArrow(
      canvas, rect, part, state,
      GetScrollbarArrowForegroundColor(bg_color, extra_params, state, dark_mode,
                                       contrast, color_provider));
}

NativeThemeMobile::NativeThemeMobile() = default;

NativeThemeMobile::~NativeThemeMobile() = default;

}  // namespace ui
