// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor_extra/shadow.h"

#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/shadow_util.h"

namespace ui {

namespace {

// Duration for opacity animation in milliseconds.
constexpr int kShadowAnimationDurationMs = 100;

}  // namespace

Shadow::Shadow() : shadow_layer_owner_(this) {}

Shadow::~Shadow() = default;

void Shadow::Init(int elevation) {
  DCHECK_GE(elevation, 0);
  desired_elevation_ = elevation;
  SetLayer(std::make_unique<ui::Layer>(ui::LAYER_NOT_DRAWN));
  layer()->set_name("Shadow Parent Container");
  RecreateShadowLayer();
}

void Shadow::SetContentBounds(const gfx::Rect& content_bounds) {
  // When the window moves but doesn't change size, this is a no-op. (The
  // origin stays the same in this case.)
  if (content_bounds == content_bounds_)
    return;

  content_bounds_ = content_bounds;
  UpdateLayerBounds();
}

void Shadow::SetElevation(int elevation) {
  DCHECK_GE(elevation, 0);
  if (desired_elevation_ == elevation)
    return;

  desired_elevation_ = elevation;

  // Stop waiting for any as yet unfinished implicit animations.
  StopObservingImplicitAnimations();

  // The old shadow layer is the new fading out layer.
  DCHECK(shadow_layer());
  fading_layer_owner_.Reset(shadow_layer_owner_.ReleaseLayer());
  RecreateShadowLayer();
  shadow_layer()->SetOpacity(0.f);

  {
    // Observe the fade out animation so we can clean up the layer when done.
    ui::ScopedLayerAnimationSettings settings(fading_layer()->GetAnimator());
    settings.AddObserver(this);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kShadowAnimationDurationMs));
    fading_layer()->SetOpacity(0.f);
  }

  {
    // We don't care to observe this one.
    ui::ScopedLayerAnimationSettings settings(shadow_layer()->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kShadowAnimationDurationMs));
    shadow_layer()->SetOpacity(1.f);
  }
}

void Shadow::SetRoundedCornerRadius(int rounded_corner_radius) {
  DCHECK_GE(rounded_corner_radius, 0);
  if (rounded_corner_radius_ == rounded_corner_radius)
    return;

  rounded_corner_radius_ = rounded_corner_radius;
  UpdateLayerBounds();
}

void Shadow::OnImplicitAnimationsCompleted() {
  std::unique_ptr<ui::Layer> to_be_deleted = fading_layer_owner_.ReleaseLayer();
  // The size needed for layer() may be smaller now that |fading_layer()| is
  // removed.
  UpdateLayerBounds();
}

// -----------------------------------------------------------------------------
// Shadow::ShadowLayerOwner:

Shadow::ShadowLayerOwner::ShadowLayerOwner(Shadow* owner,
                                           std::unique_ptr<Layer> layer)
    : LayerOwner(std::move(layer)), owner_shadow_(owner) {}

Shadow::ShadowLayerOwner::~ShadowLayerOwner() = default;

std::unique_ptr<Layer> Shadow::ShadowLayerOwner::RecreateLayer() {
  auto result = ui::LayerOwner::RecreateLayer();
  // Now update the newly recreated shadow layer with the correct nine patch
  // image details.
  owner_shadow_->details_ = nullptr;
  owner_shadow_->UpdateLayerBounds();
  return result;
}

// -----------------------------------------------------------------------------
// Shadow:

void Shadow::RecreateShadowLayer() {
  shadow_layer_owner_.Reset(std::make_unique<ui::Layer>(ui::LAYER_NINE_PATCH));
  shadow_layer()->set_name("Shadow");
  shadow_layer()->SetVisible(true);
  shadow_layer()->SetFillsBoundsOpaquely(false);
  layer()->Add(shadow_layer());

  UpdateLayerBounds();
}

void Shadow::UpdateLayerBounds() {
  if (content_bounds_.IsEmpty())
    return;

  // The ninebox assumption breaks down when the window is too small for the
  // desired elevation. The height/width of |blur_region| will be 4 * elevation
  // (see ShadowDetails::Get), so cap elevation at the most we can handle.
  const int smaller_dimension =
      std::min(content_bounds_.width(), content_bounds_.height());
  const int size_adjusted_elevation =
      std::min((smaller_dimension - 2 * rounded_corner_radius_) / 4,
               static_cast<int>(desired_elevation_));
  const auto& details =
      gfx::ShadowDetails::Get(size_adjusted_elevation, rounded_corner_radius_);
  gfx::Insets blur_region = gfx::ShadowValue::GetBlurRegion(details.values) +
                            gfx::Insets(rounded_corner_radius_);
  // Update |shadow_layer()| if details changed and it has been updated in
  // the past (|details_| is set), or elevation is non-zero.
  if ((&details != details_) && (details_ || size_adjusted_elevation)) {
    shadow_layer()->UpdateNinePatchLayerImage(details.ninebox_image);
    // The ninebox grid is defined in terms of the image size. The shadow blurs
    // in both inward and outward directions from the edge of the contents, so
    // the aperture goes further inside the image than the shadow margins (which
    // represent exterior blur).
    gfx::Rect aperture(details.ninebox_image.size());
    aperture.Inset(blur_region);
    shadow_layer()->UpdateNinePatchLayerAperture(aperture);
    details_ = &details;
  }

  // Shadow margins are negative, so this expands outwards from
  // |content_bounds_|.
  const gfx::Insets margins = gfx::ShadowValue::GetMargin(details.values);
  gfx::Rect new_layer_bounds = content_bounds_;
  new_layer_bounds.Inset(margins);
  gfx::Rect shadow_layer_bounds(new_layer_bounds.size());

  // When there's an old shadow fading out, the bounds of layer() have to be
  // big enough to encompass both shadows.
  if (fading_layer()) {
    const gfx::Rect old_layer_bounds = layer()->bounds();
    gfx::Rect combined_layer_bounds = old_layer_bounds;
    combined_layer_bounds.Union(new_layer_bounds);
    layer()->SetBounds(combined_layer_bounds);

    // If this is reached via SetContentBounds, we might hypothetically need
    // to change the size of the fading layer, but the fade is so fast it's
    // not really an issue.
    gfx::Rect fading_layer_bounds(fading_layer()->bounds());
    fading_layer_bounds.Offset(old_layer_bounds.origin() -
                               combined_layer_bounds.origin());
    fading_layer()->SetBounds(fading_layer_bounds);

    shadow_layer_bounds.Offset(new_layer_bounds.origin() -
                               combined_layer_bounds.origin());
  } else {
    layer()->SetBounds(new_layer_bounds);
  }

  shadow_layer()->SetBounds(shadow_layer_bounds);

  // Occlude the region inside the bounding box. Occlusion uses shadow layer
  // space. See nine_patch_layer.h for more context on what's going on here.
  gfx::Rect occlusion_bounds(shadow_layer_bounds.size());
  occlusion_bounds.Inset(-margins + gfx::Insets(rounded_corner_radius_));
  shadow_layer()->UpdateNinePatchOcclusion(occlusion_bounds);

  // The border is the same inset as the aperture.
  shadow_layer()->UpdateNinePatchLayerBorder(
      gfx::Rect(blur_region.left(), blur_region.top(), blur_region.width(),
                blur_region.height()));
}

}  // namespace ui
