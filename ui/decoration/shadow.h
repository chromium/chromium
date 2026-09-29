// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DECORATION_SHADOW_H_
#define UI_DECORATION_SHADOW_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "build/build_config.h"
#include "ui/decoration/decoration_source.h"
#include "ui/decoration/decoration_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/shadow_value.h"

namespace ui::decoration {

// DecorationSource that draws a drop shadow around the content it frames.
class Shadow : public DecorationSource {
 public:
  DECLARE_SAFE_CAST_TARGET()

  // The shadow style for different UI components.
  enum class Style {
    // The MD style is mainly used for view's shadow.
    kMaterialDesign,
#if BUILDFLAG(IS_CHROMEOS)
    // The system style is mainly used for Chrome OS UI components.
    kChromeOSSystemUI,
#endif
  };

  // Key and ambient shadow colors for an elevation.
  struct ElevationColors {
    SkColor key_color = gfx::kPlaceholderColor;
    SkColor ambient_color = gfx::kPlaceholderColor;

    bool operator==(const ElevationColors&) const = default;
  };

  // Mapping from elevation to key and ambient shadow colors.
  using ElevationToColorsMap = base::flat_map<int, ElevationColors>;

  // Makes ShadowValues for the given elevation and shadow style. If |colors| is
  // not provided, default colors are used.
  static gfx::ShadowValues MakeShadowValues(
      int elevation,
      Style style = Style::kMaterialDesign,
      std::optional<ElevationColors> colors = std::nullopt,
      bool is_pill_shaped = false);

  explicit Shadow(int elevation,
                  Style style = Style::kMaterialDesign,
                  ElevationToColorsMap color_map = {});

  Shadow(const Shadow&) = delete;
  Shadow& operator=(const Shadow&) = delete;

  ~Shadow() override;

  // Sets the shadow's appearance, animating opacity as necessary.
  void SetElevation(int elevation);
  int elevation() const { return elevation_; }

  // Set shadow style.
  void SetStyle(Style style);
  Style style() const { return style_; }

  // Set customized key and ambient shadows color map for certain elevations.
  void SetColorMap(const ElevationToColorsMap& color_map);
  const ElevationToColorsMap& color_map() const { return color_map_; }

  // DecorationSource:
  std::optional<Details> GetDetails(
      const gfx::Rect& content_bounds,
      const gfx::RoundedCornersF& rounded_corners) override;

  const ShadowDetails* details_for_testing() const {
    return details_ ? &details_.value() : nullptr;
  }

 private:
  // The goal elevation, set when the transition animation starts. The elevation
  // dictates the shadow's display characteristics and is proportional to the
  // size of the blur and its offset. This may not match reality if the window
  // isn't big enough to support it.
  int elevation_ = 0;

  // The style of shadow. Use MD style by default.
  Style style_ = Style::kMaterialDesign;

  // The customized key and ambient shadows color map for certain elevations.
  ElevationToColorsMap color_map_;

  // The details of the shadow image that's currently set on the decoration
  // layer. This will be nullopt until a positive elevation has been set.
  std::optional<ShadowDetails> details_;
};

}  // namespace ui::decoration

#endif  // UI_DECORATION_SHADOW_H_
