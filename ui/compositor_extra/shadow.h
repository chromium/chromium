// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_EXTRA_SHADOW_H_
#define UI_COMPOSITOR_EXTRA_SHADOW_H_

#include <memory>

#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
struct ShadowDetails;
}  // namespace gfx

namespace ui {
class Layer;

// Simple class that draws a drop shadow around content at given bounds.
class Shadow : public ui::ImplicitAnimationObserver, public ui::LayerOwner {
 public:
  Shadow();
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

  // Moves and resizes the shadow layer to frame |content_bounds|.
  // This should be used to adjust the shadow's size and position (rather than
  // applying transformations to the `layer()` of this Shadow).
  void SetContentBounds(const gfx::Rect& content_bounds);

  // Sets the shadow's appearance, animating opacity as necessary.
  void SetElevation(int elevation);

  // Sets the radius for the rounded corners to take into account when
  // adjusting the shadow layer to frame |content_bounds|. 0 or greater.
  void SetRoundedCornerRadius(int rounded_corner_radius);

  const gfx::ShadowDetails* details_for_testing() const { return details_; }

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override;

 private:
  // A shadow layer owner that correctly updates the nine patch layer details
  // when it gets recreated.
  class ShadowLayerOwner : public ui::LayerOwner {
   public:
    explicit ShadowLayerOwner(Shadow* owner,
                              std::unique_ptr<Layer> layer = nullptr);
    ~ShadowLayerOwner() override;

    // ui::LayerOwner:
    std::unique_ptr<Layer> RecreateLayer() override;

   private:
    Shadow* const owner_shadow_;

    DISALLOW_COPY_AND_ASSIGN(ShadowLayerOwner);
  };

  // Updates the shadow layer and its image to reflect |desired_elevation_|.
  void RecreateShadowLayer();

  // Updates the shadow layer bounds based on the inteior inset and the current
  // |content_bounds_|.
  void UpdateLayerBounds();

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
  const gfx::ShadowDetails* details_ = nullptr;

  // The owner of the actual shadow layer corresponding to a cc::NinePatchLayer.
  ShadowLayerOwner shadow_layer_owner_;

  // When the elevation changes, the old shadow cross-fades with the new one.
  // When non-null, this owns an old |shadow_layer()| that's being animated out.
  ui::LayerOwner fading_layer_owner_;

  // Bounds of the content that the shadow encloses.
  gfx::Rect content_bounds_;

  DISALLOW_COPY_AND_ASSIGN(Shadow);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_EXTRA_SHADOW_H_
