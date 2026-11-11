// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_wp_color_management_surface.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_wp_color_manager.h"

namespace ui {

WaylandWpColorManagementSurface::WaylandWpColorManagementSurface(
    WaylandSurface* wayland_surface,
    WaylandConnection* connection,
    wl::Object<wp_color_management_surface_v1> management_surface,
    wl::Object<wp_color_management_surface_feedback_v1> feedback_surface)
    : wayland_surface_(wayland_surface),
      connection_(connection),
      color_manager_(connection->wp_color_manager()),
      management_surface_(std::move(management_surface)),
      feedback_surface_(std::move(feedback_surface)) {
  CHECK(wayland_surface_, base::NotFatalUntil::M163);
  CHECK(connection_, base::NotFatalUntil::M163);
  CHECK(color_manager_);
  CHECK(management_surface_, base::NotFatalUntil::M163);
  if (feedback_surface_) {
    static constexpr wp_color_management_surface_feedback_v1_listener
        kListener = {.preferred_changed = &OnPreferredChanged};
    wp_color_management_surface_feedback_v1_add_listener(
        feedback_surface_.get(), &kListener, this);
    // The protocol does not guarantee that compositors send `preferred_changed`
    // immediately after the feedback object is bound. KWin and wlroots happen
    // to do so, but Mutter only sends the event on subsequent changes. Fetch
    // the initial preferred image description explicitly so that Chromium has
    // a description to work with regardless of compositor behavior.
    FetchPreferredImageDescription();
  }
  color_manager_observation_.Observe(color_manager_);
}

WaylandWpColorManagementSurface::~WaylandWpColorManagementSurface() = default;

void WaylandWpColorManagementSurface::SetColorSpace(
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata) {
  color_space_ = color_space;
  hdr_metadata_ = hdr_metadata;
  const uint64_t sequence = ++color_space_sequence_;
  if (!color_space.IsValid()) {
    wp_color_management_surface_v1_unset_image_description(
        management_surface_.get());
    return;
  }

  if (auto image_description = color_manager_->GetImageDescription(
          GetEffectiveColorSpace(color_space), hdr_metadata,
          base::BindOnce(&WaylandWpColorManagementSurface::OnSetColorSpace,
                         weak_factory_.GetWeakPtr(), sequence))) {
    OnSetColorSpace(sequence, std::move(*image_description));
  }
}

bool WaylandWpColorManagementSurface::EnsureImageDescription(
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    base::OnceClosure on_ready) {
  if (!color_space.IsValid()) {
    return true;
  }
  ImageDescriptionKey key = {GetEffectiveColorSpace(color_space), hdr_metadata};
  if (failed_image_description_ == key) {
    return true;
  }
  auto image_description = color_manager_->GetImageDescription(
      key.first, key.second,
      base::BindOnce(
          &WaylandWpColorManagementSurface::OnImageDescriptionEnsured,
          weak_factory_.GetWeakPtr(), key, std::move(on_ready)));
  if (!image_description) {
    return false;
  }
  if (!*image_description) {
    failed_image_description_ = std::move(key);
  }
  return true;
}

gfx::ColorSpace WaylandWpColorManagementSurface::GetEffectiveColorSpace(
    const gfx::ColorSpace& color_space) const {
  if (srgb_is_gamma22_ &&
      color_space.GetTransferID() == gfx::ColorSpace::TransferID::SRGB &&
      (color_manager_->IsSupportedTransferFunction(
           WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22) ||
       color_manager_->IsSupportedFeature(
           WP_COLOR_MANAGER_V1_FEATURE_SET_TF_POWER))) {
    return color_space.GetWithTransferFunction(
        gfx::ColorSpace::TransferID::GAMMA22);
  }
  return color_space;
}

// static
void WaylandWpColorManagementSurface::OnImageDescriptionEnsured(
    base::WeakPtr<WaylandWpColorManagementSurface> self,
    ImageDescriptionKey key,
    base::OnceClosure on_ready,
    scoped_refptr<WaylandWpImageDescription> image_description) {
  // `on_ready` must run even if this surface is gone, so the caller doesn't
  // wait forever.
  if (self && !image_description) {
    // Don't hold frames for a description that can't be created.
    self->failed_image_description_ = std::move(key);
  }
  std::move(on_ready).Run();
}

void WaylandWpColorManagementSurface::OnSetColorSpace(
    uint64_t sequence,
    scoped_refptr<WaylandWpImageDescription> image_description) {
  if (sequence != color_space_sequence_) {
    return;
  }
  if (!image_description) {
    LOG(ERROR) << "Failed to get image description for color space.";
    return;
  }

  wp_color_manager_v1_render_intent render_intent;
  // The protocol mandates that perceptual is always supported.
  CHECK(color_manager_->IsSupportedRenderIntent(
      WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL));
  render_intent = WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL;
  wp_color_management_surface_v1_set_image_description(
      management_surface_.get(), image_description->object(), render_intent);
}

void WaylandWpColorManagementSurface::OnImageDescription(
    scoped_refptr<WaylandWpImageDescription> image_description) {
  bool srgb_is_gamma22 = false;
  if (!image_description) {
    LOG(ERROR) << "Failed to get image description.";
    display_color_spaces_.reset();
  } else {
    srgb_is_gamma22 = image_description->srgb_is_gamma22();
    auto* wayland_screen =
        connection_->wayland_output_manager()->wayland_screen();
    display_color_spaces_ = wayland_screen
                                ? image_description->AsDisplayColorSpaces(
                                      wayland_screen->GetHDRImageFormat())
                                : image_description->AsDisplayColorSpaces();
  }
  if (srgb_is_gamma22_ != srgb_is_gamma22) {
    srgb_is_gamma22_ = srgb_is_gamma22;
    if (color_space_.GetTransferID() == gfx::ColorSpace::TransferID::SRGB) {
      SetColorSpace(color_space_, hdr_metadata_);
    }
  }
  OnHdrEnabledChanged(color_manager_->hdr_enabled());
}

// static
void WaylandWpColorManagementSurface::OnPreferredChanged(
    void* data,
    wp_color_management_surface_feedback_v1* feedback_surface,
    uint32_t identity) {
  auto* self = static_cast<WaylandWpColorManagementSurface*>(data);
  CHECK(self);
  CHECK_EQ(feedback_surface, self->feedback_surface_.get());
  self->FetchPreferredImageDescription();
}

void WaylandWpColorManagementSurface::FetchPreferredImageDescription() {
  // Reset the previous image description before creating a new one.
  // Per the color management protocol specification:
  // "The client should stop using and destroy the image descriptions created
  // by earlier invocations of this request for the associated wl_surface."
  // This prevents "invalid object" errors when the compositor or client tries
  // to reference the old object ID after it has been destroyed.
  image_description_.reset();

  auto image_description_object = wl::Object<wp_image_description_v1>(
      wp_color_management_surface_feedback_v1_get_preferred(
          feedback_surface_.get()));

  image_description_ = base::MakeRefCounted<WaylandWpImageDescription>(
      std::move(image_description_object), connection_, std::nullopt,
      base::BindOnce(&WaylandWpColorManagementSurface::OnImageDescription,
                     weak_factory_.GetWeakPtr()));
}

void WaylandWpColorManagementSurface::OnHdrEnabledChanged(bool hdr_enabled) {
  WaylandWindow* root = wayland_surface_->root_window();
  if (!root) {
    return;
  }

  auto display_color_spaces = hdr_enabled ? display_color_spaces_ : nullptr;
  root->OnDisplayColorSpacesChanged(
      display_color_spaces
          ? display_color_spaces
          : base::MakeRefCounted<gfx::DisplayColorSpacesRef>());
}

void WaylandWpColorManagementSurface::OnColorManagerDestroyed() {
  NOTREACHED();
}

void WaylandWpColorManagementSurface::OnColorManagerReady() {
  FetchPreferredImageDescription();
}

}  // namespace ui
