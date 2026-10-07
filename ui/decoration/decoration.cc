// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/decoration/decoration.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "ui/compositor/layer_nine_patch.h"
#include "ui/compositor/layer_not_drawn.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/decoration/shadow.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace ui {

namespace {

// Returns `rounded_corners` with each radius clamped to half of the smaller
// dimension of `bounds`, as larger radii would produce invalid ninebox
// geometry.
gfx::RoundedCornersF ClampRadiiToBounds(
    const gfx::Rect& bounds,
    const gfx::RoundedCornersF& rounded_corners) {
  const float max_radius =
      std::floor(std::min(bounds.width(), bounds.height()) / 2.0f);
  return gfx::RoundedCornersF(
      std::min(rounded_corners.upper_left(), max_radius),
      std::min(rounded_corners.upper_right(), max_radius),
      std::min(rounded_corners.lower_right(), max_radius),
      std::min(rounded_corners.lower_left(), max_radius));
}

// Returns the layer name for a decoration, e.g. "Decoration:Shadow" for a
// `debug_name` of "Shadow".
std::string MakeLayerName(std::string_view debug_name) {
  constexpr std::string_view kBaseName = "Decoration";
  return debug_name.empty() ? std::string(kBaseName)
                            : base::StrCat({kBaseName, "-", debug_name});
}

using Appearance = decoration::DecorationSource::Appearance;

// Returns the bounds of a decoration layer that frames `content_bounds` with
// `margins`. Decoration margins are negative, so this expands outwards.
gfx::Rect GetFramingBounds(const gfx::Rect& content_bounds,
                           const gfx::Insets& margins) {
  gfx::Rect bounds = content_bounds;
  bounds.Inset(margins);
  return bounds;
}

// Converts the content-relative `occlusion_rect` into the ninebox space of a
// decoration layer with `margins`, where the content sits inset by `-margins`.
// See nine_patch_layer.h.
gfx::Rect ToNineboxOcclusion(const gfx::Rect& occlusion_rect,
                             const gfx::Insets& margins) {
  if (occlusion_rect.IsEmpty()) {
    return gfx::Rect();
  }
  gfx::Rect occlusion = occlusion_rect;
  occlusion.Offset(-margins.left(), -margins.top());
  return occlusion;
}

}  // namespace

// static
std::unique_ptr<Decoration> Decoration::Create(
    std::unique_ptr<decoration::DecorationSource> source,
    std::string_view debug_name) {
  return std::make_unique<Decoration>(std::move(source), debug_name);
}

// static
std::unique_ptr<Decoration> Decoration::CreateShadow(
    int elevation,
    ui::decoration::Shadow::Style style,
    ui::decoration::Shadow::ElevationToColorsMap color_map) {
  auto decoration = Create(std::make_unique<ui::decoration::Shadow>(
                               elevation, style, std::move(color_map)),
                           /*debug_name=*/"Shadow");
  // Historically, shadows frame content with slightly rounded corners unless
  // told otherwise.
  decoration->SetRoundedCorners(gfx::RoundedCornersF(2.f));
  return decoration;
}

Decoration::Decoration(std::unique_ptr<decoration::DecorationSource> source,
                       std::string_view debug_name)
    : source_(std::move(source)),
      name_(MakeLayerName(debug_name)),
      decoration_layer_owner_(this) {
  CHECK(source_);
  source_->set_details_changed_callback(base::BindRepeating(
      &Decoration::UpdateAppearance, base::Unretained(this)));

  SetLayer(std::make_unique<ui::LayerNotDrawn>());
  layer()->SetName(base::StrCat({name_, ":Container"}));
  RecreateDecorationLayer();
}

Decoration::~Decoration() = default;

void Decoration::SetContentBounds(const gfx::Rect& content_bounds) {
  // The layer's bounds should change with the content bounds accordingly. Need
  // to recalculate the layer bounds if the layer bounds were modified after the
  // content bounds were last set. When the window moves but doesn't change
  // size, this is a no-op. (The origin stays the same in this case.)
  if (content_bounds == content_bounds_ &&
      layer()->bounds() == last_layer_bounds_) {
    return;
  }

  content_bounds_ = content_bounds;
  UpdateAppearance();
}

void Decoration::SetRoundedCorners(
    const gfx::RoundedCornersF& rounded_corners) {
  if (rounded_corners == rounded_corners_) {
    return;
  }

  rounded_corners_ = rounded_corners;
  UpdateAppearance();
}

void Decoration::OnImplicitAnimationsCompleted() {
  std::unique_ptr<ui::Layer> to_be_deleted = fading_layer_owner_.ReleaseLayer();
  fading_appearance_ = std::nullopt;
  // The size needed for layer() may be smaller now that |fading_layer()| is
  // removed.
  UpdateAppearance();
}

// -----------------------------------------------------------------------------
// Decoration::DecorationLayerOwner:

Decoration::DecorationLayerOwner::DecorationLayerOwner(
    Decoration* owner,
    std::unique_ptr<ui::Layer> layer)
    : LayerOwner(std::move(layer)), owner_decoration_(owner) {}

Decoration::DecorationLayerOwner::~DecorationLayerOwner() = default;

std::unique_ptr<ui::Layer> Decoration::DecorationLayerOwner::RecreateLayer() {
  auto result = ui::LayerOwner::RecreateLayer();
  // Force the newly recreated layer to be re-populated.
  owner_decoration_->active_appearance_ = std::nullopt;
  owner_decoration_->UpdateAppearance();
  return result;
}

// -----------------------------------------------------------------------------
// Decoration:

void Decoration::RecreateDecorationLayer() {
  decoration_layer_owner_.Reset(std::make_unique<ui::LayerNinePatch>());
  decoration_layer()->SetName(name_);
  decoration_layer()->SetFillsBoundsOpaquely(false);
  layer()->Add(decoration_layer());

  active_appearance_ = std::nullopt;
  UpdateAppearance();
}

void Decoration::UpdateAppearance(
    std::optional<base::TimeDelta> cross_fade_duration) {
  CHECK(layer());

  if (cross_fade_duration.has_value()) {
    CrossFadeToNewAppearance(*cross_fade_duration);
  } else {
    UpdateAppearanceImmediately();
  }
}

void Decoration::CrossFadeToNewAppearance(base::TimeDelta duration) {
  // Stop waiting for any as yet unfinished implicit animations.
  StopObservingImplicitAnimations();

  // The old decoration layer is the new fading out layer.
  DCHECK(decoration_layer());
  fading_layer_owner_.Reset(decoration_layer_owner_.ReleaseLayer());
  fading_appearance_ = active_appearance_;
  RecreateDecorationLayer();
  decoration_layer()->SetOpacity(0.f);

  {
    // Observe the fade out animation so we can clean up the layer when done.
    ui::ScopedLayerAnimationSettings settings(fading_layer()->GetAnimator());
    settings.AddObserver(this);
    settings.SetTransitionDuration(duration);
    fading_layer()->SetOpacity(0.f);
  }

  {
    // We don't care to observe this one.
    ui::ScopedLayerAnimationSettings settings(
        decoration_layer()->GetAnimator());
    settings.SetTransitionDuration(duration);
    decoration_layer()->SetOpacity(1.f);
  }
}

void Decoration::UpdateAppearanceImmediately() {
  const std::optional<decoration::DecorationSource::Details> details =
      content_bounds_.IsEmpty()
          ? std::nullopt
          : source_->GetDetails(
                content_bounds_,
                ClampRadiiToBounds(content_bounds_, rounded_corners_));

  // Compare only the appearance, so geometry or occlusion changes don't
  // re-upload the image.
  const std::optional<Appearance> appearance =
      details.has_value() ? std::make_optional(details->appearance)
                          : std::nullopt;

  // With no appearance there is nothing to draw, yet the nine-patch layer still
  // holds the image it was last given and would keep drawing it. Hiding the
  // layer keeps that image and ninebox geometry in place, so an appearance that
  // comes back unchanged only needs the layer shown again.
  decoration_layer()->SetVisible(appearance.has_value());

  if (appearance.has_value() && active_appearance_ != appearance) {
    decoration_layer()->UpdateNinePatchLayerImage(appearance->nine_patch_image);
    // The ninebox grid is defined in terms of the image size.
    gfx::Rect aperture(appearance->nine_patch_image.size());
    aperture.Inset(appearance->aperture_insets);
    decoration_layer()->UpdateNinePatchLayerAperture(aperture);

    // The border is the same inset as the aperture.
    decoration_layer()->UpdateNinePatchLayerBorder(gfx::Rect(
        appearance->aperture_insets.left(), appearance->aperture_insets.top(),
        appearance->aperture_insets.width(),
        appearance->aperture_insets.height()));

    active_appearance_ = appearance;
  }

  const gfx::Insets margins = appearance.value_or(Appearance()).margins;
  const gfx::Rect decoration_bounds =
      GetFramingBounds(content_bounds_, margins);

  // While cross-fading, also re-frame the old (fading) decoration around the
  // current content using the margins it was drawn with, so it doesn't stay
  // at a stale size if the content bounds change mid-fade.
  const gfx::Insets fading_margins =
      fading_appearance_.value_or(Appearance()).margins;
  const gfx::Rect fading_bounds =
      fading_layer() && !content_bounds_.IsEmpty()
          ? GetFramingBounds(content_bounds_, fading_margins)
          : gfx::Rect();

  // layer() must be big enough to encompass both decorations.
  gfx::Rect layer_bounds = decoration_bounds;
  layer_bounds.Union(fading_bounds);
  layer()->SetBounds(layer_bounds);
  last_layer_bounds_ = layer_bounds;

  // Child layers are positioned relative to layer().
  const gfx::Vector2d layer_offset = layer_bounds.OffsetFromOrigin();
  const gfx::Rect occlusion_rect =
      details.has_value() ? details->occlusion_rect : gfx::Rect();

  decoration_layer()->SetBounds(decoration_bounds - layer_offset);
  decoration_layer()->UpdateNinePatchOcclusion(
      ToNineboxOcclusion(occlusion_rect, margins));

  if (fading_layer()) {
    fading_layer()->SetBounds(fading_bounds - layer_offset);
    fading_layer()->UpdateNinePatchOcclusion(
        ToNineboxOcclusion(occlusion_rect, fading_margins));
  }
}

}  // namespace ui
