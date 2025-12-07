// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_fluent.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/cstring_view.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/native_theme/features/native_theme_features.h"

namespace ui {

namespace {

// A sufficiently large value ensures the most round curve for the corners of
// the scrollbar thumb and overlay buttons.
constexpr int kScrollbarPartsRadius = 999;

// The outline width used to paint track and buttons in High Contrast mode.
constexpr float kScrollbarTrackOutlineWidth = 1.0f;

}  // namespace

int NativeThemeFluent::GetPaintedScrollbarTrackInset() const {
  return 1;
}

bool NativeThemeFluent::GetArrowIconsAvailable() const {
  return !!GetArrowIconTypeface();
}

void NativeThemeFluent::SetArrowIconsAvailableForTesting(bool available) {
  typeface_ = available ? skia::DefaultTypeface() : nullptr;
}

NativeThemeFluent::NativeThemeFluent() {
  set_use_overlay_scrollbar(IsFluentOverlayScrollbarEnabled());
}

NativeThemeFluent::~NativeThemeFluent() = default;

gfx::Size NativeThemeFluent::GetVerticalScrollbarButtonSize() const {
  return gfx::Size(kScrollbarThickness, kScrollbarButtonSideLength);
}

gfx::Size NativeThemeFluent::GetVerticalScrollbarThumbSize() const {
  return gfx::Size(9, 17);
}

gfx::RectF NativeThemeFluent::GetArrowRect(const gfx::Rect& rect,
                                           Part part,
                                           State state) const {
  const bool arrow_icons_available = GetArrowIconsAvailable();
  int unscaled_arrow_side = 9;
  if (state == kPressed) {
    unscaled_arrow_side = arrow_icons_available ? 8 : 7;
  }

  // Note: Using initializer_list form forces returning by copy, not ref.
  const auto [min_rect_side, max_rect_side] =
      std::minmax({rect.width(), rect.height()});
  const float scale_factor =
      max_rect_side / static_cast<float>(kScrollbarButtonSideLength);
  int arrow_side = base::ClampCeil(unscaled_arrow_side * scale_factor);

  gfx::RectF arrow_rect(rect);
  if (arrow_icons_available) {
    arrow_rect.ClampToCenteredSize(
        static_cast<gfx::SizeF>(gfx::Size(arrow_side, arrow_side)));
  } else {
    // Add 1px to the side length if the difference between smaller button rect
    // and arrow side length is odd to keep the arrow rect in the center as well
    // as use int coordinates. This avoids anti-aliasing.
    arrow_side += (min_rect_side - arrow_side) % 2;
    arrow_rect.ClampToCenteredSize(
        static_cast<gfx::SizeF>(gfx::Size(arrow_side, arrow_side)));
    arrow_rect.set_origin(
        {std::floor(arrow_rect.x()), std::floor(arrow_rect.y())});
  }

  // The end result is a centered arrow rect within the button rect with the
  // applied offset.
  const int unscaled_offset =
      (part == kScrollbarUpArrow || part == kScrollbarLeftArrow) ? -1 : 1;
  gfx::Vector2dF offset(std::round(unscaled_offset * scale_factor), 0);
  if (part == kScrollbarUpArrow || part == kScrollbarDownArrow) {
    offset.Transpose();
  }
  return arrow_rect + offset;
}

std::optional<ColorId> NativeThemeFluent::GetScrollbarThumbColorId(
    State state,
    const ScrollbarThumbExtraParams& extra_params) const {
  return (extra_params.is_thumb_minimal_mode && state != kHovered &&
          state != kPressed)
             ? std::make_optional(
                   kColorWebNativeControlScrollbarThumbOverlayMinimalMode)
             : std::nullopt;
}

float NativeThemeFluent::GetScrollbarPartContrastRatioForState(
    State state) const {
  return 1.8f;
}

void NativeThemeFluent::PaintArrowButton(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const gfx::Rect& rect,
    Part part,
    State state,
    bool forced_colors,
    bool dark_mode,
    PreferredContrast contrast,
    const ScrollbarArrowExtraParams& extra_params) const {
  const auto paint_button = [this, canvas, part](const gfx::RectF& paint_rect,
                                                 cc::PaintFlags paint_flags) {
    if (!use_overlay_scrollbar()) {
      canvas->drawRect(gfx::RectFToSkRect(paint_rect), paint_flags);
      return;
    }

    SkScalar ul = 0, ll = 0, ur = 0, lr = 0;
    if (part == kScrollbarUpArrow) {
      ul = kScrollbarPartsRadius;
      ur = kScrollbarPartsRadius;
    } else if (part == kScrollbarDownArrow) {
      ll = kScrollbarPartsRadius;
      lr = kScrollbarPartsRadius;
    } else if (part == kScrollbarLeftArrow) {
      ul = kScrollbarPartsRadius;
      ll = kScrollbarPartsRadius;
    } else if (part == kScrollbarRightArrow) {
      ur = kScrollbarPartsRadius;
      lr = kScrollbarPartsRadius;
    }
    const gfx::RRectF rrect(paint_rect, ul, ul, ur, ur, lr, lr, ll, ll);
    paint_flags.setAntiAlias(true);
    canvas->drawRRect(static_cast<SkRRect>(rrect), paint_flags);
  };

  gfx::RectF button_fill_rect(rect);
  // Windows native Fluent scrollbars draw a border around the button in forced
  // colors mode regardless of the contrast of the colors; and the border seems
  // clearly beneficial in high contrast, especially on platforms that don't
  // natively do forced colors.
  if (forced_colors || contrast == PreferredContrast::kMore) {
    gfx::OutsetsF edge_outsets;
    if (part == kScrollbarUpArrow) {
      edge_outsets.set_bottom(kScrollbarTrackOutlineWidth);
    } else if (part == kScrollbarDownArrow) {
      edge_outsets.set_top(kScrollbarTrackOutlineWidth);
    } else if (part == kScrollbarLeftArrow) {
      edge_outsets.set_right(kScrollbarTrackOutlineWidth);
    } else if (part == kScrollbarRightArrow) {
      edge_outsets.set_left(kScrollbarTrackOutlineWidth);
    }
    button_fill_rect.Outset(edge_outsets);

    gfx::RectF outline_rect = button_fill_rect;
    outline_rect.Inset(kScrollbarTrackOutlineWidth / 2.0f);

    cc::PaintFlags outline_flags;
    outline_flags.setColor(
        GetScrollbarThumbColor(color_provider, state,
                               {.is_hovering = extra_params.is_hovering,
                                .thumb_color = extra_params.thumb_color,
                                .track_color = extra_params.track_color}));
    outline_flags.setStyle(cc::PaintFlags::kStroke_Style);
    outline_flags.setStrokeWidth(kScrollbarTrackOutlineWidth);
    paint_button(outline_rect, outline_flags);

    // Adjust the fill rect to not overlap with the outline stroke rect.
    button_fill_rect.Inset(kScrollbarTrackOutlineWidth);
  }

  // Paint button background.
  const SkColor bg_color = GetScrollbarArrowBackgroundColor(
      extra_params, state, dark_mode, contrast, color_provider);
  cc::PaintFlags bg_flags;
  bg_flags.setColor(bg_color);
  paint_button(button_fill_rect, bg_flags);

  // Paint arrow.
  const SkColor fg_color = GetScrollbarArrowForegroundColor(
      bg_color, extra_params, state, dark_mode, contrast, color_provider);
  if (const auto typeface = GetArrowIconTypeface()) {
    static constexpr auto kCodePointMap =
        base::MakeFixedFlatMap<Part, base::cstring_view>(
            {{kScrollbarDownArrow, "\uEDDC"},
             {kScrollbarLeftArrow, "\uEDD9"},
             {kScrollbarRightArrow, "\uEDDA"},
             {kScrollbarUpArrow, "\uEDDB"}});

    const gfx::RectF bounding_rect = GetArrowRect(rect, part, state);
    // The bounding rect for an arrow is square, so we can use the width
    // regardless of the arrow direction.
    SkFont font(typeface, bounding_rect.width());
    font.setEdging(SkFont::Edging::kAntiAlias);
    font.setSubpixel(true);

    cc::PaintFlags fg_flags;
    fg_flags.setAntiAlias(true);
    fg_flags.setColor(fg_color);
    canvas->drawTextBlob(
        SkTextBlob::MakeFromString(kCodePointMap.at(part).c_str(), font),
        bounding_rect.x(), bounding_rect.bottom(), fg_flags);
  } else {
    // Paint regular triangular arrows if arrow icons are not available.
    PaintArrow(canvas, rect, part, state, fg_color);
  }
}

void NativeThemeFluent::PaintScrollbarThumb(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    Part part,
    State state,
    const gfx::Rect& rect,
    const ScrollbarThumbExtraParams& extra_params) const {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(GetScrollbarThumbColor(color_provider, state, extra_params));
  const SkRect sk_rect = gfx::RectToSkRect(rect);
  if (extra_params.is_web_test) {
    // Web tests draw the thumb as a square to avoid trivial discrepancies due
    // to rounding/AA.
    canvas->drawRect(sk_rect, flags);
  } else {
    canvas->drawRRect(SkRRect::MakeRectXY(sk_rect, kScrollbarPartsRadius,
                                          kScrollbarPartsRadius),
                      flags);
  }
}

void NativeThemeFluent::PaintScrollbarTrack(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect,
    bool forced_colors,
    PreferredContrast contrast) const {
  gfx::Rect track_fill_rect = rect;
  // See comments in `PaintArrowButton()` re: the condition here.
  if (forced_colors || contrast == PreferredContrast::kMore) {
    gfx::Insets edge_insets;
    if (part == kScrollbarHorizontalTrack) {
      edge_insets.set_left_right(-kScrollbarTrackOutlineWidth,
                                 -kScrollbarTrackOutlineWidth);
    } else {
      edge_insets.set_top_bottom(-kScrollbarTrackOutlineWidth,
                                 -kScrollbarTrackOutlineWidth);
    }
    const gfx::InsetsF outline_insets(kScrollbarTrackOutlineWidth / 2.0f);

    gfx::RectF outline_rect(rect);
    outline_rect.Inset(outline_insets + gfx::InsetsF(edge_insets));

    cc::PaintFlags outline_flags;
    outline_flags.setColor(
        color_provider->GetColor(kColorWebNativeControlScrollbarThumb));
    outline_flags.setStyle(cc::PaintFlags::kStroke_Style);
    outline_flags.setStrokeWidth(kScrollbarTrackOutlineWidth);
    canvas->drawRect(gfx::RectFToSkRect(outline_rect), outline_flags);

    // Adjust fill rect to not overlap with the outline stroke rect.
    track_fill_rect.Inset(gfx::Insets(kScrollbarTrackOutlineWidth) +
                          edge_insets);
  }

  cc::PaintFlags flags;
  flags.setColor(extra_params.track_color.value_or(
      color_provider->GetColor(kColorWebNativeControlScrollbarTrack)));
  canvas->drawIRect(gfx::RectToSkIRect(track_fill_rect), flags);
}

void NativeThemeFluent::PaintScrollbarCorner(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const ScrollbarTrackExtraParams& extra_params) const {
  cc::PaintFlags flags;
  flags.setColor(extra_params.track_color.value_or(
      color_provider->GetColor(kColorWebNativeControlScrollbarCorner)));
  canvas->drawIRect(RectToSkIRect(rect), flags);
}

sk_sp<SkTypeface> NativeThemeFluent::GetArrowIconTypeface() const {
  if (!typeface_.has_value()) {
    const sk_sp<SkFontMgr> font_manager(skia::DefaultFontMgr());
    typeface_ = font_manager->matchFamilyStyle("Segoe Fluent Icons", {});
  }
  return typeface_.value();
}

}  // namespace ui
