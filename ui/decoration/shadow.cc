// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/decoration/shadow.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace ui::decoration {

namespace {

// Duration for opacity animation.
constexpr base::TimeDelta kShadowAnimationDuration = base::Milliseconds(100);

constexpr Shadow::ElevationColors kDefaultMdShadowColors = {
    SkColorSetA(SK_ColorBLACK, 0x3d),
    SkColorSetA(SK_ColorBLACK, 0x1f),
};
#if BUILDFLAG(IS_CHROMEOS)
constexpr Shadow::ElevationColors kDefaultChromeOSSystemUIShadowColors = {
    SkColorSetA(SK_ColorBLACK, 0x3d),
    SkColorSetA(SK_ColorBLACK, 0x1a),
};
#endif

constexpr Shadow::ElevationColors GetDefaultElevationColors(
    Shadow::Style style) {
  switch (style) {
    case Shadow::Style::kMaterialDesign:
      return kDefaultMdShadowColors;
#if BUILDFLAG(IS_CHROMEOS)
    case Shadow::Style::kChromeOSSystemUI:
      return kDefaultChromeOSSystemUIShadowColors;
#endif
  }
}

}  // namespace

DEFINE_SAFE_CAST_TARGET(Shadow)

// static
gfx::ShadowValues Shadow::MakeShadowValues(
    int elevation,
    Style style,
    std::optional<ElevationColors> colors,
    bool is_pill_shaped) {
  const ElevationColors shadow_colors =
      colors.value_or(GetDefaultElevationColors(style));

  switch (style) {
    case Style::kMaterialDesign:
      return gfx::ShadowValue::MakeMdShadowValues(
          elevation, shadow_colors.key_color, shadow_colors.ambient_color,
          is_pill_shaped);
#if BUILDFLAG(IS_CHROMEOS)
    case Style::kChromeOSSystemUI:
      return gfx::ShadowValue::MakeChromeOSSystemUIShadowValues(
          elevation, shadow_colors.key_color, shadow_colors.ambient_color,
          is_pill_shaped);
#endif
  }
}

Shadow::Shadow(int elevation, Style style, ElevationToColorsMap color_map)
    : elevation_(elevation), style_(style), color_map_(std::move(color_map)) {
  DCHECK_GE(elevation_, 0);
}

Shadow::~Shadow() = default;

void Shadow::SetElevation(int elevation) {
  DCHECK_GE(elevation, 0);
  if (elevation_ == elevation) {
    return;
  }

  elevation_ = elevation;
  NotifyDecorationChanged(kShadowAnimationDuration);
}

void Shadow::SetStyle(Style style) {
  if (style_ == style) {
    return;
  }

  style_ = style;
  NotifyDecorationChanged();
}

void Shadow::SetColorMap(const ElevationToColorsMap& color_map) {
  color_map_ = color_map;
  NotifyDecorationChanged();
}

std::optional<DecorationSource::Details> Shadow::GetDetails(
    const gfx::Rect& content_bounds,
    const gfx::RoundedCornersF& rounded_corners) {
  CHECK(!content_bounds.IsEmpty());

  const int smaller_dimension =
      std::min(content_bounds.width(), content_bounds.height());
  const float max_radius = std::floor(smaller_dimension / 2.0f);

  // The ninebox assumption breaks down when the content is too small for the
  // desired elevation. The height/width of |blur_region| will be 4 * elevation
  // (see `Shadow::MakeShadowValues`), so cap elevation at the most we can
  // handle.
  const bool is_pill_shaped = (max_radius == rounded_corners.upper_left() ||
                               max_radius == rounded_corners.upper_right() ||
                               max_radius == rounded_corners.lower_right() ||
                               max_radius == rounded_corners.lower_left());
  const int max_safe_elevation =
      is_pill_shaped
          ? smaller_dimension / 4
          : (smaller_dimension - 2 * std::max({rounded_corners.upper_left(),
                                               rounded_corners.upper_right(),
                                               rounded_corners.lower_right(),
                                               rounded_corners.lower_left()})) /
                4;
  const int size_adjusted_elevation = std::min(max_safe_elevation, elevation_);
  CHECK_GE(size_adjusted_elevation, 0);

  // Do not generate or set nine-patch details if the shadow has never had a
  // positive elevation set. This keeps the decoration layer unconfigured and
  // avoids unnecessary image allocations for zero-elevation shadows.
  if (size_adjusted_elevation == 0 && !details_.has_value()) {
    return std::nullopt;
  }

  auto iter = color_map_.find(elevation_);
  const gfx::ShadowValues values = MakeShadowValues(
      size_adjusted_elevation, style_,
      iter != color_map_.end() ? std::make_optional(iter->second)
                               : std::nullopt,
      is_pill_shaped);
  const auto& details = ShadowDetails::Get(rounded_corners, values);

  details_ = details;

  // The content is opaque everywhere except where its own rounded corners are
  // drawn on top of it, so exclude those from the occluded region.
  gfx::Rect occlusion_rect(content_bounds.size());
  occlusion_rect.Inset(GetInsetsForRoundedCorners(rounded_corners));

  return Details{
      .appearance =
          {
              .nine_patch_image = details.nine_patch_image,
              .aperture_insets = details.aperture_insets,
              .margins = details.margins,
          },
      .occlusion_rect = occlusion_rect,
  };
}

}  // namespace ui::decoration
