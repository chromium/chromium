// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_wp_color_management_surface.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_wp_color_manager.h"

namespace ui {

WaylandWpColorManagementSurface::WaylandWpColorManagementSurface(
    WaylandSurface* wayland_surface,
    WaylandConnection* connection,
    wl::Object<wp_color_management_surface_v1> management_surface,
    wl::Object<wp_color_management_surface_feedback_v1> feedback_surface)
    : wayland_surface_(wayland_surface),
      connection_(connection),
      management_surface_(std::move(management_surface)),
      feedback_surface_(std::move(feedback_surface)) {
  DCHECK(wayland_surface_);
  DCHECK(connection_);
  DCHECK(management_surface_);
  if (feedback_surface_) {
    static constexpr wp_color_management_surface_feedback_v1_listener
        kListener = {.preferred_changed = &OnPreferredChanged};
    wp_color_management_surface_feedback_v1_add_listener(
        feedback_surface_.get(), &kListener, this);
  }
}

WaylandWpColorManagementSurface::~WaylandWpColorManagementSurface() = default;

void WaylandWpColorManagementSurface::SetColorSpace(
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata) {
  if (!color_space.IsValid()) {
    wp_color_management_surface_v1_unset_image_description(
        management_surface_.get());
    return;
  }

  auto* color_manager = connection_->wp_color_manager();
  DCHECK(color_manager);

  color_manager->GetImageDescription(
      color_space, hdr_metadata,
      base::BindOnce(&WaylandWpColorManagementSurface::OnSetColorSpace,
                     weak_factory_.GetWeakPtr()));
}

void WaylandWpColorManagementSurface::OnSetColorSpace(
    scoped_refptr<WaylandWpImageDescription> image_description) {
  if (!image_description) {
    LOG(ERROR) << "Failed to get image description for color space.";
    return;
  }

  auto* color_manager = connection_->wp_color_manager();
  wp_color_manager_v1_render_intent render_intent;
  if (color_manager->IsSupportedRenderIntent(
          WP_COLOR_MANAGER_V1_RENDER_INTENT_RELATIVE)) {
    render_intent = WP_COLOR_MANAGER_V1_RENDER_INTENT_RELATIVE;
  } else {
    // The protocol mandates that perceptual is always supported.
    CHECK(color_manager->IsSupportedRenderIntent(
        WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL));
    render_intent = WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL;
  }
  wp_color_management_surface_v1_set_image_description(
      management_surface_.get(), image_description->object(), render_intent);
}

// static
void WaylandWpColorManagementSurface::OnPreferredChanged(
    void* data,
    wp_color_management_surface_feedback_v1* feedback_surface,
    uint32_t identity) {
  // TODO(https://crbug.com/375959958): The image description is currently set
  // based on the output the surface resides on. We should be using this hint
  // from the compositor instead.
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace ui
