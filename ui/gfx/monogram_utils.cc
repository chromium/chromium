// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/monogram_utils.h"

#include <string>
#include <string_view>
#include <vector>

#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"

namespace gfx {
namespace {

// Draws a circle of a given `size` and `offset` in the `canvas` and fills it
// with `background_color`.
void DrawCircleInCanvas(Canvas* canvas,
                        int size,
                        int offset,
                        SkColor background_color) {
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(background_color);
  int corner_radius = size / 2;
  canvas->DrawRoundRect(Rect(offset, offset, size, size), corner_radius, flags);
}

// Will paint `letter` in the center of specified `canvas` of given `size`.
void DrawFallbackIconLetter(Canvas* canvas,
                            std::u16string_view monogram,
                            const std::vector<std::string>& font_names,
                            SkColor monogram_color,
                            int size,
                            int offset) {
  if (monogram.empty()) {
    return;
  }

  const double kDefaultFontSizeRatio = 0.5;
  int font_size = static_cast<int>(size * kDefaultFontSizeRatio);
  if (font_size <= 0) {
    return;
  }

  Font::Weight font_weight = Font::Weight::NORMAL;

#if BUILDFLAG(IS_WIN)
  font_weight = Font::Weight::SEMIBOLD;
#endif

  // TODO(https://crbug.com/41395192): Adjust the text color according to the
  // background color.
  canvas->DrawStringRectWithFlags(
      monogram, FontList(font_names, Font::NORMAL, font_size, font_weight),
      monogram_color, Rect(offset, offset, size, size),
      Canvas::TEXT_ALIGN_CENTER);
}

}  // namespace

void DrawMonogramInCanvas(Canvas* canvas,
                          int canvas_size,
                          int circle_size,
                          std::u16string_view monogram,
                          const std::vector<std::string>& font_names,
                          SkColor monogram_color,
                          SkColor background_color) {
  canvas->DrawColor(SK_ColorTRANSPARENT, SkBlendMode::kSrc);

  int offset = (canvas_size - circle_size) / 2;
  DrawCircleInCanvas(canvas, circle_size, offset, background_color);
  DrawFallbackIconLetter(canvas, monogram, font_names, monogram_color,
                         circle_size, offset);
}

}  // namespace gfx
