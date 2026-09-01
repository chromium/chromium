// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_EXTRA_SHADOW_H_
#define UI_COMPOSITOR_EXTRA_SHADOW_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_nine_patch.h"
#include "ui/compositor/layer_owner.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/shadow_util.h"

namespace ui {

// Simple class that draws a drop shadow around content at given bounds.
class Shadow : public ui::ImplicitAnimationObserver, public ui::LayerOwner {
 public:
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

  Shadow();

  Shadow(const Shadow&) = delete;
  Shadow& operator=(const Shadow&) = delete;

  ~Shadow() override;

  // Initialize for the the given shadow |elevation|. This is passed to
  // gfx::ShadowValue::MakeMdShadowValues() and controls the y-offset and blur
  // for the shadow style.
  void Init(int elevation);

  // Exposed to allow setting animation parameters for bounds and opacity
  // animations.
  ui::LayerNinePatch* shadow_layer() {
    ui::Layer* layer = shadow_layer_owner_.layer();
    return layer ? layer->AsNinePatch() : nullptr;
  }

  ui::LayerNinePatch* fading_layer() {
    ui::Layer* layer = fading_layer_owner_.layer();
    return layer ? layer->AsNinePatch() : nullptr;
  }

  // Moves and resizes the shadow layer to frame |content_bounds|.
  // This should be used to adjust the shadow's size and position (rather than
  // applying transformations to the `layer()` of this Shadow).
  void SetContentBounds(const gfx::Rect& content_bounds);
  const gfx::Rect& content_bounds() const { return content_bounds_; }

  // Sets the shadow's appearance, animating opacity as necessary.
  void SetElevation(int elevation);
  int elevation() const { return elevation_; }

  // Sets the radii for the rounded corners to take into account when
  // adjusting the shadow layer to frame |content_bounds|.
  void SetRoundedCorners(const gfx::RoundedCornersF& radii);
  const gfx::RoundedCornersF& rounded_corners() const {
    return rounded_corners_;
  }

  // Set shadow style.
  void SetStyle(Style style);
  Style style() const { return style_; }

  // Set customized key and ambient shadows color map for certain elevations.
  void SetColorMap(const ElevationToColorsMap& color_map);
  const ElevationToColorsMap& color_map() const { return color_map_; }

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override;

  const gfx::ShadowDetails* details_for_testing() const {
    return details_ ? &details_.value() : nullptr;
  }

 private:
  // A shadow layer owner that correctly updates the nine patch layer details
  // when it gets recreated.
  class ShadowLayerOwner : public ui::LayerOwner {
   public:
    explicit ShadowLayerOwner(Shadow* owner,
                              std::unique_ptr<Layer> layer = nullptr);

    ShadowLayerOwner(const ShadowLayerOwner&) = delete;
    ShadowLayerOwner& operator=(const ShadowLayerOwner&) = delete;

    ~ShadowLayerOwner() override;

    // ui::LayerOwner:
    std::unique_ptr<Layer> RecreateLayer() override;

   private:
    const raw_ptr<Shadow> owner_shadow_;
  };

  // Updates the shadow layer and its image to reflect |desired_elevation_|.
  void RecreateShadowLayer();

  // Updates the shadow appearance based on the inteior inset, the current
  // |content_bounds_|, shadow style, and colors.
  void UpdateShadowAppearance();

  // The goal elevation, set when the transition animation starts. The elevation
  // dictates the shadow's display characteristics and is proportional to the
  // size of the blur and its offset. This may not match reality if the window
  // isn't big enough to support it.
  int elevation_ = 0;

  // Rounded corners are drawn on top of the window's content layer,
  // we need to exclude them from the occlusion area.
  gfx::RoundedCornersF rounded_corners_{2};

  // The details of the shadow image that's currently set on |shadow_layer()|.
  // This will be nullopt until a positive elevation has been set.
  std::optional<gfx::ShadowDetails> details_;

  // The style of shadow. Use MD style by default.
  Style style_ = Style::kMaterialDesign;

  // The customized key and ambient shadows color map for certain elevations.
  ElevationToColorsMap color_map_;

  // The owner of the actual shadow layer corresponding to a cc::NinePatchLayer.
  ShadowLayerOwner shadow_layer_owner_;

  // When the elevation changes, the old shadow cross-fades with the new one.
  // When non-null, this owns an old |shadow_layer()| that's being animated out.
  ui::LayerOwner fading_layer_owner_;

  // Bounds of the content that the shadow encloses.
  gfx::Rect content_bounds_;

  // The layer bounds since content bounds were last set.
  gfx::Rect last_layer_bounds_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_EXTRA_SHADOW_H_
