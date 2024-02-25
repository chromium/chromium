// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_EXTRA_SHADOW_H_
#define UI_COMPOSITOR_EXTRA_SHADOW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/shadow_util.h"

namespace ui {
class Layer;

// Simple class that draws a drop shadow around content at given bounds.
class Shadow : public ui::ImplicitAnimationObserver, public ui::LayerOwner {
 public:
  // Mapping from elevation to key and ambient shadow colors. The first color is
  // the key shadow color and the second is the ambient shadow color.
  using ElevationToColorsMap = base::flat_map<int, std::pair<SkColor, SkColor>>;

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
  ui::Layer* shadow_layer() { return shadow_layer_owner_.layer(); }

  ui::Layer* fading_layer() { return fading_layer_owner_.layer(); }

  const gfx::Rect& content_bounds() const { return content_bounds_; }
  int desired_elevation() const { return desired_elevation_; }
  const ElevationToColorsMap& color_map() const { return color_map_; }

  // Moves and resizes the shadow layer to frame |content_bounds|.
  // This should be used to adjust the shadow's size and position (rather than
  // applying transformations to the `layer()` of this Shadow).
  void SetContentBounds(const gfx::Rect& content_bounds);

  // Sets the shadow's appearance, animating opacity as necessary.
  void SetElevation(int elevation);

  // Sets the radius for the rounded corners to take into account when
  // adjusting the shadow layer to frame |content_bounds|. 0 or greater.
  void SetRoundedCornerRadius(int rounded_corner_radius);

  // Set shadow style.
  void SetShadowStyle(gfx::ShadowStyle style);

  // Set customized key and ambient shadows color map for certain elevations.
  void SetElevationToColorsMap(const ElevationToColorsMap& color_map);

  const gfx::ShadowDetails* details_for_testing() const { return details_; }
  int rounded_corner_radius_for_testing() const {
    return rounded_corner_radius_;
  }

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override;

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
  int desired_elevation_ = 0;

  // Rounded corners are drawn on top of the window's content layer,
  // we need to exclude them from the occlusion area.
  int rounded_corner_radius_ = 2;

  // The details of the shadow image that's currently set on |shadow_layer()|.
  // This will be null until a positive elevation has been set. Once set, it
  // will always point to a global ShadowDetails instance that is guaranteed
  // to outlive the Shadow instance. See ui/gfx/shadow_util.h for how these
  // ShadowDetails instances are created.
  raw_ptr<const gfx::ShadowDetails, LeakedDanglingUntriaged> details_ = nullptr;

  // The style of shadow. Use MD style by default.
  gfx::ShadowStyle style_ = gfx::ShadowStyle::kMaterialDesign;

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
