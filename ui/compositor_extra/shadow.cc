// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor_extra/shadow.h"

#include "base/check.h"
#include "base/check_op.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer_nine_patch.h"
#include "ui/compositor/layer_not_drawn.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/shadow_util.h"

namespace ui {

namespace {

// Duration for opacity animation in milliseconds.
constexpr int kShadowAnimationDurationMs = 100;

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

bool IsValidRoundedCorners(const gfx::RoundedCornersF& radii) {
  return radii.upper_left() >= 0.0f && radii.upper_right() >= 0.0f &&
         radii.lower_right() >= 0.0f && radii.lower_left() >= 0.0f;
}

}  // namespace

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

Shadow::Shadow() : shadow_layer_owner_(this) {}

Shadow::~Shadow() = default;

void Shadow::Init(int elevation) {
  DCHECK_GE(elevation, 0);
  elevation_ = elevation;
  SetLayer(std::make_unique<ui::LayerNotDrawn>());
  layer()->SetName("Shadow Parent Container");
  RecreateShadowLayer();
}

void Shadow::SetContentBounds(const gfx::Rect& content_bounds) {
  // The layer's bounds should change with the content bounds accordingly. Need
  // to recalculate the layer bounds if the layer bounds were modified after the
  // content bounds were last set. When the window moves but doesn't change
  // size, this is a no-op. (The origin stays the same in this case.)
  if (content_bounds == content_bounds_ &&
      layer()->bounds() == last_layer_bounds_) {
    return;
  }

  content_bounds_ = content_bounds;
  UpdateShadowAppearance();
}

void Shadow::SetElevation(int elevation) {
  DCHECK_GE(elevation, 0);
  if (elevation_ == elevation) {
    return;
  }

  elevation_ = elevation;

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
        base::Milliseconds(kShadowAnimationDurationMs));
    fading_layer()->SetOpacity(0.f);
  }

  {
    // We don't care to observe this one.
    ui::ScopedLayerAnimationSettings settings(shadow_layer()->GetAnimator());
    settings.SetTransitionDuration(
        base::Milliseconds(kShadowAnimationDurationMs));
    shadow_layer()->SetOpacity(1.f);
  }
}

void Shadow::SetRoundedCorners(const gfx::RoundedCornersF& radii) {
  CHECK(IsValidRoundedCorners(radii));
  if (rounded_corners_ == radii) {
    return;
  }

  rounded_corners_ = radii;
  UpdateShadowAppearance();
}

void Shadow::SetStyle(Style style) {
  if (style_ == style)
    return;

  style_ = style;
  UpdateShadowAppearance();
}

void Shadow::SetColorMap(const ElevationToColorsMap& color_map) {
  color_map_ = color_map;
  UpdateShadowAppearance();
}

void Shadow::OnImplicitAnimationsCompleted() {
  std::unique_ptr<ui::Layer> to_be_deleted = fading_layer_owner_.ReleaseLayer();
  // The size needed for layer() may be smaller now that |fading_layer()| is
  // removed.
  UpdateShadowAppearance();
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
  owner_shadow_->details_ = std::nullopt;
  owner_shadow_->UpdateShadowAppearance();
  return result;
}

// -----------------------------------------------------------------------------
// Shadow:

void Shadow::RecreateShadowLayer() {
  shadow_layer_owner_.Reset(std::make_unique<ui::LayerNinePatch>());
  shadow_layer()->SetName("Shadow");
  shadow_layer()->SetVisible(true);
  shadow_layer()->SetFillsBoundsOpaquely(false);
  layer()->Add(shadow_layer());

  details_ = std::nullopt;
  UpdateShadowAppearance();
}

void Shadow::UpdateShadowAppearance() {
  if (content_bounds_.IsEmpty())
    return;

  const int smaller_dimension =
      std::min(content_bounds_.width(), content_bounds_.height());

  // Corner radii cannot exceed half of the smaller dimension of the content
  // bounds. Clamp each corner radius to avoid invalid ninebox geometry.
  const float max_radius = std::floor(smaller_dimension / 2.0f);
  const gfx::RoundedCornersF size_adjusted_rounded_corners(
      std::min(rounded_corners_.upper_left(), max_radius),
      std::min(rounded_corners_.upper_right(), max_radius),
      std::min(rounded_corners_.lower_right(), max_radius),
      std::min(rounded_corners_.lower_left(), max_radius));

  // The ninebox assumption breaks down when the window is too small for the
  // desired elevation. The height/width of |blur_region| will be 4 * elevation
  // (see ShadowDetails::Get), so cap elevation at the most we can handle.
  const bool is_pill_shaped =
      (max_radius == size_adjusted_rounded_corners.upper_left() ||
       max_radius == size_adjusted_rounded_corners.upper_right() ||
       max_radius == size_adjusted_rounded_corners.lower_right() ||
       max_radius == size_adjusted_rounded_corners.lower_left());
  const int max_safe_elevation =
      is_pill_shaped
          ? smaller_dimension / 4
          : (smaller_dimension -
             2 * std::max({size_adjusted_rounded_corners.upper_left(),
                           size_adjusted_rounded_corners.upper_right(),
                           size_adjusted_rounded_corners.lower_right(),
                           size_adjusted_rounded_corners.lower_left()})) /
                4;
  const int size_adjusted_elevation = std::min(max_safe_elevation, elevation_);
  CHECK_GE(size_adjusted_elevation, 0);

  auto iter = color_map_.find(elevation_);
  const gfx::ShadowValues values = MakeShadowValues(
      size_adjusted_elevation, style_,
      iter != color_map_.end() ? std::make_optional(iter->second)
                               : std::nullopt,
      is_pill_shaped);
  const auto& details =
      gfx::ShadowDetails::Get(size_adjusted_rounded_corners, values);

  const gfx::Insets aperture_insets =
      gfx::ShadowDetails::GetNineboxApertureInsets(
          details.values, size_adjusted_rounded_corners);

  // Update |shadow_layer()| if details changed and it has been updated in
  // the past (|details_| is set), or elevation is non-zero.
  if (details != details_ && (details_ || size_adjusted_elevation)) {
    shadow_layer()->UpdateNinePatchLayerImage(details.nine_patch_image);
    // The ninebox grid is defined in terms of the image size. The shadow blurs
    // in both inward and outward directions from the edge of the contents (and
    // rounded corners if any), so the aperture goes further inside the image
    // than the shadow margins (which represent exterior blur).
    gfx::Rect aperture(details.nine_patch_image.size());
    aperture.Inset(aperture_insets);
    shadow_layer()->UpdateNinePatchLayerAperture(aperture);
    details_ = details;
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

  last_layer_bounds_ = layer()->bounds();

  shadow_layer()->SetBounds(shadow_layer_bounds);

  // Occlude the region inside the bounding box. Occlusion uses shadow layer
  // space. See nine_patch_layer.h for more context on what's going on here.
  gfx::Rect occlusion_bounds(shadow_layer_bounds.size());
  gfx::Insets corner_insets = gfx::ShadowDetails::GetInsetsForRoundedCorners(
      size_adjusted_rounded_corners);
  occlusion_bounds.Inset(-margins + corner_insets);
  shadow_layer()->UpdateNinePatchOcclusion(occlusion_bounds);

  // The border is the same inset as the aperture.
  shadow_layer()->UpdateNinePatchLayerBorder(
      gfx::Rect(aperture_insets.left(), aperture_insets.top(),
                aperture_insets.width(), aperture_insets.height()));
}

}  // namespace ui
